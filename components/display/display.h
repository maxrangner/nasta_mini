#pragma once

#include <stdint.h>
#include "message_types.h"

enum class DisplayAnimation : uint8_t {
    NONE,
    BOOT,
    DIRECTION_LEFT,
    DIRECTION_RIGHT
};

struct DisplayState {
    SystemState system_state = SystemState::BOOT;
    char departure_text[sizeof(Departure {}.display)] = {};
    uint8_t walk_time_minutes = 0;
    uint8_t gradient_minutes = kDefaultGradientMinutes;
    DisplayBrightness brightness = kDefaultDisplayBrightness;
};

void displayInit();
void displaySetState(const DisplayState& state);
void displayPlayAnimation(DisplayAnimation animation);
void displayUpdate();
