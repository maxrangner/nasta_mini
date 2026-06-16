#include "display.h"

#include "led_matrix.h"

static constexpr uint8_t kDirectionIndicatorFrames_ = 5;
static constexpr uint8_t kPixelBrightness_ = 1;

static LedMatrix matrix_ {};
static DisplayData current_display_data {};
static uint32_t animation_frame_ = 0;
static uint32_t last_direction_change_counter_ = 0;
static uint8_t direction_indicator_frames_left_ = 0;
static bool is_initialized_ = false;

static void renderDisplay();
static void showDirectionIndicator();

void displayInit() {
    if (is_initialized_) {
        return;
    }

    matrix_.init();
    is_initialized_ = true;
}

void displaySetData(const DisplayData& display_data) {
    if (!is_initialized_) {
        return;
    }

    current_display_data = display_data;

    if (current_display_data.direction_change_counter != last_direction_change_counter_) {
        last_direction_change_counter_ = current_display_data.direction_change_counter;
        direction_indicator_frames_left_ = kDirectionIndicatorFrames_;
    }
}

void displayUpdate() {
    if (!is_initialized_) {
        return;
    }

    animation_frame_++;
    renderDisplay();
}

static void renderDisplay() {
    if (direction_indicator_frames_left_ > 0) {
        direction_indicator_frames_left_--;
        showDirectionIndicator();
        return;
    }

    switch (current_display_data.screen) {
        case DisplayScreen::BOOT:
            matrix_.bootAnimation(animation_frame_);
            break;

        case DisplayScreen::CONNECTING:
            matrix_.setColor(0, 0, kPixelBrightness_);
            matrix_.connectionAnimation(animation_frame_);
            break;

        case DisplayScreen::CONNECTED:
            matrix_.setColor(0, kPixelBrightness_, 0);
            matrix_.displayIcon(MatrixIcon::OK);
            break;

        case DisplayScreen::SETUP:
            matrix_.setColor(0, kPixelBrightness_, kPixelBrightness_);
            matrix_.displayIcon(MatrixIcon::HEART);
            break;

        case DisplayScreen::DEPARTURES:
            matrix_.setColor(
                current_display_data.show_stale_data ? kPixelBrightness_ : 0,
                kPixelBrightness_,
                0
            );

            if (!current_display_data.has_departure_for_active_direction) {
                matrix_.displayIcon(MatrixIcon::QUESTION);
                break;
            }

            matrix_.displayDeparture(
                current_display_data.departure_text,
                animation_frame_
            );
            break;

        case DisplayScreen::NO_DEPARTURES:
            matrix_.setColor(kPixelBrightness_, kPixelBrightness_, 0);
            matrix_.displayIcon(MatrixIcon::QUESTION);
            break;

        case DisplayScreen::API_ERROR:
            matrix_.setColor(kPixelBrightness_, 0, 0);
            matrix_.displayIcon(MatrixIcon::QUESTION);
            break;

        case DisplayScreen::NETWORK_ERROR:
            matrix_.setColor(kPixelBrightness_, 0, 0);
            matrix_.displayIcon(MatrixIcon::CROSS);
            break;
    }
}

static void showDirectionIndicator() {
    matrix_.setColor(0, kPixelBrightness_, 0);

    if (current_display_data.active_direction <= 1) {
        matrix_.displayIcon(MatrixIcon::ARROW_LEFT);
        return;
    }

    matrix_.displayIcon(MatrixIcon::ARROW_RIGHT);
}
