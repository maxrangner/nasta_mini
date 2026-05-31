#include "system_manager.h"
#include "esp_log.h"
#include "types.h"
#include "credentials.h"

static const char *TAG = "system manager";

SystemManager::SystemManager(Queues* queues)
    : system_in_queue_(queues->system_in_queue),
      network_in_queue_(queues->network_in_queue) {
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

void SystemManager::handleNetworkStatus(NetworkStatus status) {
    switch (status) {
        case NetworkStatus::DISCONNECTED:
            setState(SystemState::NO_CONNECTION);
            break;

        case NetworkStatus::CONNECTING:
            setState(SystemState::CONNECTING);
            break;

        case NetworkStatus::CONNECTED:
            if (system_state_ == SystemState::BOOT ||
                system_state_ == SystemState::NO_CONNECTION ||
                system_state_ == SystemState::CONNECTING ||
                system_state_ == SystemState::NETWORK_ERROR) {
                setState(SystemState::CONNECTING);
            }
            break;

        case NetworkStatus::SETUP:
            setState(SystemState::SETUP);
            break;

        case NetworkStatus::NETWORK_ERROR:
            setState(SystemState::NETWORK_ERROR);
            break;
    }
}

void SystemManager::handleDepartures(const Departures& departures) {
    if ((departures.directions[0].count + departures.directions[1].count) == 0) {
        setState(SystemState::NO_DEPARTURES);
        return;
    }

    setState(SystemState::DEPARTURES);
}

void SystemManager::handleApiError() {
    setState(SystemState::API_ERROR);
}

void SystemManager::systemTask(void* pvParameters) {
    auto* self = static_cast<SystemManager*>(pvParameters);

    while(true) {
        if (xQueueReceive(self->system_in_queue_, &self->packet_, pdMS_TO_TICKS(self->kUpdateInterval_))) {
            switch (self->packet_.type) {
                case SystemPacketType::NETWORK_STATUS:
                    ESP_LOGI(TAG, "Packet - NETWORK_STATUS: %d", static_cast<int>(self->packet_.network_status));
                    self->handleNetworkStatus(self->packet_.network_status);
                    break;

                case SystemPacketType::DEPARTURES_DATA:
                    ESP_LOGI(TAG, "Packet - DEPARTURES_DATA");
                    self->handleDepartures(self->packet_.departures);
                    break;

                case SystemPacketType::API_ERROR:
                    ESP_LOGI(TAG, "Packet - API_ERROR");
                    self->handleApiError();
                    break;
            }
        }
    }
}
