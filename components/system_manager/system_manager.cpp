#include "system_manager.h"
#include "esp_log.h"
#include "types.h"
#include "credentials.h"

static const char *TAG = "system manager";

SystemManager::SystemManager(Queues* queues) {
    system_in_queue_ = queues->system_in_queue;
    network_in_queue_ = queues->network_in_queue;
    
    xTaskCreatePinnedToCore(     // UI Task
      systemTask,                // Function to implement the task
      "systemTask",              // Name of the task
      8192,                      // Stack size in words
      this,                      // Task input parameter
      2,                         // Priority of the task
      &task_system_manager_,     // Task handle.
      0                          // Core where the task should run
    );
    // settings_ = {
    //     .setting_transport_mode = TransportMode::METRO,
    //     .setting_direction_code = 1,
    //     .setting_ssid = SSID,
    //     .setting_password = PASSWORD
    // };
}

void SystemManager::systemTask(void* pvParameters) {
    auto* self = static_cast<SystemManager*>(pvParameters);

    while(true) {
        vTaskDelay(pdMS_TO_TICKS(SystemManager::kUpdateInterval_));
    }
}