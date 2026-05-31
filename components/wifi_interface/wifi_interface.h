#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "message_types.h"
#include "settings.h"

// WifiInterface is a thin Wi-Fi helper owned by NetworkManager.
class WifiInterface {
public:
    explicit WifiInterface(QueueHandle_t network_queue);
    void init();
    void start();
    void stop();
    void connect();
    void disconnect();
    void setStaMode();
    void setApMode();
    void setStaConfig(const WifiSettings& settings);
    void setApConfig();

private:
    static void wifiEventCallback(void* arg, 
        esp_event_base_t event_base,
        int32_t event_id,
        void* event_data);
    wifi_config_t toStaConfig(const WifiSettings& settings);
    wifi_config_t toApConfig();
    void sendLinkEvent(WifiLinkEvent event);
    bool toWifiLinkEvent(esp_event_base_t base, int32_t id, WifiLinkEvent* event);

    QueueHandle_t network_in_queue_;
};
