#include "display.h"
#include "led_matrix.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static constexpr uint32_t kDirectionHoldFrames_ = 5;

static LedMatrix matrix_ {};
static DisplayState current_state_ {};
static DisplayAnimation active_animation_ = DisplayAnimation::NONE;
static uint32_t frame_      = 0;
static uint32_t anim_frame_ = 0;
static bool initialized_    = false;

static void renderState();
static void renderDeparture();
static void renderAnimation();
static void advanceAnimation();
static uint32_t animationFrameLimit(DisplayAnimation animation);
static bool isClockDisplay(const char* text);
static bool parseMinutes(const char* text, uint8_t* minutes);

void displayInit() {
    if (initialized_) return;
    matrix_.init();
    initialized_ = true;
}

void displaySetState(const DisplayState& state) {
    if (!initialized_) return;
    current_state_ = state;
}

void displayPlayAnimation(DisplayAnimation animation) {
    if (!initialized_) return;

    active_animation_ = animation;
    anim_frame_       = 0;
}

void displayUpdate() {
    if (!initialized_) return;

    if (active_animation_ != DisplayAnimation::NONE) {
        renderAnimation();
    } else {
        renderState();
    }

    frame_++;
    advanceAnimation();
}

static void renderState() {
    switch (current_state_.system_state) {
        case SystemState::BOOT:           matrix_.clear();                                  break;
        case SystemState::CONNECTING:     matrix_.showConnecting(frame_);                   break;
        case SystemState::CONNECTED:      matrix_.showConnected();                          break;
        case SystemState::SETUP:          matrix_.showSetup();                              break;
        case SystemState::NO_DEPARTURES:  matrix_.showNoDepartures();                       break;
        case SystemState::API_ERROR:      matrix_.showApiError();                           break;
        case SystemState::NETWORK_ERROR:  matrix_.showNetworkError();                       break;
        case SystemState::DEPARTURES:     renderDeparture();                                break;
    }
}

static void renderDeparture() {
    if (current_state_.departure_text[0] == '\0') {
        matrix_.showDepartureUnknown();
        return;
    }

    uint8_t minutes = 0;

    if (isClockDisplay(current_state_.departure_text)) {
        matrix_.showDepartureClock(current_state_.departure_text, frame_);
        return;
    }

    if (parseMinutes(current_state_.departure_text, &minutes)) {
        matrix_.showDepartureMinutes(minutes);
        return;
    }

    matrix_.showDepartureUnknown();
}

static void renderAnimation() {
    switch (active_animation_) {
        case DisplayAnimation::BOOT:            matrix_.showBootFrame(anim_frame_);   break;
        case DisplayAnimation::DIRECTION_LEFT:  matrix_.showDirectionLeft();          break;
        case DisplayAnimation::DIRECTION_RIGHT: matrix_.showDirectionRight();         break;
        case DisplayAnimation::NONE:            break;
    }
}

static void advanceAnimation() {
    if (active_animation_ == DisplayAnimation::NONE) return;

    anim_frame_++;

    if (anim_frame_ < animationFrameLimit(active_animation_)) return;

    active_animation_ = DisplayAnimation::NONE;
    anim_frame_ = 0;
}

static uint32_t animationFrameLimit(DisplayAnimation animation) {
    switch (animation) {
        case DisplayAnimation::BOOT:             return LedMatrix::kBootFrameCount;
        case DisplayAnimation::DIRECTION_LEFT:
        case DisplayAnimation::DIRECTION_RIGHT:  return kDirectionHoldFrames_;
        case DisplayAnimation::NONE:             return 0;
    }

    return 0;
}

static bool isClockDisplay(const char* text) {
    if (text == nullptr || strlen(text) != 5) return false;

    return isdigit(static_cast<unsigned char>(text[0])) &&
           isdigit(static_cast<unsigned char>(text[1])) &&
           text[2] == ':' &&
           isdigit(static_cast<unsigned char>(text[3])) &&
           isdigit(static_cast<unsigned char>(text[4]));
}

static bool parseMinutes(const char* text, uint8_t* minutes) {
    if (text == nullptr || minutes == nullptr) return false;

    while (*text == ' ') text++;

    if (strcmp(text, "Nu")  == 0 || strcmp(text, "NU")  == 0 ||
        strcmp(text, "Now") == 0 || strcmp(text, "NOW") == 0) {
        *minutes = 0;
        return true;
    }

    char* end = nullptr;
    unsigned long value = strtoul(text, &end, 10);
    if (end == text) return false;

    *minutes = static_cast<uint8_t>(value > 255 ? 255 : value);
    return true;
}
