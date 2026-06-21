#include "system_manager.h"
#include "esp_log.h"
#include "settings_storage.h"
#include <string.h>

static const char *TAG = "system manager";
static constexpr uint32_t kControlQueueSendTimeoutMs = 10;

SystemManager::SystemManager(Queues* queues, Display* display)
    : system_in_queue_(queues->system_in_queue),
      network_in_queue_(queues->network_in_queue),
      display_(display) {
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
    if (!isValidDisplayBrightness(settings_.brightness)) {
        settings_.brightness = kDefaultDisplayBrightness;
    }

    selected_direction_ = settings_.startup_direction;

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

    xTaskCreatePinnedToCore(       // UI Task
        systemTask,                // Function to implement the task
        "systemTask",              // Name of the task
        8192,                      // Stack size in words
        this,                      // Task input parameter
        2,                         // Priority of the task
        &task_system_manager_,     // Task handle.
        kTaskCore_                 // Core where the task should run
    );
}

void SystemManager::handleButtonCallback(button_event_t event, uint8_t gpio_num, void* user_data) {
    (void)gpio_num;

    auto* self = static_cast<SystemManager*>(user_data);
    if (self == nullptr || self->system_in_queue_ == nullptr) {
        return;
    }

    SystemEvent system_event {};

    switch (event) {
        case BTN_SHORT_PRESS:
            system_event.type = SystemEventType::TOGGLE_DIRECTION;
            break;

        case BTN_LONG_PRESS:
            system_event.type = SystemEventType::FORCE_SETUP;
            break;

        default:
            return;
    }

    (void)xQueueSend(self->system_in_queue_, &system_event, 0);
}

void SystemManager::startRuntime() {
    BootMode boot_mode = decideBootMode(settings_);
    bool is_setup_mode = boot_mode == BootMode::SETUP;

    ESP_LOGI(TAG, "Boot mode -> %d", static_cast<int>(boot_mode));

    network_state_.status = is_setup_mode ? NetworkStatus::SETUP : NetworkStatus::CONNECTING;
    setSystemState(is_setup_mode ? SystemState::SETUP : SystemState::CONNECTING);
    display_->playAnimation(DisplayAnimation::BOOT);

    if (requestMode(
        is_setup_mode ? NetworkCommandType::START_SETUP_MODE
                      : NetworkCommandType::START_NORMAL_MODE
    )) {
        return;
    }

    setSystemState(SystemState::NETWORK_ERROR);
    display_->playAnimation(DisplayAnimation::NONE);
}

void SystemManager::handleNetworkState(const NetworkState& network_state) {
    network_state_ = network_state;
    setSystemState(stateForNetworkStatus(network_state_.status));
}

void SystemManager::handleToggleDirection() {
    selected_direction_ = selected_direction_ == 1 ? 2 : 1;
    ESP_LOGI(TAG, "Direction -> %u", static_cast<unsigned>(selected_direction_));

    bool show_left_arrow = selected_direction_ == 1;
    if (settings_.flip_direction_arrows) {
        show_left_arrow = !show_left_arrow;
    }

    display_->playAnimation(
        show_left_arrow
            ? DisplayAnimation::DIRECTION_LEFT
            : DisplayAnimation::DIRECTION_RIGHT
    );
}

void SystemManager::handleForceSetup() {
    ESP_LOGI(TAG, "Button requested setup mode");

    if (requestMode(NetworkCommandType::START_SETUP_MODE)) {
        network_state_.status = NetworkStatus::SETUP;
        setSystemState(SystemState::SETUP);
        display_->playAnimation(DisplayAnimation::NONE);
        return;
    }

    setSystemState(SystemState::NETWORK_ERROR);
    display_->playAnimation(DisplayAnimation::NONE);
}

void SystemManager::handleSetupConfig(const SetupConfig& setup_config) {
    if (!isValidSetupConfig(setup_config)) {
        ESP_LOGW(TAG, "Rejected invalid setup config");
        return;
    }

    if (!saveSetupConfig(setup_config)) {
        setSystemState(SystemState::NETWORK_ERROR);
        display_->playAnimation(DisplayAnimation::NONE);
        return;
    }

    network_state_.status = NetworkStatus::CONNECTING;
    setSystemState(SystemState::CONNECTING);

    if (!requestMode(NetworkCommandType::START_NORMAL_MODE)) {
        setSystemState(SystemState::NETWORK_ERROR);
        display_->playAnimation(DisplayAnimation::NONE);
    }
}

void SystemManager::handleSystemEvent(const SystemEvent& system_event) {
    switch (system_event.type) {
        case SystemEventType::NETWORK_STATE:
            handleNetworkState(system_event.network_state);
            break;

        case SystemEventType::TOGGLE_DIRECTION:
            handleToggleDirection();
            break;

        case SystemEventType::FORCE_SETUP:
            handleForceSetup();
            break;

        case SystemEventType::SETUP_CONFIG:
            handleSetupConfig(system_event.setup_config);
            break;
    }
}

bool SystemManager::saveSetupConfig(const SetupConfig& setup_config) {
    DeviceSettings updated_settings = settings_;
    applySetupConfig(updated_settings, setup_config);

    if (!saveDeviceSettings(updated_settings)) {
        ESP_LOGW(TAG, "Failed to save setup config");
        return false;
    }

    settings_ = updated_settings;
    selected_direction_ = settings_.startup_direction;
    return true;
}

bool SystemManager::requestMode(NetworkCommandType command_type) {
    NetworkCommand command {};
    command.type = command_type;

    if (command_type == NetworkCommandType::START_NORMAL_MODE ||
        command_type == NetworkCommandType::START_SETUP_MODE) {
        command.settings = settings_;
    }

    if (xQueueSend(
        network_in_queue_,
        &command,
        pdMS_TO_TICKS(kControlQueueSendTimeoutMs)
    ) == pdTRUE) {
        return true;
    }

    ESP_LOGW(TAG, "Failed to queue network command: %d", static_cast<int>(command.type));
    return false;
}

void SystemManager::setSystemState(SystemState new_state) {
    if (system_state_ == new_state) {
        return;
    }

    system_state_ = new_state;
    ESP_LOGI(TAG, "State -> %d", static_cast<int>(system_state_));
}

DisplayState SystemManager::buildDisplayState() const {
    DisplayState display_state {};
    display_state.system_state = system_state_;
    display_state.walk_time_minutes = settings_.walk_time_minutes;
    display_state.brightness = settings_.brightness;
    display_state.rotate_display_180 = settings_.rotate_display_180;

    if (selected_direction_ < 1 ||
        selected_direction_ > kMaxDepartureDirections) {
        return display_state;
    }

    const DirectionDepartures& active_departures =
        network_state_.departures.directions[selected_direction_ - 1];

    if (active_departures.count == 0) {
        return display_state;
    }

    memcpy(
        display_state.departure_text,
        active_departures.departures[0].display,
        sizeof(display_state.departure_text)
    );
    return display_state;
}

SystemState SystemManager::stateForNetworkStatus(NetworkStatus network_status) {
    switch (network_status) {
        case NetworkStatus::CONNECTING:
            return SystemState::CONNECTING;

        case NetworkStatus::SETUP:
            return SystemState::SETUP;

        case NetworkStatus::SETUP_ERROR:
        case NetworkStatus::NETWORK_ERROR:
            return SystemState::NETWORK_ERROR;

        case NetworkStatus::CONNECTED:
            return SystemState::CONNECTED;

        case NetworkStatus::NO_DEPARTURES:
            return SystemState::NO_DEPARTURES;

        case NetworkStatus::DEPARTURES:
            return SystemState::DEPARTURES;

        case NetworkStatus::API_ERROR:
            return SystemState::API_ERROR;
    }

    return SystemState::NETWORK_ERROR;
}

void SystemManager::systemTask(void* pvParameters) {
    auto* self = static_cast<SystemManager*>(pvParameters);
    self->startRuntime();
    TickType_t next_update = xTaskGetTickCount();

    while(true) {
        SystemEvent system_event {};

        while (xQueueReceive(self->system_in_queue_, &system_event, 0) == pdTRUE) {
            self->handleSystemEvent(system_event);
        }

        self->display_->setState(self->buildDisplayState());
        self->display_->update();
        vTaskDelayUntil(&next_update, pdMS_TO_TICKS(self->kUpdateInterval_));
    }
}
