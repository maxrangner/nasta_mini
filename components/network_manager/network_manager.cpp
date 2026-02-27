#include "network_manager.h"
#include "esp_log.h"
#include "message_types.h"

static const char *TAG = "network manager";

NetworkManager::NetworkManager(Queues* queues) {
    queue_data_ = queues->data_queue;
    queue_settings_ = queues->settings_queue;
    wifi_event_queue_ = queues->wifi_event_queue;

    xTaskCreatePinnedToCore(     // UI Task
      networkTask,               // Function to implement the task
      "networkTask",             // Name of the task
      8192,                      // Stack size in words
      this,                      // Task input parameter
      1,                         // Priority of the task
      &task_network_manager_,    // Task handle.
      0                          // Core where the task should run
    );

    wifi_reconnect_timer_ = xTimerCreate(
        "Reconnect Timer",
        pdMS_TO_TICKS(5000),
        pdFALSE,
        this,
        reconnectTimerCallback
    );
    if (wifi_reconnect_timer_ == NULL) {
        ESP_LOGE(TAG, "Failed to create reconnect timer");
    }
}

void NetworkManager::networkTask(void* pvParameters) {
    auto* self = static_cast<NetworkManager*>(pvParameters);

    self->onStateChange(self->network_state);
    NetworkState network_state_next = NetworkState::INIT;
    NetworkEvent wifi_event;

    while(true) {
        xQueueReceive(self->wifi_event_queue_, &wifi_event, portMAX_DELAY);
        ESP_LOGI(TAG, "Wifi event: %d", wifi_event);
        network_state_next = self->stateMachine(self->network_state, wifi_event);

        if (network_state_next != self->network_state) {
            self->network_state = network_state_next;
            self->onStateChange(self->network_state);
        }
    }
}

NetworkManager::NetworkState NetworkManager::stateMachine(NetworkState current_state, NetworkEvent event) {
    NetworkState next_state = current_state;

    switch (current_state) {
        case NetworkState::INIT:
            if (event == NetworkEvent::STARTED) next_state = NetworkState::CONNECTING_STA;
            break;
        case NetworkState::CONNECTING_STA:
            if (event == NetworkEvent::CONNECTED) next_state = NetworkState::CONNECTED_STA;
            if (event == NetworkEvent::DISCONNECTED) next_state = NetworkState::DISCONNECTED;
            break;
        case NetworkState::CONNECTED_STA:
            if (event == NetworkEvent::DISCONNECTED) next_state = NetworkState::DISCONNECTED;
            break;
        case NetworkState::DISCONNECTED:
            if (event == NetworkEvent::CONNECTED) next_state = NetworkState::CONNECTED_STA;
            if (event == NetworkEvent::RETRY_TIMER) {
                if (reconnect_retires_++ >= 5) {
                    next_state = NetworkState::ERROR;
                } else {
                    next_state = NetworkState::CONNECTING_STA;
                }
            }
            if (event == NetworkEvent::ERROR) next_state = NetworkState::ERROR;
            break;
        case NetworkState::ERROR:
            break;
        default: break;
    }

    return next_state;
}

void NetworkManager::onStateChange(NetworkState new_state) {
    switch (new_state) {
        case NetworkState::INIT:
            ESP_LOGI(TAG, "WiFi init");
            wifi_interface_.init(wifi_event_queue_);
            break;
        case NetworkState::CONNECTING_STA:
            ESP_LOGI(TAG, "Connecteing to WiFi...");
            xTimerStop(wifi_reconnect_timer_, 0);
            wifi_interface_.connect();
            break;
        case NetworkState::CONNECTED_STA:
            ESP_LOGI(TAG, "Connected to WiFi!");
            reconnect_retires_ = 0;
            break;
        case NetworkState::DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected...");
            xTimerStart(wifi_reconnect_timer_, 0);
            break;
        case NetworkState::ERROR:
            ESP_LOGI(TAG, "Error!");
            break;
        default: break;
    }
}

void NetworkManager::reconnectTimerCallback(TimerHandle_t xTimer) {
    auto* self = static_cast<NetworkManager*>(
        pvTimerGetTimerID(xTimer)
    );
    NetworkEvent event = NetworkEvent::RETRY_TIMER;
    xQueueSend(self->wifi_event_queue_, &event, 0);
}