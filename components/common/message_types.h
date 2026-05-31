#pragma once
#include "freertos/FreeRTOS.h"
#include "types.h"

enum class WifiLinkEvent {
    LINK_DISCONNECTED,
    LINK_CONNECTING_STA,
    LINK_CONNECTED_STA,
    LINK_AP_ACTIVE,
    LINK_ERROR
};

struct NetworkPacket {
    WifiLinkEvent wifi_link_event;
};

enum class SystemPacketType {
    NETWORK_STATUS,
    DEPARTURES_DATA,
    DATA_ERROR
};

enum class NetworkStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    SETUP,
    ERROR
};

struct Departure {
    char destination[20];
    uint8_t direction_code;
    char display[10];
    TransportMode transport_mode;
    uint8_t line;
};

struct Departures {
    Departure departures_dir_1[3];
    Departure departures_dir_2[3];
    uint8_t num_direction_1;
    uint8_t num_direction_2;
};

struct SystemPacket {
    SystemPacketType type;
    union {
        NetworkStatus network_status;
        Departures departures;
    };
};
