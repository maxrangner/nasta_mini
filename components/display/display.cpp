#include "display.h"
#include "led_matrix.h"

#include <ctype.h>
#include <string.h>

static constexpr uint32_t kDirectionHoldFrames_ = 5;

static LedMatrix matrix_ {};
static DisplayState current_state_ {};
static DisplayAnimation active_animation_ = DisplayAnimation::NONE;
static uint32_t frame_ = 0;
static uint32_t anim_frame_ = 0;
static bool initialized_ = false;

static void renderState();
static void showDeparture();
static void renderAnimation();
static void advanceAnimation();
static uint32_t animationFrameLimit(DisplayAnimation animation);
static bool looksLikeClockTime(const char* text);

struct DepartureColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

static DepartureColor departureColorForMinutes(uint8_t departure_minutes, uint8_t walk_time_minutes);

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
    anim_frame_ = 0;
}

void displayUpdate() {
    if (!initialized_) return;

    uint8_t brightness = displayBrightnessValue(current_state_.brightness);
    matrix_.setBrightness(brightness);
    matrix_.setRotation(current_state_.rotate_display_180);

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
        case SystemState::SETUP:          matrix_.showSetup(frame_);                        break;
        case SystemState::NO_DEPARTURES:  matrix_.showNoDepartures();                       break;
        case SystemState::API_ERROR:      matrix_.showApiError();                           break;
        case SystemState::NETWORK_ERROR:  matrix_.showNetworkError();                       break;
        case SystemState::DEPARTURES:     showDeparture();                                  break;
    }
}

static void showDeparture() {
    const char* departure_text = current_state_.departure_text;
    uint8_t brightness = displayBrightnessValue(current_state_.brightness);

    if (departure_text[0] == '\0') {
        matrix_.showDepartureUnknown();
        return;
    }

    if (looksLikeClockTime(departure_text)) {
        matrix_.showDepartureClock(
            departure_text,
            frame_,
            0,
            brightness,
            0
        );
        return;
    }

    while (*departure_text == ' ') departure_text++;

    uint8_t departure_minutes = 0;

    if (strcmp(departure_text, "Nu") == 0 || strcmp(departure_text, "NU") == 0) {
        departure_minutes = 0;
    } else if (!isdigit(static_cast<unsigned char>(*departure_text))) {
        matrix_.showDepartureUnknown();
        return;
    } else {
        while (isdigit(static_cast<unsigned char>(*departure_text))) {
            departure_minutes = departure_minutes * 10 + (*departure_text - '0');
            if (departure_minutes >= 255) {
                departure_minutes = 255;
                break;
            }
            departure_text++;
        }
    }

    uint8_t minutes = departure_minutes;
    DepartureColor color = departureColorForMinutes(minutes, current_state_.walk_time_minutes);

    matrix_.showDepartureMinutes(
        minutes,
        color.red,
        color.green,
        color.blue
    );
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
        case DisplayAnimation::BOOT: return LedMatrix::kBootFrameCount;
        case DisplayAnimation::DIRECTION_LEFT:
        case DisplayAnimation::DIRECTION_RIGHT: return kDirectionHoldFrames_;
        case DisplayAnimation::NONE: return 0;
    }

    return 0;
}

static bool looksLikeClockTime(const char* text) {
    if (text == nullptr || strlen(text) != 5) return false;

    return isdigit(static_cast<unsigned char>(text[0])) &&
           isdigit(static_cast<unsigned char>(text[1])) &&
           text[2] == ':' &&
           isdigit(static_cast<unsigned char>(text[3])) &&
           isdigit(static_cast<unsigned char>(text[4]));
}

static DepartureColor departureColorForMinutes(uint8_t departure_minutes, uint8_t walk_time_minutes) {
    static constexpr DepartureColor kColors[] = {
        {5, 0, 0},
        {5, 1, 0},
        {4, 2, 0},
        {1, 5, 0},
        {0, 5, 0},
        {5, 5, 5},
    };

    if (departure_minutes <= walk_time_minutes) {
        return kColors[0];
    }

    uint8_t offset = departure_minutes - walk_time_minutes;
    if (offset >= sizeof(kColors) / sizeof(kColors[0])) {
        return kColors[(sizeof(kColors) / sizeof(kColors[0])) - 1];
    }

    return kColors[offset];
}
