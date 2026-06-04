#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_crt_bundle.h"
#include "app_context.h"
#include "wifi_interface.h"
#include "message_types.h"
#include "settings.h"

class NetworkManager {
    enum class NetworkMode {
        NONE,
        NORMAL,
        SETUP
    };
    NetworkMode mode_ = NetworkMode::NONE;
    static constexpr BaseType_t kTaskCore_ = 1;
    TaskHandle_t task_network_manager_ = nullptr;
    QueueHandle_t system_in_queue_ = nullptr;
    QueueHandle_t network_in_queue_ = nullptr;
    WifiInterface wifi_interface_;
    NetworkState network_state_ {};
    TickType_t prev_reconnect_attempt_ = 0;
    TickType_t prev_api_fetch_ = 0;
    TickType_t last_successful_fetch_ = 0;
    uint8_t reconnection_attempts_ = 0;
    uint8_t api_failures_ = 0;
    
    static constexpr uint32_t kUpdateInterval_ = 1000;
    static constexpr uint32_t kApiTiming_ = 10000;
    static constexpr uint32_t kReconnectTiming_ = 10000;
    static constexpr uint32_t kStaleDataTiming_ = 30000;
    static constexpr uint8_t kMaxRetries_ = 5;
    static constexpr uint8_t kMaxApiFailures_ = 6;
    static constexpr size_t kMaxApiBufferSize_ = 102400;
    static constexpr size_t kMaxApiUrlLength_ = 160;
    char* api_buffer = nullptr;
    char api_url_[kMaxApiUrlLength_] = {};
    DeviceSettings applied_settings_ {};
    httpd_handle_t setup_server_ = nullptr;

    esp_http_client_config_t http_cfg_ {};
    void setNetworkPhase(NetworkPhase new_phase);
    bool handleWifiError(esp_err_t err, const char* action);
    void handleWifiEvent(WifiLinkEvent event);
    void sendNetworkState();
    bool buildApiUrl();
    void startSetupMode();
    void startNormalMode(const DeviceSettings& settings);
public:
    NetworkManager(Queues* queues);
    void init();

private:
    static void networkTask(void* pvParameters);

    bool apiFetch();
    bool jsonParser(const char* buffer, Departures* departures);
};
