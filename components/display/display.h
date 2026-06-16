#pragma once

#include <stdint.h>
#include "message_types.h"

enum class DisplayScreen : uint8_t {
    BOOT,
    CONNECTING,
    CONNECTED,
    SETUP,
    DEPARTURES,
    NO_DEPARTURES,
    API_ERROR,
    NETWORK_ERROR
};

struct DisplayData {
    DisplayScreen screen = DisplayScreen::BOOT;
    uint8_t active_direction = 1;
    bool has_departure_for_active_direction = false;
    bool show_stale_data = false;
    uint32_t direction_change_counter = 0;
    char departure_text[sizeof(Departure {}.display)] = {};
};

void displayInit();
void displaySetData(const DisplayData& display_data);
void displayUpdate();
