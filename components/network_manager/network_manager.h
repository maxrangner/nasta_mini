#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "app_context.h"
#include "wifi_interface.h"
#include "message_types.h"

class NetworkManager {
    enum class NetworkState {
        ONLINE,
        OFFLINE,
        SETUP_PORTAL,
        API_ERROR
    };
    NetworkState network_state = NetworkState::OFFLINE;
    TaskHandle_t task_network_manager_ = nullptr;
    QueueHandle_t system_in_queue_ = nullptr;
    QueueHandle_t network_in_queue_ = nullptr;
    WifiInterface wifi_interface_;
    DataPacket packet_;
    
    static constexpr uint32_t kUpdateInterval_ = 1000;
    static constexpr size_t kMaxApiBufferSize_ = 102400;
    char* api_buffer;

    esp_http_client_config_t http_cfg_;
public:
    NetworkManager(Queues* queues);
    static void networkTask(void* pvParameters);

    void apiFetch(esp_http_client_config_t* cfg);
    void jsonParser(char* buffer);
};