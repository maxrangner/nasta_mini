#include "system_manager.h"
#include "esp_log.h"
#include "settings_storage.h"

static const char *TAG = "system manager";
static constexpr uint32_t kControlQueueSendTimeoutMs = 10;

static uint8_t normalizeDirection(uint8_t direction) {
    if (direction < 1 || direction > kMaxDepartureDirections) {
        return 1;
    }

    return direction;
}

static DirectionDepartures getActiveDepartures(
    const NetworkState& network_state,
    SystemState system_state,
    uint8_t selected_direction
) {
    if (selected_direction < 1 ||
        selected_direction > kMaxDepartureDirections) {
        return {};
    }

    if (system_state != SystemState::DEPARTURES &&
        system_state != SystemState::NO_DEPARTURES) {
        return {};
    }

    return network_state.departures.directions[selected_direction - 1];
}

static bool sendNetworkCommand(
    QueueHandle_t queue,
    const NetworkCommand& command,
    TickType_t wait_ticks = 0
) {
    if (xQueueSend(queue, &command, wait_ticks) == pdTRUE) {
        return true;
    }

    ESP_LOGW(TAG, "Failed to queue network command: %d", static_cast<int>(command.type));
    return false;
}

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

    applySettings(settings_);
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
        0                          // Core where the task should run
    );

    NetworkCommand command {};

    if (boot_mode == BootMode::SETUP) {
        command.type = NetworkCommandType::START_SETUP_MODE;
    }
    else {
        command.type = NetworkCommandType::START_NORMAL_MODE;
        command.settings = settings_;
    }

    sendNetworkCommand(
        network_in_queue_,
        command,
        pdMS_TO_TICKS(kControlQueueSendTimeoutMs)
    );
}

void SystemManager::handleButtonCallback(button_event_t event, uint8_t gpio_num, void* user_data) {
    (void)gpio_num;

    auto* self = static_cast<SystemManager*>(user_data);
    if (self == nullptr) {
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

    if (xQueueSend(self->system_in_queue_, &system_event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to queue system event: %d", static_cast<int>(system_event.type));
    }
}

void SystemManager::setState(SystemState new_state) {
    if (system_state_ == new_state) {
        return;
    }

    system_state_ = new_state;
    ESP_LOGI(TAG, "State -> %d", static_cast<int>(system_state_));
}

void SystemManager::applySettings(const DeviceSettings& settings) {
    settings_ = settings;
    settings_.direction.startup_direction =
        normalizeDirection(settings_.direction.startup_direction);
    selected_direction_ = settings_.direction.startup_direction;
}

void SystemManager::handleSetupConfig(const SetupConfig& config) {
    if (!isValidSetupConfig(config)) {
        ESP_LOGW(TAG, "Rejected invalid setup config");
        return;
    }

    DeviceSettings updated_settings = settings_;
    applySetupConfig(updated_settings, config);

    if (!saveDeviceSettings(updated_settings)) {
        ESP_LOGW(TAG, "Failed to save setup config");
        setState(SystemState::NETWORK_ERROR);
        return;
    }

    applySettings(updated_settings);

    NetworkCommand command {};
    command.type = NetworkCommandType::START_NORMAL_MODE;
    command.settings = settings_;

    if (!sendNetworkCommand(
        network_in_queue_,
        command,
        pdMS_TO_TICKS(kControlQueueSendTimeoutMs)
    )) {
        setState(SystemState::NETWORK_ERROR);
    }
}

void SystemManager::handleButtonInput(SystemInputEvent event) {
    switch (event) {
        case SystemInputEvent::TOGGLE_DIRECTION:
            selected_direction_++;
            if (selected_direction_ > kMaxDepartureDirections) {
                selected_direction_ = 1;
            }
            break;

        case SystemInputEvent::FORCE_SETUP: {
            NetworkCommand command {};
            command.type = NetworkCommandType::START_SETUP_MODE;

            if (sendNetworkCommand(
                network_in_queue_,
                command,
                pdMS_TO_TICKS(kControlQueueSendTimeoutMs)
            )) {
                setState(SystemState::SETUP);
            }
            break;
        }
    }
}

void SystemManager::setSystemState() {
    SystemState new_state = system_state_;

    switch (network_state_.phase) {
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
            break;
    }

    if (network_state_.phase == NetworkPhase::READY) {
        switch (network_state_.departure_state) {
            case DepartureState::NONE:
                new_state = SystemState::CONNECTED;
                break;

            case DepartureState::READY:
                if (totalDepartureCount(network_state_.departures) == 0) {
                    new_state = SystemState::NO_DEPARTURES;
                    break;
                }

                new_state = SystemState::DEPARTURES;
                break;

            case DepartureState::API_ERROR:
                new_state = SystemState::API_ERROR;
                break;
        }
    }

    setState(new_state);
}

void SystemManager::renderDisplay() {
    DirectionDepartures active_departures =
        getActiveDepartures(network_state_, system_state_, selected_direction_);

    switch (system_state_) {
        case SystemState::BOOT:
            matrix_.bootAnimation(animation_frame_);
            break;

        case SystemState::CONNECTING:
            matrix_.setColor(0, 0, 16);
            matrix_.connectionAnimation(animation_frame_);
            break;

        case SystemState::CONNECTED:
            matrix_.setColor(0, 16, 0);
            matrix_.displayIcon(MatrixIcon::OK);
            break;

        case SystemState::SETUP:
            matrix_.setColor(0, 16, 16);
            matrix_.displayIcon(MatrixIcon::HEART);
            break;

        case SystemState::DEPARTURES:
            matrix_.setColor(
                network_state_.stale_data ? 16 : 0,
                16,
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
            matrix_.setColor(16, 16, 0);
            matrix_.displayIcon(MatrixIcon::QUESTION);
            break;

        case SystemState::API_ERROR:
            matrix_.setColor(16, 0, 0);
            matrix_.displayIcon(MatrixIcon::QUESTION);
            break;

        case SystemState::NETWORK_ERROR:
            matrix_.setColor(16, 0, 0);
            matrix_.displayIcon(MatrixIcon::CROSS);
            break;
    }
}

void SystemManager::systemTask(void* pvParameters) {
    auto* self = static_cast<SystemManager*>(pvParameters);
    SystemEvent system_event {};

    while(true) {
        if (xQueueReceive(
            self->system_in_queue_,
            &system_event,
            pdMS_TO_TICKS(self->kUpdateInterval_)
        ) == pdTRUE) {
            switch (system_event.type) {
                case SystemEventType::NETWORK_STATE:
                    self->network_state_ = system_event.network_state;
                    self->setSystemState();
                    break;

                case SystemEventType::INPUT_EVENT:
                    self->handleButtonInput(system_event.input_event);
                    break;

                case SystemEventType::SETUP_CONFIG:
                    self->handleSetupConfig(system_event.setup_config);
                    break;
            }
        }

        self->animation_frame_++;
        self->renderDisplay();
    }
}
