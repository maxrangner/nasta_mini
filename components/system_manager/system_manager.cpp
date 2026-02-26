#include "system_manager.h"
#include "esp_log.h"

static const char *TAG = "system manager";

SystemManager::SystemManager(Queues* queues) {
    xTaskCreatePinnedToCore(     // UI Task
      systemTask,                // Function to implement the task
      "systemTask",              // Name of the task
      8192,                      // Stack size in words
      this,                      // Task input parameter
      2,                         // Priority of the task
      &task_system_manager_,     // Task handle.
      0                          // Core where the task should run
    );
    queue_data_ = queues->data_queue;
    queue_settings_ = queues->settings_queue;
}


void SystemManager::systemTask(void* pvParameters) {
    auto* self = static_cast<SystemManager*>(pvParameters);

    while(true) {
        vTaskDelay(pdMS_TO_TICKS(SystemManager::kUpdateInterval_));
    }
}