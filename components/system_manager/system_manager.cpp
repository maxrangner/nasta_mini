#include "system_manager.h"
#include "esp_log.h"
static const char *TAG = "system manager";

SystemManager::SystemManager(Queues* queues)
    : system_in_queue_(queues->system_in_queue) {
}

void SystemManager::init() {
    if (task_system_manager_ != nullptr) {
        return;
    }

    ESP_LOGI(TAG, "State -> %d", static_cast<int>(system_state_));

    xTaskCreatePinnedToCore(     // UI Task
        systemTask,                // Function to implement the task
        "systemTask",              // Name of the task
        8192,                      // Stack size in words
        this,                      // Task input parameter
        2,                         // Priority of the task
        &task_system_manager_,     // Task handle.
        0                          // Core where the task should run
    );
}

void SystemManager::setState(SystemState new_state) {
    if (system_state_ == new_state) {
        return;
    }

    system_state_ = new_state;
    ESP_LOGI(TAG, "State -> %d", static_cast<int>(system_state_));
}

void SystemManager::updateSystemState() {
    switch (snapshot_.connectivity) {
        case NetworkStatus::DISCONNECTED:
            setState(SystemState::NO_CONNECTION);
            return;

        case NetworkStatus::CONNECTING:
            setState(SystemState::CONNECTING);
            return;

        case NetworkStatus::SETUP:
            setState(SystemState::SETUP);
            return;

        case NetworkStatus::NETWORK_ERROR:
            setState(SystemState::NETWORK_ERROR);
            return;

        case NetworkStatus::CONNECTED:
            break;
    }

    switch (snapshot_.fetch_status) {
        case FetchStatus::IDLE:
            setState(SystemState::CONNECTED);
            return;

        case FetchStatus::FRESH:
            if (totalDepartureCount(snapshot_.departures) == 0) {
                setState(SystemState::NO_DEPARTURES);
                return;
            }

            setState(SystemState::DEPARTURES);
            return;

        case FetchStatus::STALE:
            if (totalDepartureCount(snapshot_.departures) == 0) {
                setState(SystemState::API_ERROR);
                return;
            }

            setState(SystemState::DEPARTURES);
            return;

        case FetchStatus::API_ERROR:
            setState(SystemState::API_ERROR);
            return;
    }
}

void SystemManager::systemTask(void* pvParameters) {
    auto* self = static_cast<SystemManager*>(pvParameters);

    while(true) {
        if (xQueueReceive(self->system_in_queue_, &self->snapshot_, pdMS_TO_TICKS(self->kUpdateInterval_))) {
            self->updateSystemState();
        }
    }
}
