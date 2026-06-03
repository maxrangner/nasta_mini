#include "wifi_interface.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "wifi interface";
static constexpr const char* kSetupApSsid = "sl-go-mini-setup";
static constexpr uint32_t kLinkEventSendTimeoutMs = 10;

WifiInterface::WifiInterface(QueueHandle_t queue) 
    : network_in_queue_(queue) {
}

esp_err_t WifiInterface::init() {
    if (esp_netif_create_default_wifi_sta() == nullptr) {
        return ESP_FAIL;
    }

    if (esp_netif_create_default_wifi_ap() == nullptr) {
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifiEventCallback,
        this);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifiEventCallback,
        this);
    if (err != ESP_OK) {
        return err;
    }
    
    ESP_LOGI(TAG, "wifi init finished");
    return ESP_OK;
}

esp_err_t WifiInterface::start() {
    ESP_LOGI(TAG, "start");
    return esp_wifi_start();
}

esp_err_t WifiInterface::stop() {
    ESP_LOGI(TAG, "stop");
    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK &&
        err != ESP_ERR_WIFI_NOT_INIT &&
        err != ESP_ERR_WIFI_NOT_STARTED) {
        return err;
    }

    return ESP_OK;
}

esp_err_t WifiInterface::connect() {
    ESP_LOGI(TAG, "connect");
    return esp_wifi_connect();
}

esp_err_t WifiInterface::setStaMode() {
    ESP_LOGI(TAG, "setStaMode");
    return esp_wifi_set_mode(WIFI_MODE_STA);
}

esp_err_t WifiInterface::setApMode() {
    ESP_LOGI(TAG, "setApMode");
    return esp_wifi_set_mode(WIFI_MODE_AP);
}

esp_err_t WifiInterface::setStaConfig(const WifiSettings& settings) {
    wifi_config_t credentials = toStaConfig(settings);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &credentials);
    if (err != ESP_OK) {
        return err;
    }

    return esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
}

esp_err_t WifiInterface::setApConfig() {
    wifi_config_t ap_config = toApConfig();
    return esp_wifi_set_config(WIFI_IF_AP, &ap_config);
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

wifi_config_t WifiInterface::toStaConfig(const WifiSettings& settings) {
    wifi_config_t credentials = {};
    size_t ssid_length = strnlen(settings.ssid, kMaxWifiSsidLength);
    size_t password_length = strnlen(settings.password, kMaxWifiPasswordLength);

    memcpy(credentials.sta.ssid, settings.ssid, ssid_length);
    memcpy(credentials.sta.password, settings.password, password_length);
    return credentials;
}

wifi_config_t WifiInterface::toApConfig() {
    wifi_config_t config = {};
    size_t ssid_length = strlen(kSetupApSsid);

    memcpy(config.ap.ssid, kSetupApSsid, ssid_length);
    config.ap.ssid_len = ssid_length;
    config.ap.max_connection = 4;
    config.ap.authmode = WIFI_AUTH_OPEN;
    config.ap.channel = 1;

    return config;
}

void WifiInterface::sendLinkEvent(WifiLinkEvent event) {
    NetworkCommand command {};
    command.type = NetworkCommandType::WIFI_LINK_EVENT;
    command.wifi_link_event = event;
    if (xQueueSend(
        network_in_queue_,
        &command,
        pdMS_TO_TICKS(kLinkEventSendTimeoutMs)
    ) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to queue network command: %d", static_cast<int>(command.type));
    }
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
