#include "system_manager.h"
#include "esp_log.h"
#include "settings_storage.h"
#include <string.h>

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

    ESP_LOGI(TAG, "State -> %d", static_cast<int>(system_state_));
    ESP_LOGI(TAG, "Boot mode -> %d", static_cast<int>(boot_mode));
    updateDisplay();
    displayUpdate();

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

    if (xQueueSend(network_in_queue_, &command, pdMS_TO_TICKS(kControlQueueSendTimeoutMs)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to queue network command: %d", static_cast<int>(command.type));
    }
}

SystemState SystemManager::getState() const {
    return system_state_;
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

void SystemManager::handleNetworkStateEvent(const NetworkState& network_state) {
    network_state_ = network_state;

    switch (network_state_.status) {
        case NetworkStatus::CONNECTING:
            setState(SystemState::CONNECTING);
            return;

        case NetworkStatus::SETUP:
            setState(SystemState::SETUP);
            return;

        case NetworkStatus::SETUP_ERROR:
        case NetworkStatus::NETWORK_ERROR:
            setState(SystemState::NETWORK_ERROR);
            return;

        case NetworkStatus::CONNECTED:
            setState(SystemState::CONNECTED);
            return;

        case NetworkStatus::NO_DEPARTURES:
            setState(SystemState::NO_DEPARTURES);
            return;

        case NetworkStatus::DEPARTURES_FRESH:
        case NetworkStatus::DEPARTURES_STALE:
            setState(SystemState::DEPARTURES);
            return;

        case NetworkStatus::API_ERROR:
            setState(SystemState::API_ERROR);
            return;
    }
}

void SystemManager::handleSetupConfigEvent(const SetupConfig& setup_config) {
    if (!isValidSetupConfig(setup_config)) {
        ESP_LOGW(TAG, "Rejected invalid setup config");
        return;
    }

    DeviceSettings updated_settings = settings_;
    applySetupConfig(updated_settings, setup_config);

    if (!saveDeviceSettings(updated_settings)) {
        ESP_LOGW(TAG, "Failed to save setup config");
        setState(SystemState::NETWORK_ERROR);
        return;
    }

    settings_ = updated_settings;
    selected_direction_ = settings_.startup_direction;

    NetworkCommand normal_command {};
    normal_command.type = NetworkCommandType::START_NORMAL_MODE;
    normal_command.settings = settings_;

    if (xQueueSend(
        network_in_queue_,
        &normal_command,
        pdMS_TO_TICKS(kControlQueueSendTimeoutMs)
    ) != pdTRUE) {
        ESP_LOGW(
            TAG,
            "Failed to queue network command: %d",
            static_cast<int>(normal_command.type)
        );
        setState(SystemState::NETWORK_ERROR);
    }
}

void SystemManager::handleSystemEvent(const SystemEvent& system_event) {
    switch (system_event.type) {
        case SystemEventType::NETWORK_STATE:
            handleNetworkStateEvent(system_event.network_state);
            break;

        case SystemEventType::INPUT_EVENT:
            switch (system_event.input_event) {
                case SystemInputEvent::TOGGLE_DIRECTION:
                    selected_direction_++;

                    if (selected_direction_ > kMaxDepartureDirections) {
                        selected_direction_ = 1;
                    }

                    direction_change_counter_++;
                    ESP_LOGI(TAG, "Direction -> %u", static_cast<unsigned>(selected_direction_));
                    break;

                case SystemInputEvent::FORCE_SETUP: {
                    ESP_LOGI(TAG, "Button requested setup mode");

                    NetworkCommand setup_command {};
                    setup_command.type = NetworkCommandType::START_SETUP_MODE;

                    if (xQueueSend(
                        network_in_queue_,
                        &setup_command,
                        pdMS_TO_TICKS(kControlQueueSendTimeoutMs)
                    ) == pdTRUE) {
                        setState(SystemState::SETUP);
                    }
                    else {
                        ESP_LOGW(TAG, "Failed to queue network command: %d", static_cast<int>(setup_command.type));
                    }
                    break;
                }
            }
            break;

        case SystemEventType::SETUP_CONFIG:
            handleSetupConfigEvent(system_event.setup_config);
            break;
    }

    updateDisplay();
}

void SystemManager::setState(SystemState new_state) {
    system_state_ = new_state;
    ESP_LOGI(TAG, "State -> %d", static_cast<int>(system_state_));
}

DisplayData SystemManager::makeDisplayData() const {
    DisplayData display_data {};
    display_data.active_direction = selected_direction_;
    display_data.show_stale_data = network_state_.status == NetworkStatus::DEPARTURES_STALE;
    display_data.direction_change_counter = direction_change_counter_;

    switch (system_state_) {
        case SystemState::BOOT:
            display_data.screen = DisplayScreen::BOOT;
            break;

        case SystemState::CONNECTING:
            display_data.screen = DisplayScreen::CONNECTING;
            break;

        case SystemState::CONNECTED:
            display_data.screen = DisplayScreen::CONNECTED;
            break;

        case SystemState::SETUP:
            display_data.screen = DisplayScreen::SETUP;
            break;

        case SystemState::DEPARTURES:
            display_data.screen = DisplayScreen::DEPARTURES;

            if (selected_direction_ >= 1 &&
                selected_direction_ <= kMaxDepartureDirections) {
                const DirectionDepartures& active_departures =
                    network_state_.departures.directions[selected_direction_ - 1];

                if (active_departures.count > 0) {
                    display_data.has_departure_for_active_direction = true;
                    memcpy(
                        display_data.departure_text,
                        active_departures.departures[0].display,
                        sizeof(display_data.departure_text)
                    );
                }
            }
            break;

        case SystemState::NO_DEPARTURES:
            display_data.screen = DisplayScreen::NO_DEPARTURES;
            break;

        case SystemState::API_ERROR:
            display_data.screen = DisplayScreen::API_ERROR;
            break;

        case SystemState::NETWORK_ERROR:
            display_data.screen = DisplayScreen::NETWORK_ERROR;
            break;
    }

    return display_data;
}

void SystemManager::updateDisplay() {
    displaySetData(makeDisplayData());
}

void SystemManager::systemTask(void* pvParameters) {
    auto* self = static_cast<SystemManager*>(pvParameters);
    TickType_t next_update = xTaskGetTickCount();

    while(true) {
        SystemEvent system_event {};

        while (xQueueReceive(
            self->system_in_queue_,
            &system_event,
            0
        ) == pdTRUE) {
            self->handleSystemEvent(system_event);
        }

        displayUpdate();
        vTaskDelayUntil(&next_update, pdMS_TO_TICKS(self->kUpdateInterval_));
    }
}
