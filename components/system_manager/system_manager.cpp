#include "system_manager.h"
#include "esp_log.h"
#include "settings_storage.h"

static const char *TAG = "system manager";
static constexpr uint32_t kControlQueueSendTimeoutMs = 10;

SystemManager::SystemManager(Queues* queues)
    : system_in_queue_(queues->system_in_queue),
      network_in_queue_(queues->network_in_queue) {
}

void SystemManager::init() {
    if (task_system_manager_ != nullptr) {
        return;
    }

    if (!loadDeviceSettings(&settings_)) {
        ESP_LOGI(TAG, "No stored settings loaded, using defaults");
    }

    if (settings_.startup_direction != 1 &&
        settings_.startup_direction != 2) {
        settings_.startup_direction = 1;
    }

    selected_direction_ = settings_.startup_direction;
    BootMode boot_mode = decideBootMode(settings_);

    button_service_init();

    button_cfg_t button_cfg = {
        .gpio_num = kMainButtonPin_,
        .hasPullup = kMainButtonHasPullup_,
        .debounce = kButtonDebounceMs_,
        .long_press_dur = kButtonLongPressMs_,
        .btn_callback = handleButtonCallback,
        .user_data = this,
    };
    button_init(&button_cfg, &main_button_);
    matrix_.init();

    ESP_LOGI(TAG, "State -> %d", static_cast<int>(system_state_));
    ESP_LOGI(TAG, "Boot mode -> %d", static_cast<int>(boot_mode));
    renderDisplay();

    xTaskCreatePinnedToCore(       // UI Task
        systemTask,                // Function to implement the task
        "systemTask",              // Name of the task
        8192,                      // Stack size in words
        this,                      // Task input parameter
        2,                         // Priority of the task
        &task_system_manager_,     // Task handle.
        kTaskCore_                 // Core where the task should run
    );

    NetworkCommand command {};

    if (boot_mode == BootMode::SETUP) {
        command.type = NetworkCommandType::START_SETUP_MODE;
    }
    else {
        command.type = NetworkCommandType::START_NORMAL_MODE;
        command.settings = settings_;
    }

    if (xQueueSend(
        network_in_queue_,
        &command,
        pdMS_TO_TICKS(kControlQueueSendTimeoutMs)
    ) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to queue network command: %d", static_cast<int>(command.type));
    }
}

void SystemManager::handleButtonCallback(button_event_t event, uint8_t gpio_num, void* user_data) {
    (void)gpio_num;

    auto* self = static_cast<SystemManager*>(user_data);
    if (self == nullptr || self->system_in_queue_ == nullptr) {
        return;
    }

    SystemEvent system_event {};
    system_event.type = SystemEventType::INPUT_EVENT;

    switch (event) {
        case BTN_SHORT_PRESS:
            system_event.input_event = SystemInputEvent::TOGGLE_DIRECTION;
            break;

        case BTN_LONG_PRESS:
            system_event.input_event = SystemInputEvent::FORCE_SETUP;
            break;

        default:
            return;
    }

    (void)xQueueSend(self->system_in_queue_, &system_event, 0);
}

void SystemManager::setState(SystemState new_state) {
    if (system_state_ == new_state) {
        return;
    }

    system_state_ = new_state;
    ESP_LOGI(TAG, "State -> %d", static_cast<int>(system_state_));
}

void SystemManager::renderDisplay() {
    DirectionDepartures active_departures {};

    if ((system_state_ == SystemState::DEPARTURES ||
         system_state_ == SystemState::NO_DEPARTURES) &&
        selected_direction_ >= 1 &&
        selected_direction_ <= kMaxDepartureDirections) {
        active_departures = network_state_.departures.directions[selected_direction_ - 1];
    }

    switch (system_state_) {
        case SystemState::BOOT:
            matrix_.bootAnimation(animation_frame_);
            break;

        case SystemState::CONNECTING:
            matrix_.setColor(0, 0, kPixelBrightness_);
            matrix_.connectionAnimation(animation_frame_);
            break;

        case SystemState::CONNECTED:
            matrix_.setColor(0, kPixelBrightness_, 0);
            matrix_.displayIcon(MatrixIcon::OK);
            break;

        case SystemState::SETUP:
            matrix_.setColor(0, kPixelBrightness_, kPixelBrightness_);
            matrix_.displayIcon(MatrixIcon::HEART);
            break;

        case SystemState::DEPARTURES:
            matrix_.setColor(
                network_state_.stale_data ? kPixelBrightness_ : 0,
                kPixelBrightness_,
                0
            );

            if (active_departures.count == 0) {
                matrix_.displayIcon(MatrixIcon::QUESTION);
                break;
            }

            matrix_.displayDeparture(
                active_departures.departures[0].display,
                animation_frame_
            );
            break;

        case SystemState::NO_DEPARTURES:
            matrix_.setColor(kPixelBrightness_, kPixelBrightness_, 0);
            matrix_.displayIcon(MatrixIcon::QUESTION);
            break;

        case SystemState::API_ERROR:
            matrix_.setColor(kPixelBrightness_, 0, 0);
            matrix_.displayIcon(MatrixIcon::QUESTION);
            break;

        case SystemState::NETWORK_ERROR:
            matrix_.setColor(kPixelBrightness_, 0, 0);
            matrix_.displayIcon(MatrixIcon::CROSS);
            break;
    }
}

void SystemManager::systemTask(void* pvParameters) {
    auto* self = static_cast<SystemManager*>(pvParameters);

    while(true) {
        SystemEvent system_event {};

        if (xQueueReceive(
            self->system_in_queue_,
            &system_event,
            pdMS_TO_TICKS(self->kUpdateInterval_)
        ) == pdTRUE) {
            switch (system_event.type) {
                case SystemEventType::NETWORK_STATE: {
                    SystemState new_state = self->system_state_;
                    self->network_state_ = system_event.network_state;

                    switch (self->network_state_.phase) {
                        case NetworkPhase::CONNECTING:
                            new_state = SystemState::CONNECTING;
                            break;

                        case NetworkPhase::SETUP:
                            new_state = SystemState::SETUP;
                            break;

                        case NetworkPhase::ERROR:
                            new_state = SystemState::NETWORK_ERROR;
                            break;

                        case NetworkPhase::READY:
                            switch (self->network_state_.departure_state) {
                                case DepartureState::NONE:
                                    new_state = SystemState::CONNECTED;
                                    break;

                                case DepartureState::READY:
                                    if (totalDepartureCount(self->network_state_.departures) == 0) {
                                        new_state = SystemState::NO_DEPARTURES;
                                        break;
                                    }

                                    new_state = SystemState::DEPARTURES;
                                    break;

                                case DepartureState::API_ERROR:
                                    new_state = SystemState::API_ERROR;
                                    break;
                            }
                            break;
                    }

                    self->setState(new_state);
                    break;
                }

                case SystemEventType::INPUT_EVENT: {
                    if (system_event.input_event == SystemInputEvent::TOGGLE_DIRECTION) {
                        self->selected_direction_++;
                        if (self->selected_direction_ > kMaxDepartureDirections) {
                            self->selected_direction_ = 1;
                        }

                        ESP_LOGI(
                            TAG,
                            "Direction -> %u",
                            static_cast<unsigned>(self->selected_direction_)
                        );
                        break;
                    }

                    ESP_LOGI(TAG, "Button requested setup mode");

                    NetworkCommand setup_command {};
                    setup_command.type = NetworkCommandType::START_SETUP_MODE;

                    if (xQueueSend(
                        self->network_in_queue_,
                        &setup_command,
                        pdMS_TO_TICKS(kControlQueueSendTimeoutMs)
                    ) == pdTRUE) {
                        self->setState(SystemState::SETUP);
                    }
                    else {
                        ESP_LOGW(
                            TAG,
                            "Failed to queue network command: %d",
                            static_cast<int>(setup_command.type)
                        );
                    }
                    break;
                }

                case SystemEventType::SETUP_CONFIG: {
                    if (!isValidSetupConfig(system_event.setup_config)) {
                        ESP_LOGW(TAG, "Rejected invalid setup config");
                        break;
                    }

                    DeviceSettings updated_settings = self->settings_;
                    applySetupConfig(updated_settings, system_event.setup_config);

                    if (!saveDeviceSettings(updated_settings)) {
                        ESP_LOGW(TAG, "Failed to save setup config");
                        self->setState(SystemState::NETWORK_ERROR);
                        break;
                    }

                    self->settings_ = updated_settings;
                    self->selected_direction_ = self->settings_.startup_direction;

                    NetworkCommand normal_command {};
                    normal_command.type = NetworkCommandType::START_NORMAL_MODE;
                    normal_command.settings = self->settings_;

                    if (xQueueSend(
                        self->network_in_queue_,
                        &normal_command,
                        pdMS_TO_TICKS(kControlQueueSendTimeoutMs)
                    ) != pdTRUE) {
                        ESP_LOGW(
                            TAG,
                            "Failed to queue network command: %d",
                            static_cast<int>(normal_command.type)
                        );
                        self->setState(SystemState::NETWORK_ERROR);
                    }
                    break;
                }
            }
        }

        self->animation_frame_++;
        self->renderDisplay();
    }
}
