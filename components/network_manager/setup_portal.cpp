#include "setup_portal.h"

#include "esp_log.h"
#include "message_types.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

static const char* TAG = "setup portal";
static constexpr size_t kMaxEncodedSsidLength = kMaxWifiSsidLength * 3;
static constexpr size_t kMaxEncodedPasswordLength = kMaxWifiPasswordLength * 3;
static constexpr size_t kMaxSiteIdLength = 10;
static constexpr size_t kMaxTransportValueLength = 15;
static constexpr size_t kMaxDirectionValueLength = 3;

static constexpr size_t kMaxSetupRequestLength =
    sizeof("ssid=") - 1 +
    kMaxEncodedSsidLength +
    sizeof("&password=") - 1 +
    kMaxEncodedPasswordLength +
    sizeof("&site_id=") - 1 +
    kMaxSiteIdLength +
    sizeof("&transport=") - 1 +
    kMaxTransportValueLength +
    sizeof("&direction=") - 1 +
    kMaxDirectionValueLength +
    1;

static const char* kSetupPage =
R"html(
<!doctype html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>sl-go-mini setup</title>
<style>
body { font-family: sans-serif; max-width: 420px; margin: 24px auto; padding: 0 16px; }
label { display: block; margin-top: 14px; font-weight: 600; }
input, select, button { width: 100%; box-sizing: border-box; padding: 12px; margin-top: 6px; }
button { margin-top: 20px; }
</style>
</head>
<body>
<h1>sl-go-mini setup</h1>
<form method="post" action="/save">
<label for="ssid">Wi-Fi name</label>
<input id="ssid" name="ssid" maxlength="32" required>
<label for="password">Wi-Fi password</label>
<input id="password" name="password" type="password" maxlength="64">
<label for="site_id">SL site id</label>
<input id="site_id" name="site_id" type="number" min="1" required>
<label for="transport">Transport filter</label>
<select id="transport" name="transport">
<option value="">Any</option>
<option value="METRO">Metro</option>
<option value="TRAIN">Train</option>
<option value="BUS">Bus</option>
<option value="TRAM">Tram</option>
</select>
<label for="direction">Startup direction</label>
<select id="direction" name="direction">
<option value="1">Direction 1</option>
<option value="2">Direction 2</option>
</select>
<button type="submit">Save and restart</button>
</form>
</body>
</html>
)html";

static const char* kSetupSavedPage =
R"html(
<!doctype html>
<html>
<head><meta name="viewport" content="width=device-width, initial-scale=1"><title>Saved</title></head>
<body style="font-family:sans-serif;max-width:420px;margin:24px auto;padding:0 16px;">
<h1>Settings saved</h1>
<p>The device is switching to normal Wi-Fi mode now.</p>
</body>
</html>
)html";

static const char* kSetupErrorPage =
R"html(
<!doctype html>
<html>
<head><meta name="viewport" content="width=device-width, initial-scale=1"><title>Setup error</title></head>
<body style="font-family:sans-serif;max-width:420px;margin:24px auto;padding:0 16px;">
<h1>Setup error</h1>
<p>Submitted settings were invalid. Please go back and try again.</p>
</body>
</html>
)html";

static void urlDecode(char* destination, size_t destination_size, const char* source) {
    if (destination == nullptr || destination_size == 0) {
        return;
    }

    size_t write_index = 0;

    for (size_t i = 0; source[i] != '\0' && write_index + 1 < destination_size; i++) {
        if (source[i] == '+') {
            destination[write_index++] = ' ';
            continue;
        }

        if (source[i] == '%' &&
            source[i + 1] != '\0' &&
            source[i + 2] != '\0') {
            char hex[3] = { source[i + 1], source[i + 2], '\0' };
            destination[write_index++] = static_cast<char>(strtol(hex, nullptr, 16));
            i += 2;
            continue;
        }

        destination[write_index++] = source[i];
    }

    destination[write_index] = '\0';
}

static esp_err_t sendSetupErrorResponse(httpd_req_t* req) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kSetupErrorPage, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handleSetupPageRequest(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kSetupPage, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handleSetupSaveRequest(httpd_req_t* req) {
    QueueHandle_t system_in_queue = reinterpret_cast<QueueHandle_t>(req->user_ctx);
    char body[kMaxSetupRequestLength] = {};

    if (req->content_len <= 0 || req->content_len >= sizeof(body)) {
        return sendSetupErrorResponse(req);
    }

    int received = 0;
    while (received < req->content_len) {
        int read = httpd_req_recv(req, body + received, req->content_len - received);
        if (read <= 0) {
            return sendSetupErrorResponse(req);
        }

        received += read;
    }
    body[received] = '\0';

    SetupConfig config {};
    char encoded_ssid[kMaxWifiSsidLength * 3 + 1] = {};
    char encoded_password[kMaxWifiPasswordLength * 3 + 1] = {};
    char site_id_buffer[16] = {};
    char direction_buffer[4] = {};
    char transport_buffer[16] = {};

    if (httpd_query_key_value(body, "ssid", encoded_ssid, sizeof(encoded_ssid)) != ESP_OK ||
        httpd_query_key_value(body, "site_id", site_id_buffer, sizeof(site_id_buffer)) != ESP_OK ||
        httpd_query_key_value(body, "direction", direction_buffer, sizeof(direction_buffer)) != ESP_OK) {
        return sendSetupErrorResponse(req);
    }

    httpd_query_key_value(body, "password", encoded_password, sizeof(encoded_password));
    httpd_query_key_value(body, "transport", transport_buffer, sizeof(transport_buffer));

    urlDecode(config.wifi.ssid, sizeof(config.wifi.ssid), encoded_ssid);
    urlDecode(config.wifi.password, sizeof(config.wifi.password), encoded_password);

    char* site_id_end = nullptr;
    unsigned long site_id = strtoul(site_id_buffer, &site_id_end, 10);
    if (site_id_end == site_id_buffer || *site_id_end != '\0') {
        return sendSetupErrorResponse(req);
    }
    config.site.site_id = static_cast<uint32_t>(site_id);

    char* direction_end = nullptr;
    unsigned long direction = strtoul(direction_buffer, &direction_end, 10);
    if (direction_end == direction_buffer || *direction_end != '\0') {
        return sendSetupErrorResponse(req);
    }
    config.startup_direction = static_cast<uint8_t>(direction);
    config.site.transport_filter = toTransportMode(transport_buffer);

    if (!isValidSetupConfig(config)) {
        return sendSetupErrorResponse(req);
    }

    SystemEvent event {};
    event.type = SystemEventType::SETUP_CONFIG;
    event.setup_config = config;

    if (xQueueSend(system_in_queue, &event, 0) != pdTRUE) {
        return sendSetupErrorResponse(req);
    }

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kSetupSavedPage, HTTPD_RESP_USE_STRLEN);
}

bool startSetupPortal(httpd_handle_t* server, QueueHandle_t system_in_queue) {
    if (server == nullptr) {
        return false;
    }

    if (*server != nullptr) {
        return true;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 4;

    esp_err_t err = httpd_start(server, &config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        *server = nullptr;
        return false;
    }

    httpd_uri_t page_handler = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = &handleSetupPageRequest,
        .user_ctx = system_in_queue
    };

    httpd_uri_t save_handler = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = &handleSetupSaveRequest,
        .user_ctx = system_in_queue
    };

    httpd_register_uri_handler(*server, &page_handler);
    httpd_register_uri_handler(*server, &save_handler);
    ESP_LOGI(TAG, "Setup portal started");
    return true;
}

void stopSetupPortal(httpd_handle_t* server) {
    if (server == nullptr || *server == nullptr) {
        return;
    }

    httpd_stop(*server);
    *server = nullptr;
    ESP_LOGI(TAG, "Setup portal stopped");
}
