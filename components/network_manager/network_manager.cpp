#include "network_manager.h"
#include "esp_log.h"
#include "message_types.h"
#include "cJSON.h"
#include "utils.h"

static const char *TAG = "network manager";

NetworkManager::NetworkManager(Queues* queues)
    : system_in_queue_(queues->system_in_queue),
      network_in_queue_(queues->network_in_queue),
      wifi_interface_(queues->network_in_queue) {
}

void NetworkManager::setState(NetworkState new_state) {
    if (network_state_ == new_state) {
        return;
    }

    network_state_ = new_state;
    ESP_LOGI(TAG, "State -> %d", static_cast<int>(network_state_));
}

void NetworkManager::sendStatus(NetworkStatus status) {
    SystemPacket packet {};
    packet.type = SystemPacketType::NETWORK_STATUS;
    packet.network_status = status;
    xQueueSend(system_in_queue_, &packet, 0);
}

void NetworkManager::init() {
    if (task_network_manager_ != nullptr) {
        return;
    }

    api_buffer = (char*)malloc(kMaxApiBufferSize_);
    if (api_buffer == nullptr) {
        ESP_LOGW(TAG, "kMaxApiBufferSize_ malloc failed");
    }

    http_cfg_ = {};
    http_cfg_.url = "https://transport.integration.sl.se/v1/sites/9143/departures?transport=METRO&forecast=500";
    // http_cfg_.url = "http://"CONFIG_EXAMPLE_HTTP_ENDPOINT"/get";
    http_cfg_.method = HTTP_METHOD_GET;
    http_cfg_.timeout_ms = 5000;
    http_cfg_.crt_bundle_attach = esp_crt_bundle_attach;

    xTaskCreatePinnedToCore(     // UI Task
        networkTask,               // Function to implement the task
        "networkTask",             // Name of the task
        8192,                      // Stack size in words
        this,                      // Task input parameter
        1,                         // Priority of the task
        &task_network_manager_,    // Task handle.
        0                          // Core where the task should run
    );

    wifi_interface_.init();
    setState(NetworkState::STA_CONNECTING);
    sendStatus(NetworkStatus::CONNECTING);
    wifi_interface_.setStaMode();
    wifi_interface_.connect();
}

void NetworkManager::networkTask(void* pvParameters) {
    auto* self = static_cast<NetworkManager*>(pvParameters);
    uint32_t now = 0;
    uint32_t last_api_fetch = 0;
    uint32_t api_fetch_interval = pdMS_TO_TICKS(10000);

    while(true) {
        if (xQueueReceive(self->network_in_queue_, &self->packet_, pdMS_TO_TICKS(self->kUpdateInterval_))) {
            self->wifi_link_event_ = self->packet_.wifi_link_event;
            ESP_LOGI(TAG, "Packet - WIFI_LINK_EVENT: %d", static_cast<int>(self->wifi_link_event_));
            self->handleWifiLinkEvent(self->wifi_link_event_);
        }
        now = xTaskGetTickCount();

        if (self->retry_pending_ &&
            (now - self->retry_start_time_) > pdMS_TO_TICKS(self->kRetryDelayMs_)) {
            self->retry_pending_ = false;
            self->setState(NetworkState::STA_RECONNECTING);
            self->sendStatus(NetworkStatus::CONNECTING);
            self->wifi_interface_.setStaMode();
            self->wifi_interface_.connect();
        }

        if ((now - last_api_fetch > api_fetch_interval) && self->wifi_link_event_ == WifiLinkEvent::LINK_CONNECTED_STA) {
            if (!self->apiFetch(&self->http_cfg_)) {
                self->setState(NetworkState::API_ERROR);
                self->sendDataError();
            }
            else if (!self->jsonParser(self->api_buffer)) {
                self->setState(NetworkState::API_ERROR);
                self->sendDataError();
            }
            else {
                self->setState(NetworkState::STA_CONNECTED);
            }
            last_api_fetch = now;
        }
    }
}

void NetworkManager::handleWifiLinkEvent(WifiLinkEvent event) {
    switch (event) {
        case WifiLinkEvent::LINK_CONNECTING_STA:
            retry_pending_ = false;
            setState(NetworkState::STA_CONNECTING);
            sendStatus(NetworkStatus::CONNECTING);
            break;

        case WifiLinkEvent::LINK_CONNECTED_STA:
            retry_pending_ = false;
            retry_count_ = 0;
            setState(NetworkState::STA_CONNECTED);
            sendStatus(NetworkStatus::CONNECTED);
            break;

        case WifiLinkEvent::LINK_AP_ACTIVE:
            retry_pending_ = false;
            setState(NetworkState::AP_SETUP);
            sendStatus(NetworkStatus::SETUP);
            break;

        case WifiLinkEvent::LINK_ERROR:
            retry_pending_ = false;
            setState(NetworkState::NETWORK_ERROR);
            sendStatus(NetworkStatus::ERROR);
            break;

        case WifiLinkEvent::LINK_DISCONNECTED:
            sendStatus(NetworkStatus::DISCONNECTED);
            if (network_state_ == NetworkState::STA_CONNECTED ||
                network_state_ == NetworkState::STA_CONNECTING ||
                network_state_ == NetworkState::STA_RECONNECTING ||
                network_state_ == NetworkState::API_ERROR) {
                if (retry_pending_) {
                    break;
                }

                if (retry_count_ >= kMaxRetries_) {
                    retry_pending_ = false;
                    setState(NetworkState::NETWORK_ERROR);
                    sendStatus(NetworkStatus::ERROR);
                    break;
                }

                retry_count_++;
                retry_pending_ = true;
                retry_start_time_ = xTaskGetTickCount();
                setState(NetworkState::STA_RECONNECTING);
            }
            else {
                setState(NetworkState::NETWORK_ERROR);
                sendStatus(NetworkStatus::ERROR);
            }
            break;
    }
}

void NetworkManager::sendDataError() {
    SystemPacket packet {};
    packet.type = SystemPacketType::DATA_ERROR;
    xQueueSend(system_in_queue_, &packet, 0);
}

bool NetworkManager::apiFetch(esp_http_client_config_t* cfg) {
    if (api_buffer == nullptr) {
        ESP_LOGW(TAG, "Error allocating api_buffer to heap");
        return false;
    }

    ESP_LOGI(TAG, "Fetching API");

    esp_http_client_handle_t client = esp_http_client_init(cfg);
    if (!client) {
        ESP_LOGW(TAG, "Client init failed");
        return false;
    }
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HTTP open failed: %d", err);
        esp_http_client_cleanup(client);
        return false;
    }

    int32_t content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGW(TAG, "Invalid content_length: %d", content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }
    if (content_length > kMaxApiBufferSize_) {
        ESP_LOGW(TAG, "content_length too large: %d", content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    uint32_t total_read = 0;
    uint32_t start_tick = xTaskGetTickCount();
    const uint32_t max_wait_ticks = pdMS_TO_TICKS(5000);

    while (total_read < static_cast<uint32_t>(content_length)) {
        int32_t read = esp_http_client_read(client,
            api_buffer + total_read,
            content_length - total_read
        );
        if (read < 0) {
            ESP_LOGW(TAG, "Read error");
            break;
        }
        if (read == 0) {
            if ((xTaskGetTickCount() - start_tick) > max_wait_ticks) {
                ESP_LOGW(TAG, "Read timeout");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        total_read += read;
    }
    if (total_read != content_length) {
        ESP_LOGW(TAG, "Incomplete body: %lu/%d", total_read, content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    api_buffer[total_read] = '\0';
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return true;
}

bool NetworkManager::jsonParser(char* buffer) {
    if (buffer == nullptr) {
        ESP_LOGW(TAG, "No API buffer to parse");
        return false;
    }

    ESP_LOGI(TAG, "Parsing json");

    cJSON* root = cJSON_Parse(buffer);
    if (root == nullptr) {
        ESP_LOGW(TAG, "Error parsing root");
        return false;
    }
    
    cJSON* departures = cJSON_GetObjectItem(root, "departures");
    if (departures == nullptr || !cJSON_IsArray(departures)) {
        ESP_LOGW(TAG, "Error parsing departures");
        cJSON_Delete(root);
        return false;
    }
    
    Departures new_departures {};
    
    uint8_t count = cJSON_GetArraySize(departures);
    ESP_LOGW(TAG, "Departures: %d", count);

    for (uint8_t i = 0; i < count; i++) {
        Departure new_departure {};
        cJSON* departure = cJSON_GetArrayItem(departures, i);

        cJSON* state = cJSON_GetObjectItem(departure, "state");
        if (!cJSON_IsString(state)) {
            ESP_LOGW(TAG, "Error parsing state");
            cJSON_Delete(root);
            return false;
        }
        if (strcmp(state->valuestring, "EXPECTED") != 0) {
            continue;
        }

        cJSON* destination = cJSON_GetObjectItem(departure, "destination");
        cJSON* direction_code = cJSON_GetObjectItem(departure, "direction_code");
        cJSON* display = cJSON_GetObjectItem(departure, "display");
        cJSON* transport_mode = cJSON_GetObjectItem(departure, "display");
        cJSON* line = cJSON_GetObjectItem(departure, "line");
        cJSON* line_id = cJSON_GetObjectItem(line, "id");

        if (!cJSON_IsString(destination) ||
            !cJSON_IsNumber(direction_code) ||
            !cJSON_IsString(display) ||
            !cJSON_IsString(transport_mode) ||
            !cJSON_IsNumber(line_id)) {
            ESP_LOGW(TAG, "Error parsing departure fields");
            cJSON_Delete(root);
            return false;
        }

        snprintf(new_departure.destination, sizeof(new_departure.destination), "%s", destination->valuestring);
        new_departure.direction_code = direction_code->valueint;
        snprintf(new_departure.display, sizeof(new_departure.display), "%s", display->valuestring);
        new_departure.transport_mode = toTransportMode(transport_mode->valuestring);
        new_departure.line = line_id->valueint;

        if (new_departure.direction_code == 1 &&
            new_departures.num_direction_1 < 3) {
            new_departures.departures_dir_1[new_departures.num_direction_1] = new_departure;
            new_departures.num_direction_1++;
        }
        if (new_departure.direction_code == 2 &&
            new_departures.num_direction_2 < 3) {
            new_departures.departures_dir_2[new_departures.num_direction_2] = new_departure;
            new_departures.num_direction_2++;
        }

        ESP_LOGI(TAG, "%s %s", destination->valuestring, display->valuestring);
        printf("\n");
    }
    SystemPacket packet {
        .type = SystemPacketType::DEPARTURES_DATA,
        .departures = new_departures
    };
    xQueueSend(system_in_queue_, &packet, 0);
    cJSON_Delete(root);
    return true;
}
