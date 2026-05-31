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
    API_ERROR
};

enum class NetworkStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    SETUP,
    NETWORK_ERROR
};

static constexpr uint8_t kMaxDepartureDirections = 2;
static constexpr uint8_t kMaxDeparturesPerDirection = 3;

struct Departure {
    char destination[20];
    char display[10];
    TransportMode transport_mode;
    uint8_t line;
};

struct DirectionDepartures {
    Departure departures[kMaxDeparturesPerDirection];
    uint8_t count;
};

struct Departures {
    DirectionDepartures directions[kMaxDepartureDirections];
};

struct SystemPacket {
    SystemPacketType type;
    union {
        NetworkStatus network_status;
        Departures departures;
    };
};
