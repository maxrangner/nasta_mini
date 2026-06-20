#pragma once

#include "driver/gpio.h"
#include "led_strip.h"
#include "settings.h"
#include <stdint.h>

class LedMatrix {
public:
    static constexpr uint32_t kBootFrameCount = 8;

    void init();
    void setBrightness(uint8_t brightness);
    void clear();
    void showBootFrame(uint32_t frame);
    void showConnecting(uint32_t frame);
    void showConnected();
    void showSetup();
    void showDepartureMinutes(uint8_t minutes, uint8_t r, uint8_t g, uint8_t b);
    void showDepartureClock(const char* time_str, uint32_t frame, uint8_t r, uint8_t g, uint8_t b);
    void showDepartureUnknown();
    void showNoDepartures();
    void showApiError();
    void showNetworkError();
    void showDirectionLeft();
    void showDirectionRight();

private:
    static constexpr gpio_num_t kLedPin_     = GPIO_NUM_14;
    static constexpr uint16_t   kLedCount_   = 64;
    static constexpr uint8_t    kMatrixWidth_ = 8;

    led_strip_handle_t led_strip_ = nullptr;
    uint8_t pixel_buffer_[kLedCount_][3] = {};
    bool frame_dirty_ = false;
    uint8_t brightness_ = kDisplayBrightnessHighValue;
    uint8_t red_ = 0;
    uint8_t green_ = kDisplayBrightnessHighValue;
    uint8_t blue_ = 0;

    void setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
    void refreshIfDirty();
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void drawGraphic(const uint8_t* graphic);
    void scrollGraphics(const uint8_t** graphics, uint8_t count, uint32_t frame, uint8_t speed);
    void displayNumber(uint8_t number);
    void displayClock(const char* time_str, uint32_t frame);
};
