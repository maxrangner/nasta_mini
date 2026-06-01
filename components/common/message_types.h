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
    SETUP_CONFIG,
    START_SETUP_MODE
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

enum class SystemState {
    BOOT,
    CONNECTING,
    CONNECTED,
    NO_CONNECTION,
    SETUP,
    DEPARTURES,
    NO_DEPARTURES,
    API_ERROR,
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

enum class SystemInputEvent {
    TOGGLE_DIRECTION,
    FORCE_SETUP
};

enum class SystemMessageType {
    NETWORK_STATE,
    INPUT_EVENT
};

struct SystemMessage {
    SystemMessageType type = SystemMessageType::NETWORK_STATE;
    NetworkSnapshot network_state {};
    SystemInputEvent input_event = SystemInputEvent::TOGGLE_DIRECTION;
};

struct RenderState {
    SystemState system_state = SystemState::BOOT;
    uint8_t selected_direction = 1;
    bool stale_data = false;
    DirectionDepartures active_departures {};
};
