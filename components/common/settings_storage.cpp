#include "settings_storage.h"

#include "esp_log.h"
#include "nvs.h"

static const char* TAG = "settings storage";
static constexpr const char* kSettingsNamespace = "device";
static constexpr const char* kSettingsKey = "settings";

struct DeviceSettingsV1 {
    uint8_t version = 1;
    WifiSettings wifi {};
    SiteSettings site {};
    uint8_t startup_direction = 1;
    SetupSettings setup {};
};

struct DeviceSettingsV2 {
    uint8_t version = 2;
    WifiSettings wifi {};
    SiteSettings site {};
    uint8_t startup_direction = 1;
    SetupSettings setup {};
    uint8_t walk_time_minutes = 0;
};

struct DeviceSettingsV3 {
    uint8_t version = 3;
    WifiSettings wifi {};
    SiteSettings site {};
    uint8_t startup_direction = 1;
    SetupSettings setup {};
    uint8_t walk_time_minutes = 0;
    uint8_t gradient_minutes = kDefaultGradientMinutes;
};

bool loadDeviceSettings(DeviceSettings* settings) {
    if (settings == nullptr) {
        return false;
    }

    *settings = DeviceSettings {};

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kSettingsNamespace, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved device settings");
        return false;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open read failed: %s", esp_err_to_name(err));
        return false;
    }

    size_t size = 0;
    err = nvs_get_blob(handle, kSettingsKey, nullptr, &size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        ESP_LOGI(TAG, "No settings blob saved");
        return false;
    }
    if (err != ESP_OK) {
        nvs_close(handle);
        ESP_LOGW(TAG, "nvs_get_blob failed: %s", esp_err_to_name(err));
        return false;
    }

    DeviceSettings loaded_settings {};

    if (size != sizeof(DeviceSettings) &&
        size != sizeof(DeviceSettingsV1) &&
        size != sizeof(DeviceSettingsV2) &&
        size != sizeof(DeviceSettingsV3)) {
        nvs_close(handle);
        ESP_LOGW(TAG, "Saved settings size mismatch: %u", static_cast<unsigned>(size));
        return false;
    }

    size_t read_size = size;
    err = nvs_get_blob(handle, kSettingsKey, &loaded_settings, &read_size);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_get_blob read failed: %s", esp_err_to_name(err));
        return false;
    }

    if (loaded_settings.version != kDeviceSettingsVersion) {
        if (size == sizeof(DeviceSettingsV1) && loaded_settings.version == 1) {
            *settings = loaded_settings;
            settings->version = kDeviceSettingsVersion;
            settings->walk_time_minutes = 0;
            settings->gradient_minutes = kDefaultGradientMinutes;
            return true;
        }

        if (size == sizeof(DeviceSettingsV2) && loaded_settings.version == 2) {
            *settings = loaded_settings;
            settings->version = kDeviceSettingsVersion;
            settings->gradient_minutes = kDefaultGradientMinutes;
            return true;
        }

        if (size == sizeof(DeviceSettingsV3) && loaded_settings.version == 3) {
            *settings = loaded_settings;
            settings->version = kDeviceSettingsVersion;
            settings->brightness = kDefaultDisplayBrightness;
            return true;
        }

        ESP_LOGW(TAG, "Saved settings version mismatch: %u", loaded_settings.version);
        return false;
    }

    *settings = loaded_settings;
    return true;
}

bool saveDeviceSettings(const DeviceSettings& settings) {
    DeviceSettings settings_to_save = settings;
    settings_to_save.version = kDeviceSettingsVersion;

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kSettingsNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open write failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_blob(handle, kSettingsKey, &settings_to_save, sizeof(settings_to_save));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_set_blob failed: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}
