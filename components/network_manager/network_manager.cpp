#include "network_manager.h"
#include "esp_log.h"
#include "message_types.h"
#include "cJSON.h"
#include "utils.h"

static const char *TAG = "network manager";

NetworkManager::NetworkManager(Queues* queues) : system_in_queue_(queues->system_in_queue), network_in_queue_(queues->network_in_queue), wifi_interface_(queues->network_in_queue) {
    api_buffer = (char*)malloc(kMaxApiBufferSize_);
    if (api_buffer == nullptr) {
        ESP_LOGW(TAG, "kMaxApiBufferSize_ malloc failed");
    }
    
    xTaskCreatePinnedToCore(     // UI Task
        networkTask,               // Function to implement the task
        "networkTask",             // Name of the task
        8192,                      // Stack size in words
        this,                      // Task input parameter
        1,                         // Priority of the task
        &task_network_manager_,    // Task handle.
        0                          // Core where the task should run
    );

    http_cfg_ = {
        .url = "https://transport.integration.sl.se/v1/sites/9143/departures?transport=METRO&forecast=500",
        // .url = "http://"CONFIG_EXAMPLE_HTTP_ENDPOINT"/get",
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
        .crt_bundle_attach = esp_crt_bundle_attach
    };

    wifi_interface_.init();
}

void NetworkManager::networkTask(void* pvParameters) {
    auto* self = static_cast<NetworkManager*>(pvParameters);
    uint32_t now = 0;
    uint32_t last_api_fetch = 0;
    uint32_t api_fetch_interval = pdMS_TO_TICKS(10000);

    while(true) {
        if (xQueueReceive(self->network_in_queue_, &self->packet_, pdMS_TO_TICKS(self->kUpdateInterval_))) {
            if (self->packet_.type == PacketType::WIFI_UPDATE) {
                ESP_LOGI(TAG, "Packet - WIFI_UPDATE: %d", self->packet_.wifi_event);
            }
        }
        now = xTaskGetTickCount();
        if ((now - last_api_fetch > api_fetch_interval) && self->packet_.wifi_event == WifiLinkEvent::LINK_CONNECTED_STA) {
            self->apiFetch(&self->http_cfg_);
            self->jsonParser(self->api_buffer);
            last_api_fetch = now;
        }
    }
}

void NetworkManager::apiFetch(esp_http_client_config_t* cfg) {
    ESP_LOGI(TAG, "Fetching API");

    esp_http_client_handle_t client = esp_http_client_init(cfg);
    if (!client) {
        ESP_LOGW(TAG, "Client init failed");
        return;
    }
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HTTP open failed: %d", err);
        esp_http_client_cleanup(client);
        return;
    }

    uint32_t content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) { // || content_length > kMaxApiBufferSize_)
        ESP_LOGW(TAG, "Invalid content_length: %d", content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return;
    } else if (content_length > kMaxApiBufferSize_) return;

    if (api_buffer == nullptr) {
        ESP_LOGW(TAG, "Error allocating api_buffer to heap");
        return;
    }

    uint32_t total_read = 0;
    uint32_t start_tick = xTaskGetTickCount();
    const uint32_t max_wait_ticks = pdMS_TO_TICKS(5000);

    while (total_read < content_length) {
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
        return;
    }

    api_buffer[total_read] = '\0';
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

void NetworkManager::jsonParser(char* buffer) {
    ESP_LOGI(TAG, "Parsing json");

    cJSON* root = cJSON_Parse(buffer);
    if (root == nullptr) {
        ESP_LOGW(TAG, "Error parsing root");
    }
    
    cJSON* departures = cJSON_GetObjectItem(root, "departures");
    if (departures == nullptr) {
        ESP_LOGW(TAG, "Error parsing departures");
    }
    
    Departures new_departures;
    new_departures.num_direction_1 = 0;
    new_departures.num_direction_2 = 0;
    
    if (cJSON_IsArray(departures)) {
        uint8_t count = cJSON_GetArraySize(departures);
        ESP_LOGW(TAG, "Departures: %d", count);

        for (uint8_t i = 0; i < count; i++) {
            Departure new_departure;
            cJSON* departure = cJSON_GetArrayItem(departures, i);

            cJSON* state = cJSON_GetObjectItem(departure, "state");
            if (strcmp(state->valuestring, "EXPECTED") != 0) {
                continue;
            }

            cJSON* destination = cJSON_GetObjectItem(departure, "destination");
            snprintf(new_departure.destination, sizeof(new_departure.destination), "%s", destination->valuestring);
            cJSON* direction_code = cJSON_GetObjectItem(departure, "direction_code");
            new_departure.direction_code = direction_code->valueint;
            cJSON* display = cJSON_GetObjectItem(departure, "display");
            snprintf(new_departure.display, sizeof(new_departure.display), "%s", display->valuestring);
            cJSON* transport_mode = cJSON_GetObjectItem(departure, "display");
            new_departure.transport_mode = toTransportMode(transport_mode->valuestring);
            cJSON* line = cJSON_GetObjectItem(departure, "line");
            cJSON* line_id = cJSON_GetObjectItem(line, "id");
            new_departure.line = line_id->valueint;

            if (new_departure.direction_code == 1) {
                new_departures.departures_dir_1[new_departures.num_direction_1] = new_departure;
                new_departures.num_direction_1++;
            }
            if (new_departure.direction_code == 2) {
                new_departures.departures_dir_2[new_departures.num_direction_1] = new_departure;
                new_departures.num_direction_2++;
            }

            ESP_LOGI(TAG, "%s %s", destination->valuestring, display->valuestring);
            printf("\n");
        }
        // cJSON* departure = cJSON_GetArrayItem(departures, 0);
        // cJSON* destination = cJSON_GetObjectItem(departure, "destination");
        // cJSON* display = cJSON_GetObjectItem(departure, "display");

        // ESP_LOGI(TAG, "num_departures: %d Next departure:\n%s: %s", count, destination->valuestring, display->valuestring);
    }
    DataPacket packet {
        .type = PacketType::API_DATA,
        .departures = new_departures
    };
    xQueueSend(system_in_queue_, &packet, 0);
    cJSON_Delete(root);
}