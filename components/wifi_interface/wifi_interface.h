#pragma once
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class WifiInterface {
    char ssid_[100];
    char password_[100];
    QueueHandle_t network_in_queue_ = nullptr;
public:
    WifiInterface(QueueHandle_t queue);
    void init();
    void connect();
    void disconnect();
    void setMode();
    wifi_config_t setCredentials();
    static void wifiEventCallback(void* arg, 
        esp_event_base_t event_base,
        int32_t event_id,
        void* event_data);
};