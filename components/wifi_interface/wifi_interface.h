#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_event.h"
#include "esp_wifi.h"

class WifiInterface {
public:
    enum class WifiState {
        DISCONNECTED,
        CONNECTING_STA,
        CONNECTED_STA,
        STARTING_AP,
        AP_ACTIVE
    };
    
    explicit WifiInterface(QueueHandle_t network_queue);
    void init();
    
private:
    enum class WifiEvent {
        STARTED,
        GOT_IP,
        LOST_CONNECTION,
        RETRY_TIMEOUT,
        NONE
    };

    static void wifiEventCallback(void* arg, 
        esp_event_base_t event_base,
        int32_t event_id,
        void* event_data);

    static void timerCallback(TimerHandle_t xTimer);
    static void wifiTask(void* arg);

    void connect();
    void disconnect();
    void setMode();
    wifi_config_t setCredentials();
    void processEvent(WifiEvent event);
    WifiState stateMachine(WifiState current, WifiEvent event);
    void handleStateChange(WifiState new_state);
    WifiEvent toWifiEvent(esp_event_base_t base, int32_t id);

    QueueHandle_t network_in_queue_;
    QueueHandle_t wifi_queue_;
    TimerHandle_t retry_timer_;
    TaskHandle_t task_handle_;
    
    char ssid_[100];
    char password_[100];

    WifiState wifi_state_;
    uint8_t retry_count_;
    const uint8_t MAX_RETRIES_ = 5;
};