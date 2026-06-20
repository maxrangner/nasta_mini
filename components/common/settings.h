#pragma once

#include <stdint.h>
#include "types.h"

static constexpr uint8_t kDeviceSettingsVersion = 7;
static constexpr uint8_t kMaxWifiSsidLength = 32;
static constexpr uint8_t kMaxWifiPasswordLength = 64;

enum class DisplayBrightness : uint8_t {
    LOW = 0,
    HIGH = 1
};

static constexpr DisplayBrightness kDefaultDisplayBrightness = DisplayBrightness::HIGH;
static constexpr uint8_t kDisplayBrightnessLowValue = 1;
static constexpr uint8_t kDisplayBrightnessHighValue = 5;

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
    uint8_t walk_time_minutes = 0;
    DisplayBrightness brightness = kDefaultDisplayBrightness;
    bool flip_direction_arrows = false;
    bool rotate_display_180 = false;
};

struct SetupConfig {
    WifiSettings wifi {};
    SiteSettings site {};
    uint8_t startup_direction = 1;
    uint8_t walk_time_minutes = 0;
    DisplayBrightness brightness = kDefaultDisplayBrightness;
    bool flip_direction_arrows = false;
    bool rotate_display_180 = false;
};

inline bool isValidDisplayBrightness(DisplayBrightness brightness) {
    return brightness == DisplayBrightness::LOW ||
           brightness == DisplayBrightness::HIGH;
}

inline uint8_t displayBrightnessValue(DisplayBrightness brightness) {
    return brightness == DisplayBrightness::LOW
        ? kDisplayBrightnessLowValue
        : kDisplayBrightnessHighValue;
}

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

    if (!isValidDisplayBrightness(config.brightness)) {
        return false;
    }

    return true;
}

inline void applySetupConfig(DeviceSettings& settings, const SetupConfig& config) {
    settings.wifi = config.wifi;
    settings.site = config.site;
    settings.startup_direction = config.startup_direction;
    settings.setup.needs_setup = false;
    settings.walk_time_minutes = config.walk_time_minutes;
    settings.brightness = config.brightness;
    settings.flip_direction_arrows = config.flip_direction_arrows;
    settings.rotate_display_180 = config.rotate_display_180;
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
