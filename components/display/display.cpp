#include "display.h"
#include "led_matrix.h"

#include <ctype.h>
#include <string.h>

static LedMatrix matrix_instance {};

static constexpr uint32_t kDirectionHoldFrames_ = 5;

struct DepartureColor { uint8_t red; uint8_t green; uint8_t blue; };

static uint32_t animationFrameLimit(DisplayAnimation animation) {
    switch (animation) {
        case DisplayAnimation::BOOT:            return LedMatrix::kBootFrameCount;
        case DisplayAnimation::DIRECTION_LEFT:
        case DisplayAnimation::DIRECTION_RIGHT: return kDirectionHoldFrames_;
        case DisplayAnimation::NONE:            return 0;
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
    if (departure_minutes <= walk_time_minutes) return kColors[0];
    uint8_t offset = departure_minutes - walk_time_minutes;
    if (offset >= sizeof(kColors) / sizeof(kColors[0]))
        return kColors[(sizeof(kColors) / sizeof(kColors[0])) - 1];
    return kColors[offset];
}

void Display::init() {
    matrix_ = &matrix_instance;
    matrix_->init();
}

void Display::setState(const DisplayState& state) {
    state_ = state;
}

void Display::playAnimation(DisplayAnimation animation) {
    animation_ = animation;
    anim_frame_ = 0;
}

void Display::update() {
    matrix_->setBrightness(displayBrightnessValue(state_.brightness));
    matrix_->setRotation(state_.rotate_display_180);

    if (animation_ != DisplayAnimation::NONE) {
        switch (animation_) {
            case DisplayAnimation::BOOT:            matrix_->showBootFrame(anim_frame_); break;
            case DisplayAnimation::DIRECTION_LEFT:  matrix_->showDirectionLeft();        break;
            case DisplayAnimation::DIRECTION_RIGHT: matrix_->showDirectionRight();       break;
            case DisplayAnimation::NONE:            break;
        }
        anim_frame_++;
        if (anim_frame_ >= animationFrameLimit(animation_)) {
            animation_ = DisplayAnimation::NONE;
            anim_frame_ = 0;
        }
    } else {
        renderState();
    }

    frame_++;
}

void Display::renderState() {
    switch (state_.system_state) {
        case SystemState::BOOT:           matrix_->clear();                  break;
        case SystemState::CONNECTING:     matrix_->showConnecting(frame_);   break;
        case SystemState::CONNECTED:      matrix_->showConnected();          break;
        case SystemState::SETUP:          matrix_->showSetup(frame_);        break;
        case SystemState::NO_DEPARTURES:  matrix_->showNoDepartures();       break;
        case SystemState::API_ERROR:      matrix_->showApiError();           break;
        case SystemState::NETWORK_ERROR:  matrix_->showNetworkError();       break;
        case SystemState::DEPARTURES:     showDeparture();                  break;
    }
}

void Display::showDeparture() {
    const char* text = state_.departure_text;
    uint8_t brightness = displayBrightnessValue(state_.brightness);

    if (text[0] == '\0') {
        matrix_->showDepartureUnknown();
        return;
    }

    if (looksLikeClockTime(text)) {
        matrix_->showDepartureClock(text, frame_, 0, brightness, 0);
        return;
    }

    while (*text == ' ') text++;

    uint8_t minutes = 0;

    if (strcmp(text, "Nu") == 0 || strcmp(text, "NU") == 0) {
        minutes = 0;
    } else if (!isdigit(static_cast<unsigned char>(*text))) {
        matrix_->showDepartureUnknown();
        return;
    } else {
        uint16_t parsed_minutes = 0;
        while (isdigit(static_cast<unsigned char>(*text))) {
            parsed_minutes = parsed_minutes * 10 + static_cast<uint8_t>(*text - '0');
            if (parsed_minutes >= 255) {
                parsed_minutes = 255;
                break;
            }
            text++;
        }
        minutes = static_cast<uint8_t>(parsed_minutes);
    }

    DepartureColor color = departureColorForMinutes(minutes, state_.walk_time_minutes);
    matrix_->showDepartureMinutes(minutes, color.red, color.green, color.blue);
}
