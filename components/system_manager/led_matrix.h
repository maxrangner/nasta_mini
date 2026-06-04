#pragma once

#include "driver/gpio.h"
#include "led_strip.h"
#include <stdint.h>

enum class MatrixIcon : uint8_t {
    HEART = 0,
    COLON = 1,
    CROSS = 2,
    OK = 3,
    QUESTION = 4,
    THIRTY_PLUS = 5,
    BLANK = 6
};

class LedMatrix {
public:
    void init();
    void clear();
    void setColor(uint8_t red, uint8_t green, uint8_t blue);
    void displayIcon(MatrixIcon icon);
    void displayDeparture(const char* display_text, uint32_t animation_frame);
    void bootAnimation(uint32_t frame);
    void connectionAnimation(uint32_t frame);

private:
    static constexpr gpio_num_t kLedPin_ = GPIO_NUM_14;
    static constexpr uint16_t kLedCount_ = 64;
    static constexpr uint8_t kMatrixWidth_ = 8;

    led_strip_handle_t led_strip_ = nullptr;
    uint8_t pixel_buffer_[kLedCount_][3] = {};
    bool frame_dirty_ = false;
    uint8_t red_ = 0;
    uint8_t green_ = 16;
    uint8_t blue_ = 0;

    void setPixel(uint16_t index, uint8_t red, uint8_t green, uint8_t blue);
    void refreshIfDirty();
    void drawGraphic(const uint8_t* graphic);
    void scrollGraphics(const uint8_t** graphic_sequence,
        uint8_t num_graphics,
        uint32_t animation_frame,
        uint8_t scroll_speed);
    void displayNumber(uint8_t number);
    void displayClock(const char* time_str, uint32_t animation_frame);
    bool parseMinutes(const char* display_text, uint8_t* minutes) const;
    bool isClockDisplay(const char* display_text) const;
};
