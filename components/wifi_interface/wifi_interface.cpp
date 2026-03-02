#include "wifi_interface.h"
#include "esp_log.h"
#include "message_types.h"
#include "credentials.h"

static const char *TAG = "wifi interface";

WifiInterface::WifiInterface(QueueHandle_t queue) 
    : network_in_queue_(queue), 
    retry_timer_(nullptr), 
    task_handle_(nullptr), 
    wifi_state_(WifiState::DISCONNECTED), 
    retry_count_(0) {
        wifi_queue_ = xQueueCreate(8, sizeof(WifiEvent));
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

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t credentials = setCredentials();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &credentials));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    retry_timer_ = xTimerCreate(
        "wifi_retry",
        pdMS_TO_TICKS(5000),
        pdFALSE,
        this,
        timerCallback
    );

    xTaskCreate(
        wifiTask,
        "wifi_task",
        4096,
        this,
        5,
        &task_handle_
    );

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
        
        WifiEvent event = self->toWifiEvent(event_base, event_id);
        if (event != WifiEvent::NONE) {
            xQueueSend(self->wifi_queue_, &event, 0);
        }
}

void WifiInterface::timerCallback(TimerHandle_t xTimer) {
    auto* self = static_cast<WifiInterface*>(pvTimerGetTimerID(xTimer));
    
    WifiEvent event = WifiEvent::RETRY_TIMEOUT;
    xQueueSend(self->wifi_queue_, &event, 0);
}

void WifiInterface::wifiTask(void* arg) {
    auto* self = static_cast<WifiInterface*>(arg);
    WifiEvent event;

    while (true) {
        if (xQueueReceive(self->wifi_queue_, &event, portMAX_DELAY) == pdPASS) {
            self->processEvent(event);
        }
    }
}

void WifiInterface::processEvent(WifiEvent event) {
    WifiState current = wifi_state_;
    WifiState next = stateMachine(current, event);

    if (next == current) return;

    wifi_state_ = next;
    handleStateChange(wifi_state_);
}

WifiInterface::WifiEvent WifiInterface::toWifiEvent(esp_event_base_t base, int32_t id) {
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START)
            return WifiEvent::STARTED;
        if (id == WIFI_EVENT_STA_DISCONNECTED)
            return WifiEvent::LOST_CONNECTION;
    }
    else if (base == IP_EVENT) {
        if (id == IP_EVENT_STA_GOT_IP)
            return WifiEvent::GOT_IP;
    }

    return WifiEvent::NONE;
}

WifiInterface::WifiState WifiInterface::stateMachine(WifiState current, WifiEvent event) {
    switch (current) {
        case WifiState::DISCONNECTED:
            if (event == WifiEvent::STARTED ||
                event == WifiEvent::RETRY_TIMEOUT)
                return WifiState::CONNECTING_STA;
            break;

        case WifiState::CONNECTING_STA:
            if (event == WifiEvent::GOT_IP)
                return WifiState::CONNECTED_STA;

            if (event == WifiEvent::LOST_CONNECTION)
                return WifiState::DISCONNECTED;
            break;

        case WifiState::CONNECTED_STA:
            if (event == WifiEvent::LOST_CONNECTION)
                return WifiState::DISCONNECTED;
            break;
        default: break;
    }

    return current;
}

void WifiInterface::handleStateChange(WifiState new_state) {
    DataPacket packet{};

    switch (new_state) {
        case WifiState::CONNECTING_STA:
            ESP_LOGI(TAG, "handleStateChange::CONNECTING_STA");
            esp_wifi_connect();
            packet.type = PacketType::WIFI_UPDATE;
            packet.wifi_event = WifiLinkEvent::LINK_CONNECTING_STA;
            xQueueSend(network_in_queue_, &packet, 0);
            break;

        case WifiState::CONNECTED_STA:
            ESP_LOGI(TAG, "handleStateChange::CONNECTED_STA");
            retry_count_ = 0;
            packet.type = PacketType::WIFI_UPDATE;
            packet.wifi_event = WifiLinkEvent::LINK_CONNECTED_STA;
            xQueueSend(network_in_queue_, &packet, 0);
            break;

        case WifiState::DISCONNECTED:
            ESP_LOGI(TAG, "handleStateChange::DISCONNECTED");
            ESP_LOGW(TAG, "WiFi connection lost");
            packet.type = PacketType::WIFI_UPDATE;
            packet.wifi_event = WifiLinkEvent::LINK_DISCONNECTED;
            xQueueSend(network_in_queue_, &packet, 0);

            if (retry_count_ < MAX_RETRIES_) {
                retry_count_++;
                xTimerStart(retry_timer_, 0);
            }
            break;
        default: break;
    }
}
