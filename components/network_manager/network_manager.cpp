#include "network_manager.h"
#include "esp_log.h"
#include "message_types.h"

static const char *TAG = "network manager";

NetworkManager::NetworkManager(Queues* queues) {
    xTaskCreatePinnedToCore(     // UI Task
      networkTask,               // Function to implement the task
      "networkTask",             // Name of the task
      8192,                      // Stack size in words
      this,                      // Task input parameter
      1,                         // Priority of the task
      &task_network_manager_,    // Task handle.
      0                          // Core where the task should run
    );
    queue_data_ = queues->data_queue;
    queue_settings_ = queues->settings_queue;
    wifi_event_queue_ = queues->wifi_event_queue;
}


void NetworkManager::networkTask(void* pvParameters) {
    auto* self = static_cast<NetworkManager*>(pvParameters);
    WifiEvent wifi_event;;

    while(true) {
        xQueueReceive(self->wifi_event_queue_, &wifi_event, portMAX_DELAY);
        ESP_LOGI(TAG, "Wifi event: %d", wifi_event);
    }
}

void NetworkManager::init() {
    wifi_interface_.init(wifi_event_queue_);
}