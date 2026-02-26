#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_context.h"
#include "wifi_interface.h"

enum class NetworkState {
    INIT, // 0
    CONNECTING_STA,  // 1
    CONNECTED_STA, // 2
    DISCONNECTED, // 3
    ERROR // 4
};

class NetworkManager {
    TaskHandle_t task_network_manager_ = nullptr;
    QueueHandle_t queue_data_ = nullptr;
    QueueHandle_t queue_settings_ = nullptr;
    QueueHandle_t wifi_event_queue_ = nullptr;
    WifiInterface wifi_interface_;
    static constexpr uint32_t kUpdateInterval_ = 100;
public:
    NetworkManager(Queues* queues);
    static void networkTask(void* pvParameters);
    void init();
};