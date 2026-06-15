#pragma once
#include <stdint.h>
#include "settings.h"

enum class WifiLinkEvent {
    LINK_DISCONNECTED,
    LINK_CONNECTED_STA,
    LINK_AP_ACTIVE
};

enum class NetworkCommandType {
    WIFI_LINK_EVENT,
    START_NORMAL_MODE,
    START_SETUP_MODE
};

struct NetworkCommand {
    NetworkCommandType type = NetworkCommandType::WIFI_LINK_EVENT;
    WifiLinkEvent wifi_link_event = WifiLinkEvent::LINK_DISCONNECTED;
    DeviceSettings settings {};
};

enum class NetworkPhase : uint8_t {
    CONNECTING,
    SETUP,
    READY,
    ERROR
};

enum class DepartureState : uint8_t {
    NONE,
    READY,
    API_ERROR
};

enum class SystemState {
    BOOT,
    CONNECTING,
    CONNECTED,
    SETUP,
    DEPARTURES,
    NO_DEPARTURES,
    API_ERROR,
    NETWORK_ERROR
};

static constexpr uint8_t kMaxDepartureDirections = 2;
static constexpr uint8_t kMaxDeparturesPerDirection = 3;

struct Departure {
    char display[10];
};

struct DirectionDepartures {
    Departure departures[kMaxDeparturesPerDirection];
    uint8_t count;
};

struct Departures {
    DirectionDepartures directions[kMaxDepartureDirections];
};

struct NetworkState {
    NetworkPhase phase = NetworkPhase::CONNECTING;
    DepartureState departure_state = DepartureState::NONE;
    bool stale_data = false;
    Departures departures {};
};

enum class SystemInputEvent : uint8_t {
    TOGGLE_DIRECTION,
    FORCE_SETUP
};

enum class SystemEventType {
    NETWORK_STATE,
    INPUT_EVENT,
    SETUP_CONFIG
};

struct SystemEvent {
    SystemEventType type = SystemEventType::NETWORK_STATE;
    NetworkState network_state {};
    SystemInputEvent input_event = SystemInputEvent::TOGGLE_DIRECTION;
    SetupConfig setup_config {};
};
