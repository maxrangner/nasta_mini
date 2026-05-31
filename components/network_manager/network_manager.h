#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "app_context.h"
#include "wifi_interface.h"
#include "message_types.h"

class NetworkManager {
    enum class NetworkState {
        INIT,
        STA_CONNECTING,
        STA_CONNECTED,
        STA_RECONNECTING,
        AP_SETUP,
        API_ERROR,
        NETWORK_ERROR
    };
    NetworkState network_state_ = NetworkState::INIT;
    TaskHandle_t task_network_manager_ = nullptr;
    QueueHandle_t system_in_queue_ = nullptr;
    QueueHandle_t network_in_queue_ = nullptr;
    WifiInterface wifi_interface_;
    NetworkPacket packet_ {};
    WifiLinkEvent wifi_link_event_ = WifiLinkEvent::LINK_DISCONNECTED;
    TickType_t prev_reconnect_attempt_ = 0;
    TickType_t prev_api_fetch_ = 0;
    uint8_t reconnection_attempts_ = 0;
    uint8_t api_failures_ = 0;
    
    static constexpr uint32_t kUpdateInterval_ = 1000;
    static constexpr uint32_t kApiTiming_ = 10000;
    static constexpr uint32_t kReconnectTiming_ = 10000;
    static constexpr uint8_t kMaxRetries_ = 5;
    static constexpr uint8_t kMaxApiFailures_ = 6;
    static constexpr size_t kMaxApiBufferSize_ = 102400;
    char* api_buffer = nullptr;

    esp_http_client_config_t http_cfg_ {};
    void setState(NetworkState new_state);
    void handleWifiLinkEvent(WifiLinkEvent event);
    void sendStatus(NetworkStatus status);
public:
    NetworkManager(Queues* queues);
    void init();
    static void networkTask(void* pvParameters);

    bool apiFetch(esp_http_client_config_t* cfg);
    bool jsonParser(char* buffer);
    void sendApiError();
};
