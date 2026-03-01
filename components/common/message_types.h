#pragma once
#include "freertos/FreeRTOS.h"
#include "types.h"

enum class PacketType {
    WIFI_EVENT,
    NETWORK_EVENT,
    API_DATA,
    SETTINGS_DATA
};

enum class NetworkEvent{
    STARTED,
    CONNECTED,
    DISCONNECTED,
    RETRY_TIMER,
    ERROR
};

struct DataPacket {
    PacketType type;
    NetworkEvent network_event;
};

struct Departure {
    TimeDisplayType time_display;
    TransportMode transport;
    uint8_t direction;
    uint8_t line;
    uint8_t min_to_departure;
    char clock_next_departure[6];
};
