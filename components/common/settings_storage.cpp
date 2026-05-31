#include "settings_storage.h"

#include "esp_log.h"
#include "nvs.h"

static const char* TAG = "settings storage";
static constexpr const char* kSettingsNamespace = "device";
static constexpr const char* kSettingsKey = "settings";

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

    size_t size = sizeof(DeviceSettings);
    DeviceSettings loaded_settings {};
    err = nvs_get_blob(handle, kSettingsKey, &loaded_settings, &size);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No settings blob saved");
        return false;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_get_blob failed: %s", esp_err_to_name(err));
        return false;
    }
    if (size != sizeof(DeviceSettings)) {
        ESP_LOGW(TAG, "Saved settings size mismatch: %u", static_cast<unsigned>(size));
        return false;
    }
    if (loaded_settings.version != kDeviceSettingsVersion) {
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
