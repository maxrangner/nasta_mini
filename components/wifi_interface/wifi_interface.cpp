#include "wifi_interface.h"
#include "esp_log.h"
#include "message_types.h"
#include "credentials.h"

static const char *TAG = "wifi interface";

WifiInterface::WifiInterface(QueueHandle_t queue) 
    : network_in_queue_(queue) {
}

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

    setStaMode();
    wifi_config_t credentials = setCredentials();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &credentials));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

void WifiInterface::connect() {
    ESP_LOGI(TAG, "connect");
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void WifiInterface::disconnect() {
    ESP_LOGI(TAG, "disconnect");
    ESP_ERROR_CHECK(esp_wifi_disconnect());
}

void WifiInterface::setStaMode() {
    ESP_LOGI(TAG, "setStaMode");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
}

void WifiInterface::setApMode() {
    ESP_LOGI(TAG, "setApMode");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
}

void WifiInterface::wifiEventCallback(void* arg, 
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data) {
        (void)event_data;
        auto* self = static_cast<WifiInterface*>(arg);
        WifiLinkEvent event {};
        if (self->toWifiLinkEvent(event_base, event_id, &event)) {
            self->sendLinkEvent(event);
        }
}

wifi_config_t WifiInterface::setCredentials() {
    wifi_config_t credentials = {};
    memcpy(credentials.sta.ssid, SSID, strlen(SSID));
    memcpy(credentials.sta.password, PASSWORD, strlen(PASSWORD));
    return credentials;
}

void WifiInterface::sendLinkEvent(WifiLinkEvent event) {
    NetworkPacket packet {};
    packet.wifi_link_event = event;
    xQueueSend(network_in_queue_, &packet, 0);
}

bool WifiInterface::toWifiLinkEvent(esp_event_base_t base, int32_t id, WifiLinkEvent* event) {
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        *event = WifiLinkEvent::LINK_CONNECTED_STA;
        return true;
    }

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        *event = WifiLinkEvent::LINK_DISCONNECTED;
        return true;
    }

    if (base == WIFI_EVENT && id == WIFI_EVENT_AP_START) {
        *event = WifiLinkEvent::LINK_AP_ACTIVE;
        return true;
    }

    return false;
}
