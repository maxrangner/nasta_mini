#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_context.h"
#include "message_types.h"

enum class SystemState {
    BOOT, // 0
    NOT_CONNECTED, // 1
    CONNECTED, // 2
    SETUP, // 3
    ERROR
};

class SystemManager {
    TaskHandle_t task_system_manager_ = nullptr;
    QueueHandle_t system_in_queue_ = nullptr;
    QueueHandle_t network_in_queue_ = nullptr;
    static constexpr uint32_t kUpdateInterval_ = 100;
    DataPacket packet_;
public:
    SystemManager(Queues* queues);
    static void systemTask(void* pvParameters);
};