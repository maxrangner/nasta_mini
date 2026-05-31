#pragma once
#include "settings.h"
#include "types.h"

enum class WifiLinkEvent {
    LINK_DISCONNECTED,
    LINK_CONNECTED_STA,
    LINK_AP_ACTIVE
};

enum class NetworkPacketType {
    WIFI_LINK_EVENT,
    SETUP_CONFIG
};

struct NetworkPacket {
    NetworkPacketType type = NetworkPacketType::WIFI_LINK_EVENT;
    WifiLinkEvent wifi_link_event = WifiLinkEvent::LINK_DISCONNECTED;
    SetupConfig setup_config {};
};

enum class NetworkStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    SETUP,
    NETWORK_ERROR
};

enum class FetchStatus {
    IDLE,
    FRESH,
    STALE,
    API_ERROR
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

inline uint8_t totalDepartureCount(const Departures& departures) {
    uint8_t count = 0;

    for (uint8_t i = 0; i < kMaxDepartureDirections; i++) {
        count += departures.directions[i].count;
    }

    return count;
}

struct NetworkSnapshot {
    NetworkStatus connectivity = NetworkStatus::DISCONNECTED;
    FetchStatus fetch_status = FetchStatus::IDLE;
    Departures departures {};
};
