#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_context.h"
#include "message_types.h"

enum class SystemState {
    BOOT,
    CONNECTING,
    NO_CONNECTION,
    SETUP,
    DEPARTURES,
    NO_DEPARTURES,
    DATA_ERROR,
    ERROR
};

// SystemManager owns app-facing state and system-side behavior.
class SystemManager {
    TaskHandle_t task_system_manager_ = nullptr;
    QueueHandle_t system_in_queue_ = nullptr;
    QueueHandle_t network_in_queue_ = nullptr;
    static constexpr uint32_t kUpdateInterval_ = 100;
    SystemPacket packet_ {};
    SystemState system_state_ = SystemState::BOOT;
public:
    SystemManager(Queues* queues);
    void init();
    static void systemTask(void* pvParameters);
    void setState(SystemState new_state);
    void handleNetworkStatus(NetworkStatus status);
    void handleDepartures(const Departures& departures);
    void handleDataError();
};
