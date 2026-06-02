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

static bool sendSystemMessage(
    QueueHandle_t queue,
    const SystemMessage& message,
    TickType_t wait_ticks = 0
) {
    if (xQueueSend(queue, &message, wait_ticks) == pdTRUE) {
        return true;
    }

    ESP_LOGW(TAG, "Failed to queue system message: %d", static_cast<int>(message.type));
    return false;
}

static bool sendNetworkPacket(
    QueueHandle_t queue,
    const NetworkPacket& packet,
    TickType_t wait_ticks = 0
) {
    if (xQueueSend(queue, &packet, wait_ticks) == pdTRUE) {
        return true;
    }

    ESP_LOGW(TAG, "Failed to queue network packet: %d", static_cast<int>(packet.type));
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
        .btn_callback = handleButtonEvent,
        .user_data = this,
    };
    button_init(&button_cfg, &main_button_);
    matrix_.init();

    ESP_LOGI(TAG, "State -> %d", static_cast<int>(system_state_));
    ESP_LOGI(TAG, "Boot mode -> %d", static_cast<int>(boot_mode));
    updateRenderState();
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

    startBootFlow();
}

void SystemManager::handleButtonEvent(button_event_t event, uint8_t gpio_num, void* user_data) {
    (void)gpio_num;

    auto* self = static_cast<SystemManager*>(user_data);
    if (self == nullptr) {
        return;
    }

    SystemMessage message {};
    message.type = SystemMessageType::INPUT_EVENT;

    switch (event) {
        case BTN_SHORT_PRESS:
            message.input_event = SystemInputEvent::TOGGLE_DIRECTION;
            break;

        case BTN_LONG_PRESS:
            message.input_event = SystemInputEvent::FORCE_SETUP;
            break;
    }

    sendSystemMessage(self->system_in_queue_, message);
}

void SystemManager::setState(SystemState new_state) {
    if (system_state_ == new_state) {
        return;
    }

    system_state_ = new_state;
    ESP_LOGI(TAG, "State -> %d", static_cast<int>(system_state_));
}

void SystemManager::startBootFlow() {
    NetworkPacket packet {};
    BootMode boot_mode = decideBootMode(settings_);

    if (boot_mode == BootMode::SETUP) {
        packet.type = NetworkPacketType::START_SETUP_MODE;
    }
    else {
        packet.type = NetworkPacketType::START_NORMAL_MODE;
        packet.device_settings = settings_;
    }

    sendNetworkPacket(
        network_in_queue_,
        packet,
        pdMS_TO_TICKS(kControlQueueSendTimeoutMs)
    );
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
        updateRenderState();
        return;
    }

    applySettings(updated_settings);
    updateRenderState();
    startBootFlow();
}

void SystemManager::handleInputEvent(SystemInputEvent event) {
    switch (event) {
        case SystemInputEvent::TOGGLE_DIRECTION:
            selected_direction_++;
            if (selected_direction_ > kMaxDepartureDirections) {
                selected_direction_ = 1;
            }
            updateRenderState();
            break;

        case SystemInputEvent::FORCE_SETUP: {
            NetworkPacket packet {};
            packet.type = NetworkPacketType::START_SETUP_MODE;

            if (sendNetworkPacket(
                network_in_queue_,
                packet,
                pdMS_TO_TICKS(kControlQueueSendTimeoutMs)
            )) {
                setState(SystemState::SETUP);
                updateRenderState();
            }
            break;
        }
    }
}

void SystemManager::updateSystemState() {
    SystemState new_state = system_state_;

    switch (network_state_.connectivity) {
        case NetworkStatus::DISCONNECTED:
            new_state = SystemState::NO_CONNECTION;
            break;

        case NetworkStatus::CONNECTING:
            new_state = SystemState::CONNECTING;
            break;

        case NetworkStatus::SETUP:
            new_state = SystemState::SETUP;
            break;

        case NetworkStatus::NETWORK_ERROR:
            new_state = SystemState::NETWORK_ERROR;
            break;

        case NetworkStatus::CONNECTED:
            break;
    }

    if (network_state_.connectivity == NetworkStatus::CONNECTED) {
        switch (network_state_.fetch_status) {
            case FetchStatus::IDLE:
                new_state = SystemState::CONNECTED;
                break;

            case FetchStatus::FRESH:
                if (totalDepartureCount(network_state_.departures) == 0) {
                    new_state = SystemState::NO_DEPARTURES;
                    break;
                }

                new_state = SystemState::DEPARTURES;
                break;

            case FetchStatus::STALE:
                if (totalDepartureCount(network_state_.departures) == 0) {
                    new_state = SystemState::API_ERROR;
                    break;
                }

                new_state = SystemState::DEPARTURES;
                break;

            case FetchStatus::API_ERROR:
                new_state = SystemState::API_ERROR;
                break;
        }
    }

    setState(new_state);
}

void SystemManager::updateRenderState() {
    uint8_t selected_direction = selected_direction_;

    render_state_.system_state = system_state_;
    render_state_.selected_direction = selected_direction;
    render_state_.stale_data = network_state_.fetch_status == FetchStatus::STALE;
    render_state_.active_departures = {};

    if (selected_direction < 1 ||
        selected_direction > kMaxDepartureDirections) {
        return;
    }

    if (system_state_ != SystemState::DEPARTURES &&
        system_state_ != SystemState::NO_DEPARTURES) {
        return;
    }

    render_state_.active_departures =
        network_state_.departures.directions[selected_direction - 1];
}

void SystemManager::updateAnimationFrame() {
    animation_frame_++;
}

void SystemManager::renderDisplay() {
    switch (render_state_.system_state) {
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

        case SystemState::NO_CONNECTION:
            matrix_.setColor(16, 0, 0);
            matrix_.connectionAnimation(animation_frame_);
            break;

        case SystemState::SETUP:
            matrix_.setColor(0, 16, 16);
            matrix_.displayIcon(MatrixIcon::HEART);
            break;

        case SystemState::DEPARTURES:
            matrix_.setColor(
                render_state_.stale_data ? 16 : 0,
                16,
                0
            );

            if (render_state_.active_departures.count == 0) {
                matrix_.displayIcon(MatrixIcon::QUESTION);
                break;
            }

            matrix_.displayDeparture(
                render_state_.active_departures.departures[0].display,
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

const RenderState& SystemManager::getRenderState() const {
    return render_state_;
}

void SystemManager::systemTask(void* pvParameters) {
    auto* self = static_cast<SystemManager*>(pvParameters);

    while(true) {
        if (xQueueReceive(self->system_in_queue_, &self->message_, pdMS_TO_TICKS(self->kUpdateInterval_))) {
            switch (self->message_.type) {
                case SystemMessageType::NETWORK_STATE:
                    self->network_state_ = self->message_.network_state;
                    self->updateSystemState();
                    self->updateRenderState();
                    break;

                case SystemMessageType::INPUT_EVENT:
                    self->handleInputEvent(self->message_.input_event);
                    break;

                case SystemMessageType::SETUP_CONFIG:
                    self->handleSetupConfig(self->message_.setup_config);
                    break;
            }
        }

        self->updateAnimationFrame();
        self->renderDisplay();
    }
}
