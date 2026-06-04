#pragma once

#include <stdint.h>
#include "types.h"

static constexpr uint8_t kDeviceSettingsVersion = 1;
static constexpr uint8_t kMaxWifiSsidLength = 32;
static constexpr uint8_t kMaxWifiPasswordLength = 64;

struct WifiSettings {
    char ssid[kMaxWifiSsidLength + 1] = {};
    char password[kMaxWifiPasswordLength + 1] = {};
};

struct SiteSettings {
    uint32_t site_id = 0;
    TransportMode transport_filter = TransportMode::UNKNOWN;
};

struct SetupSettings {
    bool needs_setup = true;
};

struct DeviceSettings {
    uint8_t version = kDeviceSettingsVersion;
    WifiSettings wifi {};
    SiteSettings site {};
    uint8_t startup_direction = 1;
    SetupSettings setup {};
};

struct SetupConfig {
    WifiSettings wifi {};
    SiteSettings site {};
    uint8_t startup_direction = 1;
};

inline bool isValidSetupConfig(const SetupConfig& config) {
    if (config.wifi.ssid[0] == '\0') {
        return false;
    }

    if (config.site.site_id == 0) {
        return false;
    }

    if (config.startup_direction != 1 &&
        config.startup_direction != 2) {
        return false;
    }

    return true;
}

inline void applySetupConfig(DeviceSettings& settings, const SetupConfig& config) {
    settings.wifi = config.wifi;
    settings.site = config.site;
    settings.startup_direction = config.startup_direction;
    settings.setup.needs_setup = false;
}

enum class BootMode : uint8_t {
    NORMAL,
    SETUP
};

inline BootMode decideBootMode(const DeviceSettings& settings) {
    if (settings.setup.needs_setup) {
        return BootMode::SETUP;
    }

    if (settings.wifi.ssid[0] == '\0') {
        return BootMode::SETUP;
    }

    if (settings.site.site_id == 0) {
        return BootMode::SETUP;
    }

    return BootMode::NORMAL;
}
