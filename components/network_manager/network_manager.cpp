#include "network_manager.h"
#include "esp_log.h"
#include "cJSON.h"
#include "setup_portal.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "network manager";

NetworkManager::NetworkManager(Queues* queues)
    : system_in_queue_(queues->system_in_queue),
      network_in_queue_(queues->network_in_queue),
      wifi_interface_(queues->network_in_queue) {
}

void NetworkManager::setNetworkPhase(NetworkPhase new_phase) {
    if (network_state_.phase == new_phase) {
        return;
    }

    network_state_.phase = new_phase;
    ESP_LOGI(TAG, "Phase -> %d", static_cast<int>(network_state_.phase));
}

bool NetworkManager::handleWifiError(esp_err_t err, const char* action) {
    if (err == ESP_OK) {
        return true;
    }

    ESP_LOGW(TAG, "%s failed: %s", action, esp_err_to_name(err));
    setNetworkPhase(NetworkPhase::ERROR);
    sendNetworkState();
    return false;
}

void NetworkManager::handleWifiEvent(WifiLinkEvent event) {
    switch (event) {
        case WifiLinkEvent::LINK_CONNECTED_STA:
            reconnection_attempts_ = 0;
            prev_reconnect_attempt_ = 0;
            prev_api_fetch_ = 0;
            api_failures_ = 0;
            setNetworkPhase(NetworkPhase::READY);
            if (network_state_.departure_state == DepartureState::API_ERROR) {
                network_state_.departure_state = DepartureState::NONE;
                network_state_.stale_data = false;
            }
            sendNetworkState();
            break;

        case WifiLinkEvent::LINK_AP_ACTIVE:
            if (mode_ != NetworkMode::SETUP) {
                ESP_LOGI(TAG, "Ignoring AP start outside setup mode");
                break;
            }

            reconnection_attempts_ = 0;
            prev_reconnect_attempt_ = 0;
            prev_api_fetch_ = 0;
            api_failures_ = 0;
            setNetworkPhase(NetworkPhase::SETUP);
            if (!startSetupPortal(&setup_server_, system_in_queue_)) {
                setNetworkPhase(NetworkPhase::ERROR);
                sendNetworkState();
            }
            break;

        case WifiLinkEvent::LINK_DISCONNECTED:
            if (mode_ == NetworkMode::SETUP) {
                ESP_LOGI(TAG, "Ignoring disconnect in setup mode");
                break;
            }

            if (mode_ != NetworkMode::NORMAL) {
                setNetworkPhase(NetworkPhase::ERROR);
                sendNetworkState();
                break;
            }

            if (network_state_.departure_state == DepartureState::READY) {
                network_state_.stale_data = true;
            }
            prev_api_fetch_ = 0;
            api_failures_ = 0;
            prev_reconnect_attempt_ = xTaskGetTickCount();
            setNetworkPhase(NetworkPhase::CONNECTING);
            sendNetworkState();
            break;
    }
}

void NetworkManager::sendNetworkState() {
    SystemEvent event {};
    event.type = SystemEventType::NETWORK_STATE;
    event.network_state = network_state_;
    if (xQueueSend(system_in_queue_, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to queue system event: %d", static_cast<int>(event.type));
    }
}

bool NetworkManager::buildApiUrl() {
    const char* transport_filter = toTransportModeApiString(applied_settings_.site.transport_filter);
    const char* transport_filter_log = transport_filter[0] != '\0' ? transport_filter : "ANY";

    ESP_LOGI(
        TAG,
        "Fetch config - site_id: %lu, transport: %s",
        static_cast<unsigned long>(applied_settings_.site.site_id),
        transport_filter_log
    );

    if (transport_filter[0] == '\0') {
        int written = snprintf(
            api_url_,
            sizeof(api_url_),
            "https://transport.integration.sl.se/v1/sites/%lu/departures?forecast=500",
            static_cast<unsigned long>(applied_settings_.site.site_id)
        );

        return written > 0 && written < sizeof(api_url_);
    }

    int written = snprintf(
        api_url_,
        sizeof(api_url_),
        "https://transport.integration.sl.se/v1/sites/%lu/departures?transport=%s&forecast=500",
        static_cast<unsigned long>(applied_settings_.site.site_id),
        transport_filter
    );

    return written > 0 && written < sizeof(api_url_);
}

void NetworkManager::startNormalMode(const DeviceSettings& settings) {
    NetworkMode previous_mode = mode_;

    mode_ = NetworkMode::NORMAL;
    applied_settings_ = settings;
    reconnection_attempts_ = 0;
    prev_reconnect_attempt_ = 0;
    prev_api_fetch_ = 0;
    last_successful_fetch_ = 0;
    api_failures_ = 0;

    stopSetupPortal(&setup_server_);

    if (previous_mode != NetworkMode::NONE) {
        if (!handleWifiError(wifi_interface_.stop(), "stop Wi-Fi")) {
            return;
        }
    }

    if (!buildApiUrl()) {
        ESP_LOGW(TAG, "Failed to build API URL from settings");
        setNetworkPhase(NetworkPhase::ERROR);
        sendNetworkState();
        return;
    }

    if (api_buffer == nullptr) {
        api_buffer = (char*)malloc(kMaxApiBufferSize_);
        if (api_buffer == nullptr) {
            ESP_LOGW(TAG, "kMaxApiBufferSize_ malloc failed");
            setNetworkPhase(NetworkPhase::ERROR);
            sendNetworkState();
            return;
        }
    }

    http_cfg_ = {};
    http_cfg_.url = api_url_;
    http_cfg_.method = HTTP_METHOD_GET;
    http_cfg_.timeout_ms = 5000;
    http_cfg_.crt_bundle_attach = esp_crt_bundle_attach;

    if (!handleWifiError(wifi_interface_.setStaMode(), "set STA mode")) {
        return;
    }
    if (!handleWifiError(wifi_interface_.setStaConfig(applied_settings_.wifi), "set STA config")) {
        return;
    }
    if (!handleWifiError(wifi_interface_.start(), "start STA mode")) {
        return;
    }

    setNetworkPhase(NetworkPhase::CONNECTING);
    network_state_.departure_state = DepartureState::NONE;
    network_state_.stale_data = false;
    network_state_.departures = {};
    sendNetworkState();
    handleWifiError(wifi_interface_.connect(), "connect STA");
}

void NetworkManager::startSetupMode() {
    if (mode_ == NetworkMode::SETUP &&
        network_state_.phase == NetworkPhase::SETUP) {
        return;
    }

    NetworkMode previous_mode = mode_;

    mode_ = NetworkMode::SETUP;
    reconnection_attempts_ = 0;
    prev_reconnect_attempt_ = 0;
    prev_api_fetch_ = 0;
    last_successful_fetch_ = 0;
    api_failures_ = 0;
    stopSetupPortal(&setup_server_);

    if (previous_mode != NetworkMode::NONE) {
        if (!handleWifiError(wifi_interface_.stop(), "stop Wi-Fi")) {
            return;
        }
    }

    setNetworkPhase(NetworkPhase::SETUP);
    network_state_.departure_state = DepartureState::NONE;
    network_state_.stale_data = false;
    network_state_.departures = {};
    sendNetworkState();

    if (!handleWifiError(wifi_interface_.setApMode(), "set AP mode")) {
        return;
    }
    if (!handleWifiError(wifi_interface_.setApConfig(), "set AP config")) {
        return;
    }
    handleWifiError(wifi_interface_.start(), "start AP mode");
}

void NetworkManager::init() {
    if (task_network_manager_ != nullptr) {
        return;
    }

    if (!handleWifiError(wifi_interface_.init(), "initialize Wi-Fi")) {
        return;
    }

    xTaskCreatePinnedToCore(       // Network Task
        networkTask,               // Function to implement the task
        "networkTask",             // Name of the task
        8192,                      // Stack size in words
        this,                      // Task input parameter
        1,                         // Priority of the task
        &task_network_manager_,    // Task handle.
        kTaskCore_                 // Core where the task should run
    );
}

void NetworkManager::networkTask(void* pvParameters) {
    auto* self = static_cast<NetworkManager*>(pvParameters);
    TickType_t now = 0;
    Departures latest_departures {};
    NetworkCommand command {};

    while(true) {
        if (xQueueReceive(self->network_in_queue_, &command, pdMS_TO_TICKS(self->kUpdateInterval_))) {
            switch (command.type) {
                case NetworkCommandType::WIFI_LINK_EVENT:
                    ESP_LOGI(TAG, "Command - WIFI_LINK_EVENT: %d", static_cast<int>(command.wifi_link_event));
                    self->handleWifiEvent(command.wifi_link_event);
                    break;

                case NetworkCommandType::START_SETUP_MODE:
                    ESP_LOGI(TAG, "Command - START_SETUP_MODE");
                    self->startSetupMode();
                    break;

                case NetworkCommandType::START_NORMAL_MODE:
                    ESP_LOGI(TAG, "Command - START_NORMAL_MODE");
                    self->startNormalMode(command.settings);
                    break;
            }
        }
        now = xTaskGetTickCount();

        if (self->mode_ == NetworkMode::NORMAL &&
            self->network_state_.phase == NetworkPhase::CONNECTING &&
            self->prev_reconnect_attempt_ != 0 &&
            (now - self->prev_reconnect_attempt_) >= pdMS_TO_TICKS(self->kReconnectTiming_)) {
            if (self->reconnection_attempts_ >= self->kMaxRetries_) {
                ESP_LOGW(TAG, "Too many Wi-Fi reconnect failures, switching to setup mode");
                self->startSetupMode();
            }
            else {
                self->reconnection_attempts_++;
                self->prev_reconnect_attempt_ = now;
                self->sendNetworkState();
                if (!self->handleWifiError(self->wifi_interface_.setStaMode(), "set STA mode")) {
                    continue;
                }
                self->handleWifiError(self->wifi_interface_.connect(), "connect STA");
            }
        }

        if (self->network_state_.phase == NetworkPhase::READY &&
            self->network_state_.departure_state == DepartureState::READY &&
            !self->network_state_.stale_data &&
            self->last_successful_fetch_ != 0 &&
            (now - self->last_successful_fetch_) >= pdMS_TO_TICKS(self->kStaleDataTiming_)) {
            self->network_state_.stale_data = true;
            self->sendNetworkState();
        }

        if (self->network_state_.phase == NetworkPhase::READY &&
            (self->prev_api_fetch_ == 0 ||
            (now - self->prev_api_fetch_) >= pdMS_TO_TICKS(self->kApiTiming_))) {
            self->prev_api_fetch_ = now;

            bool fetch_ok = self->apiFetch() &&
                self->jsonParser(self->api_buffer, &latest_departures);
            bool has_cached_data =
                self->network_state_.departure_state == DepartureState::READY;

            if (fetch_ok) {
                self->api_failures_ = 0;
                self->last_successful_fetch_ = now;
                self->setNetworkPhase(NetworkPhase::READY);
                self->network_state_.departure_state = DepartureState::READY;
                self->network_state_.stale_data = false;
                self->network_state_.departures = latest_departures;
                self->sendNetworkState();
                continue;
            }

            if (has_cached_data) {
                if (!self->network_state_.stale_data) {
                    self->network_state_.stale_data = true;
                    self->sendNetworkState();
                }
                continue;
            }

            if (self->api_failures_ < self->kMaxApiFailures_) {
                self->api_failures_++;

                if (self->api_failures_ >= self->kMaxApiFailures_) {
                    self->network_state_.departure_state = DepartureState::API_ERROR;
                    self->network_state_.stale_data = false;
                    self->sendNetworkState();
                }
            }
        }
    }
}

bool NetworkManager::apiFetch() {
    if (api_buffer == nullptr) {
        ESP_LOGW(TAG, "Error allocating api_buffer to heap");
        return false;
    }

    ESP_LOGI(TAG, "Fetching API");

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg_);
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

    int32_t header_length = esp_http_client_fetch_headers(client);
    if (header_length < 0) {
        ESP_LOGW(TAG, "Failed to fetch HTTP headers: %d", header_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    int status_code = esp_http_client_get_status_code(client);
    if (status_code < 200 || status_code >= 300) {
        ESP_LOGW(TAG, "Unexpected HTTP status: %d", status_code);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    int64_t content_length = esp_http_client_get_content_length(client);
    if (content_length >= kMaxApiBufferSize_) {
        ESP_LOGW(TAG, "content_length too large: %lld", content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    uint32_t total_read = 0;
    uint32_t start_tick = xTaskGetTickCount();
    const uint32_t max_wait_ticks = pdMS_TO_TICKS(5000);

    while (true) {
        int32_t bytes_left = static_cast<int32_t>(kMaxApiBufferSize_ - total_read - 1);
        if (bytes_left <= 0) {
            ESP_LOGW(TAG, "API response too large for buffer");
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return false;
        }

        if (content_length > 0) {
            int64_t remaining = content_length - total_read;
            if (remaining <= 0) {
                break;
            }

            if (remaining < bytes_left) {
                bytes_left = static_cast<int32_t>(remaining);
            }
        }

        int32_t read = esp_http_client_read(client,
            api_buffer + total_read,
            bytes_left
        );
        if (read < 0) {
            ESP_LOGW(TAG, "Read error");
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return false;
        }
        if (read == 0) {
            if (esp_http_client_is_complete_data_received(client)) {
                break;
            }

            if ((xTaskGetTickCount() - start_tick) > max_wait_ticks) {
                ESP_LOGW(TAG, "Read timeout");
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return false;
            }

            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        total_read += read;
        start_tick = xTaskGetTickCount();
    }

    if (!esp_http_client_is_complete_data_received(client)) {
        ESP_LOGW(TAG, "HTTP body was not fully received");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    if (content_length > 0 && total_read != static_cast<uint32_t>(content_length)) {
        ESP_LOGW(TAG, "Incomplete body: %lu/%lld", total_read, content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    if (total_read == 0) {
        ESP_LOGW(TAG, "Empty HTTP body");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    api_buffer[total_read] = '\0';
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return true;
}

bool NetworkManager::jsonParser(const char* buffer, Departures* departures_out) {
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
            ESP_LOGW(TAG, "Skipping departure without state");
            continue;
        }
        if (strcmp(state->valuestring, "EXPECTED") != 0) {
            continue;
        }

        cJSON* direction_code_json = cJSON_GetObjectItem(departure, "direction_code");
        cJSON* display = cJSON_GetObjectItem(departure, "display");
        cJSON* line_json = cJSON_GetObjectItem(departure, "line");

        if (!cJSON_IsObject(line_json) ||
            !cJSON_IsNumber(direction_code_json) ||
            !cJSON_IsString(display)) {
            ESP_LOGW(TAG, "Skipping malformed departure");
            continue;
        }

        cJSON* transport_mode_json = cJSON_GetObjectItem(line_json, "transport_mode");

        if (!cJSON_IsString(transport_mode_json)) {
            ESP_LOGW(TAG, "Skipping malformed departure");
            continue;
        }

        uint8_t direction = direction_code_json->valueint;
        TransportMode transport_mode = toTransportMode(transport_mode_json->valuestring);

        if (applied_settings_.site.transport_filter != TransportMode::UNKNOWN &&
            transport_mode != applied_settings_.site.transport_filter) {
            continue;
        }

        snprintf(new_departure.display, sizeof(new_departure.display), "%s", display->valuestring);

        if (direction >= 1 &&
            direction <= kMaxDepartureDirections) {
            DirectionDepartures& direction_departures =
                new_departures.directions[direction - 1];

            if (direction_departures.count < kMaxDeparturesPerDirection) {
                direction_departures.departures[direction_departures.count] = new_departure;
                direction_departures.count++;
            }
        }
    }
    if (departures_out == nullptr) {
        cJSON_Delete(root);
        return false;
    }

    *departures_out = new_departures;
    cJSON_Delete(root);
    return true;
}
