#pragma once
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class WifiInterface {
    char ssid_[100];
    char password_[100];
    QueueHandle_t wifi_event_queue_ = nullptr;
public:
    WifiInterface();
    void init(QueueHandle_t queue);
    wifi_config_t setCredentials();
    void setMode();
    static void callback(void* arg, 
        esp_event_base_t event_base,
        int32_t event_id,
        void* event_data);
};