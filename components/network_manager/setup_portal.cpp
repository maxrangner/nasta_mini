#include "setup_portal.h"

#include "esp_netif.h"
#include "esp_log.h"
#include "message_types.h"
#include "utils.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/sockets.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char* TAG = "setup portal";
static constexpr const char* kSetupPortalUrl = "http://setup.sl-go-mini/";
static constexpr const char* kApNetifKey = "WIFI_AP_DEF";
static constexpr uint16_t kDnsPort = 53;
static constexpr size_t kMaxDnsPacketSize = 512;
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

static const char* kSharedStyle = R"css(
:root {
    /* Theme variables: update these to restyle the portal quickly. */
    --bg: #eef3f7;
    --bg-accent: #d7e5f2;
    --card: #ffffff;
    --card-border: #d8e1e8;
    --text: #14212b;
    --muted: #5c6b76;
    --accent: #0f6d95;
    --accent-contrast: #ffffff;
    --input-bg: #f8fbfd;
    --input-border: #c6d2db;
    --input-focus: #0f6d95;
    --danger: #b53d32;
    --success: #1f7a4f;
    --radius: 20px;
    --shadow: 0 18px 48px rgba(20, 33, 43, 0.12);
    --space: 18px;
    --max-width: 520px;
}

* { box-sizing: border-box; }

body {
    margin: 0;
    min-height: 100vh;
    font-family: Arial, Helvetica, sans-serif;
    color: var(--text);
    background:
        radial-gradient(circle at top, var(--bg-accent), transparent 40%),
        linear-gradient(180deg, #f7fafc 0%, var(--bg) 100%);
    padding: 20px 14px 28px;
}

.shell {
    max-width: var(--max-width);
    margin: 0 auto;
}

.card {
    background: var(--card);
    border: 1px solid var(--card-border);
    border-radius: var(--radius);
    box-shadow: var(--shadow);
    overflow: hidden;
}

.hero,
.content {
    padding: 22px;
}

.hero {
    border-bottom: 1px solid var(--card-border);
}

.badge {
    display: inline-block;
    padding: 6px 10px;
    border-radius: 999px;
    background: rgba(15, 109, 149, 0.1);
    color: var(--accent);
    font-size: 13px;
    font-weight: 700;
    letter-spacing: 0.02em;
    text-transform: uppercase;
}

h1 {
    margin: 14px 0 10px;
    font-size: 30px;
    line-height: 1.05;
}

h2 {
    margin: 0 0 6px;
    font-size: 18px;
}

p {
    margin: 0;
    color: var(--muted);
    line-height: 1.5;
}

form {
    margin: 0;
}

fieldset {
    margin: 0 0 24px;
    padding: 0;
    border: 0;
}

legend {
    margin-bottom: 12px;
    padding: 0;
    font-size: 18px;
    font-weight: 700;
}

.field {
    margin-top: 14px;
}

label {
    display: block;
    margin-bottom: 6px;
    font-weight: 700;
}

.hint {
    display: block;
    margin-top: 6px;
    color: var(--muted);
    font-size: 14px;
}

input,
select,
button {
    width: 100%;
    border-radius: 14px;
    border: 1px solid var(--input-border);
    background: var(--input-bg);
    color: var(--text);
    font: inherit;
}

input,
select {
    padding: 13px 14px;
}

input:focus,
select:focus,
button:focus {
    outline: none;
    border-color: var(--input-focus);
    box-shadow: 0 0 0 4px rgba(15, 109, 149, 0.14);
}

.password-wrap {
    position: relative;
}

.password-wrap input {
    padding-right: 92px;
}

.toggle {
    position: absolute;
    top: 50%;
    right: 8px;
    width: auto;
    margin: 0;
    padding: 8px 12px;
    transform: translateY(-50%);
    border-radius: 10px;
    background: #ffffff;
    font-size: 14px;
    font-weight: 700;
}

.primary {
    margin-top: 6px;
    padding: 14px 16px;
    border: 0;
    background: var(--accent);
    color: var(--accent-contrast);
    font-weight: 700;
    cursor: pointer;
}

.primary[disabled] {
    opacity: 0.78;
    cursor: default;
}

.status {
    border-left: 4px solid var(--accent);
    padding-left: 12px;
}

.status.success {
    border-left-color: var(--success);
}

.status.error {
    border-left-color: var(--danger);
}

.footer-note {
    margin-top: 14px;
    font-size: 14px;
}

.link {
    color: var(--accent);
    font-weight: 700;
    text-decoration: none;
}

@media (min-width: 720px) {
    body {
        padding: 44px 22px;
    }

    .hero,
    .content {
        padding: 28px;
    }

    h1 {
        font-size: 34px;
    }
}
)css";

static const char* kSetupPageBody = R"html(
<div class="shell">
  <div class="card">
    <div class="hero">
      <span class="badge">Setup mode</span>
      <h1>sl-go-mini</h1>
      <p>Connect Wi-Fi and choose a stop for live departures. If this page does not open automatically, visit <strong>http://setup.sl-go-mini/</strong>.</p>
    </div>
    <div class="content">
      <form method="post" action="/save" id="setup-form">
        <fieldset>
          <legend>Wi-Fi</legend>

          <div class="field">
            <label for="ssid">Wi-Fi name</label>
            <input id="ssid" name="ssid" maxlength="32" placeholder="Home Wi-Fi" autocomplete="username" required>
          </div>

          <div class="field">
            <label for="password">Wi-Fi password</label>
            <div class="password-wrap">
              <input id="password" name="password" type="password" maxlength="64" autocomplete="current-password" placeholder="Optional for open networks">
              <button class="toggle" type="button" id="toggle-password" aria-label="Show password">Show</button>
            </div>
          </div>
        </fieldset>

        <fieldset>
          <legend>Departure settings</legend>

          <div class="field">
            <label for="site_id">SL site id</label>
            <input id="site_id" name="site_id" type="number" min="1" inputmode="numeric" placeholder="Example: 9192" required>
            <span class="hint">Enter the stop or station id used by the SL API.</span>
          </div>

          <div class="field">
            <label for="transport">Transport filter</label>
            <select id="transport" name="transport">
              <option value="">Any</option>
              <option value="METRO">Metro</option>
              <option value="TRAIN">Train</option>
              <option value="BUS">Bus</option>
              <option value="TRAM">Tram</option>
            </select>
          </div>
        </fieldset>

        <fieldset>
          <legend>Display</legend>

          <div class="field">
            <label for="direction">Startup direction</label>
            <select id="direction" name="direction">
              <option value="1">Direction 1</option>
              <option value="2">Direction 2</option>
            </select>
            <span class="hint">A short button press switches direction later.</span>
          </div>
        </fieldset>

        <button class="primary" type="submit" id="save-button">Save and restart</button>
        <p class="footer-note">The device leaves setup mode after saving.</p>
      </form>
    </div>
  </div>
</div>
<script>
(function () {
  var form = document.getElementById('setup-form');
  var password = document.getElementById('password');
  var toggle = document.getElementById('toggle-password');
  var saveButton = document.getElementById('save-button');

  if (toggle && password) {
    toggle.addEventListener('click', function () {
      var isHidden = password.type === 'password';
      password.type = isHidden ? 'text' : 'password';
      toggle.textContent = isHidden ? 'Hide' : 'Show';
      toggle.setAttribute('aria-label', isHidden ? 'Hide password' : 'Show password');
    });
  }

  if (form && saveButton) {
    form.addEventListener('submit', function () {
      saveButton.disabled = true;
      saveButton.textContent = 'Saving...';
    });
  }
})();
</script>
)html";

static const char* kSetupSavedPageBody = R"html(
<div class="shell">
  <div class="card">
    <div class="hero">
      <span class="badge">Saved</span>
      <h1>Settings saved</h1>
      <p>The device is switching to normal Wi-Fi mode now.</p>
    </div>
    <div class="content">
      <p class="status success">You can close this page. The display should reconnect with the new settings shortly.</p>
    </div>
  </div>
</div>
)html";

static const char* kSetupErrorPageBody = R"html(
<div class="shell">
  <div class="card">
    <div class="hero">
      <span class="badge">Setup error</span>
      <h1>Submitted settings were invalid</h1>
      <p>Please go back and try again.</p>
    </div>
    <div class="content">
      <p class="status error">Check that Wi-Fi name, SL site id, and startup direction were filled in correctly.</p>
      <p class="footer-note"><a class="link" href="/">Return to setup</a></p>
    </div>
  </div>
</div>
)html";

static int s_dns_socket = -1;
static uint8_t s_ap_ip_bytes[4] = {};

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

static esp_err_t sendHtmlDocument(httpd_req_t* req, const char* title, const char* body_html) {
    httpd_resp_set_type(req, "text/html");

    esp_err_t err = httpd_resp_sendstr_chunk(
        req,
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>"
    );
    if (err != ESP_OK) {
        return err;
    }

    err = httpd_resp_sendstr_chunk(req, title);
    if (err != ESP_OK) {
        return err;
    }

    err = httpd_resp_sendstr_chunk(req, "</title><style>");
    if (err != ESP_OK) {
        return err;
    }

    err = httpd_resp_sendstr_chunk(req, kSharedStyle);
    if (err != ESP_OK) {
        return err;
    }

    err = httpd_resp_sendstr_chunk(req, "</style></head><body>");
    if (err != ESP_OK) {
        return err;
    }

    err = httpd_resp_sendstr_chunk(req, body_html);
    if (err != ESP_OK) {
        return err;
    }

    err = httpd_resp_sendstr_chunk(req, "</body></html>");
    if (err != ESP_OK) {
        return err;
    }

    return httpd_resp_sendstr_chunk(req, nullptr);
}

static esp_err_t sendRedirectResponse(httpd_req_t* req, const char* location) {
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Location", location);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t sendSetupErrorResponse(httpd_req_t* req) {
    httpd_resp_set_status(req, "400 Bad Request");
    return sendHtmlDocument(req, "Setup error", kSetupErrorPageBody);
}

static esp_err_t handleSetupGetRequest(httpd_req_t* req) {
    (void)req;
    return sendHtmlDocument(req, "sl-go-mini setup", kSetupPageBody);
}

static esp_err_t handleSetupRedirectError(httpd_req_t* req, httpd_err_code_t error) {
    (void)error;
    return sendRedirectResponse(req, "/");
}

static bool configureCaptivePortalDhcp() {
    esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey(kApNetifKey);
    if (ap_netif == nullptr) {
        ESP_LOGW(TAG, "AP netif not available for DHCP captive portal");
        return false;
    }

    esp_err_t err = esp_netif_dhcps_stop(ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGW(TAG, "Failed to stop AP DHCP server: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_netif_dhcps_option(
        ap_netif,
        ESP_NETIF_OP_SET,
        ESP_NETIF_CAPTIVEPORTAL_URI,
        const_cast<char*>(kSetupPortalUrl),
        strlen(kSetupPortalUrl)
    );
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set DHCP captive portal URI: %s", esp_err_to_name(err));
        esp_netif_dhcps_start(ap_netif);
        return false;
    }

    err = esp_netif_dhcps_start(ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_LOGW(TAG, "Failed to start AP DHCP server: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "DHCP captive portal URI -> %s", kSetupPortalUrl);
    return true;
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

    return sendHtmlDocument(req, "Saved", kSetupSavedPageBody);
}

static bool refreshApIpAddress() {
    esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey(kApNetifKey);
    if (ap_netif == nullptr) {
        ESP_LOGW(TAG, "AP netif not available");
        return false;
    }

    esp_netif_ip_info_t ip_info {};
    if (esp_netif_get_ip_info(ap_netif, &ip_info) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read AP IP info");
        return false;
    }

    s_ap_ip_bytes[0] = ip4_addr1(&ip_info.ip);
    s_ap_ip_bytes[1] = ip4_addr2(&ip_info.ip);
    s_ap_ip_bytes[2] = ip4_addr3(&ip_info.ip);
    s_ap_ip_bytes[3] = ip4_addr4(&ip_info.ip);
    return true;
}

static bool startDnsServer() {
    if (s_dns_socket >= 0) {
        return true;
    }

    if (!refreshApIpAddress()) {
        return false;
    }

    int dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (dns_socket < 0) {
        ESP_LOGW(TAG, "DNS socket create failed: errno %d", errno);
        return false;
    }

    int flags = fcntl(dns_socket, F_GETFL, 0);
    if (flags < 0 || fcntl(dns_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        ESP_LOGW(TAG, "DNS socket non-blocking setup failed: errno %d", errno);
        close(dns_socket);
        return false;
    }

    sockaddr_in bind_addr {};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(kDnsPort);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(dns_socket, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        ESP_LOGW(TAG, "DNS bind failed: errno %d", errno);
        close(dns_socket);
        return false;
    }

    s_dns_socket = dns_socket;
    ESP_LOGI(
        TAG,
        "DNS captive portal active at %u.%u.%u.%u",
        s_ap_ip_bytes[0],
        s_ap_ip_bytes[1],
        s_ap_ip_bytes[2],
        s_ap_ip_bytes[3]
    );
    return true;
}

static void stopDnsServer() {
    if (s_dns_socket < 0) {
        return;
    }

    close(s_dns_socket);
    s_dns_socket = -1;
}

static size_t skipDnsQuestionName(const uint8_t* packet, size_t packet_length, size_t offset) {
    while (offset < packet_length) {
        uint8_t label_length = packet[offset];
        if (label_length == 0) {
            return offset + 1;
        }

        if ((label_length & 0xc0) != 0 || offset + label_length >= packet_length) {
            return 0;
        }

        offset += label_length + 1;
    }

    return 0;
}

static int buildDnsResponse(uint8_t* packet, int packet_length) {
    if (packet == nullptr || packet_length < 12) {
        return -1;
    }

    uint16_t question_count = static_cast<uint16_t>((packet[4] << 8) | packet[5]);
    if (question_count == 0) {
        return -1;
    }

    size_t name_end = skipDnsQuestionName(packet, static_cast<size_t>(packet_length), 12);
    if (name_end == 0 || name_end + 4 > static_cast<size_t>(packet_length)) {
        return -1;
    }

    uint16_t query_type = static_cast<uint16_t>((packet[name_end] << 8) | packet[name_end + 1]);
    uint16_t query_class = static_cast<uint16_t>((packet[name_end + 2] << 8) | packet[name_end + 3]);

    packet[2] |= 0x84;
    packet[3] = 0x00;
    packet[4] = 0x00;
    packet[5] = 0x01;
    packet[8] = 0x00;
    packet[9] = 0x00;
    packet[10] = 0x00;
    packet[11] = 0x00;

    if (query_type != 1 || query_class != 1) {
        packet[6] = 0x00;
        packet[7] = 0x00;
        return static_cast<int>(name_end + 4);
    }

    size_t response_length = name_end + 4;
    if (response_length + 16 > kMaxDnsPacketSize) {
        return -1;
    }

    packet[6] = 0x00;
    packet[7] = 0x01;

    packet[response_length++] = 0xc0;
    packet[response_length++] = 0x0c;
    packet[response_length++] = 0x00;
    packet[response_length++] = 0x01;
    packet[response_length++] = 0x00;
    packet[response_length++] = 0x01;
    packet[response_length++] = 0x00;
    packet[response_length++] = 0x00;
    packet[response_length++] = 0x00;
    packet[response_length++] = 0x00;
    packet[response_length++] = 0x00;
    packet[response_length++] = 0x04;
    packet[response_length++] = s_ap_ip_bytes[0];
    packet[response_length++] = s_ap_ip_bytes[1];
    packet[response_length++] = s_ap_ip_bytes[2];
    packet[response_length++] = s_ap_ip_bytes[3];

    return static_cast<int>(response_length);
}

static void pollDnsServer() {
    if (s_dns_socket < 0) {
        return;
    }

    for (int i = 0; i < 4; i++) {
        uint8_t packet[kMaxDnsPacketSize] = {};
        sockaddr_storage source_addr {};
        socklen_t source_addr_length = sizeof(source_addr);

        int received = recvfrom(
            s_dns_socket,
            packet,
            sizeof(packet),
            0,
            reinterpret_cast<sockaddr*>(&source_addr),
            &source_addr_length
        );
        if (received < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return;
            }

            ESP_LOGW(TAG, "DNS recv failed: errno %d", errno);
            return;
        }

        int response_length = buildDnsResponse(packet, received);
        if (response_length <= 0) {
            continue;
        }

        int sent = sendto(
            s_dns_socket,
            packet,
            response_length,
            0,
            reinterpret_cast<sockaddr*>(&source_addr),
            source_addr_length
        );
        if (sent < 0) {
            ESP_LOGW(TAG, "DNS send failed: errno %d", errno);
            return;
        }
    }
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
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(server, &config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        *server = nullptr;
        return false;
    }

    httpd_uri_t page_handler = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = &handleSetupGetRequest,
        .user_ctx = system_in_queue
    };

    httpd_uri_t save_handler = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = &handleSetupSaveRequest,
        .user_ctx = system_in_queue
    };

    if (httpd_register_uri_handler(*server, &page_handler) != ESP_OK ||
        httpd_register_uri_handler(*server, &save_handler) != ESP_OK ||
        httpd_register_err_handler(*server, HTTPD_404_NOT_FOUND, &handleSetupRedirectError) != ESP_OK) {
        httpd_stop(*server);
        *server = nullptr;
        return false;
    }

    if (!configureCaptivePortalDhcp()) {
        httpd_stop(*server);
        *server = nullptr;
        return false;
    }

    if (!startDnsServer()) {
        httpd_stop(*server);
        *server = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "Setup portal started");
    return true;
}

void stopSetupPortal(httpd_handle_t* server) {
    if (server == nullptr || *server == nullptr) {
        stopDnsServer();
        return;
    }

    stopDnsServer();
    httpd_stop(*server);
    *server = nullptr;
    ESP_LOGI(TAG, "Setup portal stopped");
}

void pollSetupPortal() {
    pollDnsServer();
}
