#pragma once
#include "freertos/FreeRTOS.h"
#include "types.h"

struct DataPacket {
    uint8_t time;
};

struct SettingsPacket {
    TransportMode setting_transport_mode;
    uint8_t setting_direction_code;
    char setting_ssid[30];
    char setting_password[30];
};

enum class NetworkEvent{
    STARTED,
    CONNECTED,
    DISCONNECTED,
    RETRY_TIMER,
    ERROR
};
