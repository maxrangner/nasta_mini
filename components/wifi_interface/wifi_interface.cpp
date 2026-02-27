#include "wifi_interface.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "credentials.h"
#include "message_types.h"

static const char *TAG = "wifi interface";

WifiInterface::WifiInterface() {}

void WifiInterface::init(QueueHandle_t queue) {
    wifi_event_queue_ = queue;
    
    // Init phase
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event callbacks
    esp_event_handler_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &callback,
        this);

    esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &callback,
        this);

    // Cfg phase
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t credentials = setCredentials();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &credentials));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

void WifiInterface::connect() {
    esp_wifi_connect();
}

wifi_config_t WifiInterface::setCredentials() {
    wifi_config_t credentials = {};
    memcpy(credentials.sta.ssid, SSID, strlen(SSID));
    memcpy(credentials.sta.password, PASSWORD, strlen(PASSWORD));
    return credentials;
}

void WifiInterface::setMode() {

}

void WifiInterface::callback(void* arg, 
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data) {
        auto* self = static_cast<WifiInterface*>(arg);
        NetworkEvent event;

        if (event_base == WIFI_EVENT) {
            switch (event_id) {
                case WIFI_EVENT_STA_START:
                    event = NetworkEvent::STARTED;
                    xQueueSend(self->wifi_event_queue_, &event, 0);
                    break;
                case WIFI_EVENT_STA_DISCONNECTED:
                    event = NetworkEvent::DISCONNECTED;
                    xQueueSend(self->wifi_event_queue_, &event, 0);
                    break;
                default:
                    break;
            }
        } 
        else if (event_base == IP_EVENT) {
            switch (event_id) {
                case IP_EVENT_STA_GOT_IP:
                    event = NetworkEvent::CONNECTED;
                    xQueueSend(self->wifi_event_queue_, &event, 0);
                    break;
                default:
                    break;
            }
        }
}
