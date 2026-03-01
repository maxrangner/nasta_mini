#include "wifi_interface.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "credentials.h"
#include "message_types.h"

static const char *TAG = "wifi interface";

WifiInterface::WifiInterface(QueueHandle_t queue) : network_in_queue_(queue) {}

void WifiInterface::init() {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifiEventCallback,
        this);

    esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifiEventCallback,
        this);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t credentials = setCredentials();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &credentials));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

void WifiInterface::connect() {
    esp_wifi_connect();
}

void WifiInterface::disconnect() {

}

void WifiInterface::setMode() {

}

wifi_config_t WifiInterface::setCredentials() {
    wifi_config_t credentials = {};
    memcpy(credentials.sta.ssid, SSID, strlen(SSID));
    memcpy(credentials.sta.password, PASSWORD, strlen(PASSWORD));
    return credentials;
}

void WifiInterface::wifiEventCallback(void* arg, 
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data) {
        auto* self = static_cast<WifiInterface*>(arg);
        DataPacket packet;
        packet.type = PacketType::WIFI_EVENT;

        if (event_base == WIFI_EVENT) {
            switch (event_id) {
                case WIFI_EVENT_STA_START:
                    packet.network_event = NetworkEvent::STARTED;
                    xQueueSend(self->network_in_queue_, &packet, 0);
                    break;
                case WIFI_EVENT_STA_DISCONNECTED:
                    packet.network_event = NetworkEvent::DISCONNECTED;
                    xQueueSend(self->network_in_queue_, &packet, 0);
                    break;
                default:
                    break;
            }
        } 
        else if (event_base == IP_EVENT) {
            switch (event_id) {
                case IP_EVENT_STA_GOT_IP:
                    packet.network_event = NetworkEvent::CONNECTED;
                    xQueueSend(self->network_in_queue_, &packet, 0);
                    break;
                default:
                    break;
            }
        }
}
