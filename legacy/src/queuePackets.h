#pragma once
#include <stdint.h>
#include "config.h"
#include "apiDataTypes.h"

enum class EventType {
    NO_WIFI,
    NO_DATA,
    NO_API_RESPONSE,
    DATA
};

struct SettingsPacket{
    TransportMode settingTransportMode;
    uint8_t settingDirectionCode;
    char settingSsid[30];
    char settingPassword[30];
};

struct Departure {
    TimeDisplayType displayTimeType;
    uint8_t minutes;
    char clock_time[10];
    // char destination[25];
    // char direction[25];
    uint8_t directionCode;
    // uint8_t line;
    // char state[25];
    // TransportMode transportMode;
    // char expected[25];
};

struct Direction {
    Departure departures[NUM_DEPARTURES];
    uint8_t count = 0;
};

struct QueuePacket{
    EventType type;
    Direction direction[2];
};
