#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "message_types.h"

// WifiInterface is a thin Wi-Fi helper owned by NetworkManager.
class WifiInterface {
public:
    explicit WifiInterface(QueueHandle_t network_queue);
    void init();
    void connect();
    void disconnect();
    void setStaMode();
    void setApMode();

private:
    static void wifiEventCallback(void* arg, 
        esp_event_base_t event_base,
        int32_t event_id,
        void* event_data);
    wifi_config_t setCredentials();
    void sendLinkEvent(WifiLinkEvent event);
    bool toWifiLinkEvent(esp_event_base_t base, int32_t id, WifiLinkEvent* event);

    QueueHandle_t network_in_queue_;
};
