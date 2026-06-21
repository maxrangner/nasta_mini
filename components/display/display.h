#pragma once

#include <stdint.h>
#include "message_types.h"

class LedMatrix;

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
    DisplayBrightness brightness = kDefaultDisplayBrightness;
    bool rotate_display_180 = false;
};

class Display {
public:
    void init();
    void setState(const DisplayState& state);
    void playAnimation(DisplayAnimation animation);
    void update();

private:
    LedMatrix* matrix_ = nullptr;
    DisplayState state_ {};
    DisplayAnimation animation_ = DisplayAnimation::NONE;
    uint32_t frame_ = 0;
    uint32_t anim_frame_ = 0;

    void renderState();
    void showDeparture();
};
