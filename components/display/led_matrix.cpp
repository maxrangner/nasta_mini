#include "led_matrix.h"

#include "esp_err.h"
#include "matrix_animations.h"
#include "matrix_graphics.h"
#include "soc/soc_caps.h"
#include <string.h>

namespace {

struct RgbColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

static constexpr uint8_t kCenterRings[64] = {
    6,5,4,3,3,4,5,6,
    5,4,3,2,2,3,4,5,
    4,3,2,1,1,2,3,4,
    3,2,1,0,0,1,2,3,
    3,2,1,0,0,1,2,3,
    4,3,2,1,1,2,3,4,
    5,4,3,2,2,3,4,5,
    6,5,4,3,3,4,5,6
};

static constexpr uint8_t kSetupLevels[7] = { 0, 1, 2, 4, 6, 8, 10 };

static uint8_t lerpChannel(uint8_t from, uint8_t to, uint8_t blend) {
    const uint16_t start = static_cast<uint16_t>(from) * (255 - blend);
    const uint16_t end = static_cast<uint16_t>(to) * blend;
    return static_cast<uint8_t>((start + end) / 255);
}

static RgbColor rainbowColor(uint8_t brightness, uint8_t hue) {
    const RgbColor kStops[6] = {
        {brightness, 0, 0},
        {brightness, brightness, 0},
        {0, brightness, 0},
        {0, brightness, brightness},
        {0, 0, brightness},
        {brightness, 0, brightness},
    };

    const uint16_t scaled = static_cast<uint16_t>(hue) * 6;
    const uint8_t segment = (scaled / 256) % 6;
    const uint8_t blend = scaled % 256;
    const RgbColor from = kStops[segment];
    const RgbColor to = kStops[(segment + 1) % 6];

    return {
        lerpChannel(from.red, to.red, blend),
        lerpChannel(from.green, to.green, blend),
        lerpChannel(from.blue, to.blue, blend),
    };
}

}

void LedMatrix::init() {
    led_strip_config_t strip_config = {
        .strip_gpio_num = kLedPin_,
        .max_leds = kLedCount_,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
        .flags = { .invert_out = false },
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 256,
        .flags = {
#if SOC_RMT_SUPPORT_DMA
            .with_dma = true,
#else
            .with_dma = false,
#endif
        },
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
    ESP_ERROR_CHECK(led_strip_clear(led_strip_));
    memset(pixel_buffer_, 0, sizeof(pixel_buffer_));
    frame_dirty_ = false;
}

void LedMatrix::setBrightness(uint8_t brightness) {
    brightness_ = brightness;
}

void LedMatrix::setRotation(bool rotated_180) {
    rotated_180_ = rotated_180;
}

void LedMatrix::clear() {
    if (led_strip_ == nullptr) return;
    ESP_ERROR_CHECK(led_strip_clear(led_strip_));
    memset(pixel_buffer_, 0, sizeof(pixel_buffer_));
    frame_dirty_ = false;
}

void LedMatrix::showBootFrame(uint32_t frame) {
    setColor(brightness_, brightness_, brightness_);
    drawGraphic(kBootAnimation[frame % kBootFrameCount]);
}

void LedMatrix::showConnecting(uint32_t frame) {
    const uint8_t hue = static_cast<uint8_t>(frame * 9);
    const RgbColor color = rainbowColor(brightness_, hue);

    setColor(color.red, color.green, color.blue);
    drawGraphic(kWorkAnimation[frame % 4]);
}

void LedMatrix::showConnected() {
    setColor(0, brightness_, 0);
    drawGraphic(kOk);
}

// void LedMatrix::showSetup() {
//     setColor(0, brightness_, 0);
//     drawGraphic(kHeart);
// }

void LedMatrix::showSetup(uint32_t frame) {
    if (led_strip_ == nullptr) return;

    const uint8_t phase = (frame / 1) % 7;

    for (uint16_t i = 0; i < kLedCount_; i++) {
        const uint8_t level = kSetupLevels[(kCenterRings[i] + phase) % 7];
        setPixel(i, level, level, level);
    }

    refreshIfDirty();
}

void LedMatrix::showDepartureMinutes(uint8_t minutes, uint8_t r, uint8_t g, uint8_t b) {
    setColor(r, g, b);
    displayNumber(minutes);
}

void LedMatrix::showDepartureClock(const char* time_str, uint32_t frame, uint8_t r, uint8_t g, uint8_t b) {
    setColor(r, g, b);

    if (time_str == nullptr) {
        drawGraphic(kQuestion);
        return;
    }

    displayClock(time_str, frame);
}

void LedMatrix::showDepartureUnknown() {
    setColor(brightness_, brightness_, 0);
    drawGraphic(kQuestion);
}

void LedMatrix::showNoDepartures() {
    setColor(brightness_, brightness_, 0);
    drawGraphic(kQuestion);
}

void LedMatrix::showApiError() {
    setColor(brightness_, 0, 0);
    drawGraphic(kQuestion);
}

void LedMatrix::showNetworkError() {
    setColor(brightness_, 0, 0);
    drawGraphic(kCross);
}

void LedMatrix::showDirectionLeft() {
    setColor(brightness_, brightness_, brightness_);
    drawGraphic(kArrowLeft);
}

void LedMatrix::showDirectionRight() {
    setColor(brightness_, brightness_, brightness_);
    drawGraphic(kArrowRight);
}

void LedMatrix::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (rotated_180_) index = 63 - index;
    if (pixel_buffer_[index][0] == r &&
        pixel_buffer_[index][1] == g &&
        pixel_buffer_[index][2] == b) {
        return;
    }
    pixel_buffer_[index][0] = r;
    pixel_buffer_[index][1] = g;
    pixel_buffer_[index][2] = b;
    frame_dirty_ = true;
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip_, index, r, g, b));
}

void LedMatrix::refreshIfDirty() {
    if (led_strip_ == nullptr || !frame_dirty_) return;
    ESP_ERROR_CHECK(led_strip_refresh(led_strip_));
    frame_dirty_ = false;
}

void LedMatrix::setColor(uint8_t r, uint8_t g, uint8_t b) {
    red_   = r;
    green_ = g;
    blue_  = b;
}

void LedMatrix::drawGraphic(const uint8_t* graphic) {
    if (led_strip_ == nullptr || graphic == nullptr) return;
    for (uint16_t i = 0; i < kLedCount_; i++) {
        if (graphic[i] == 1) {
            setPixel(i, red_, green_, blue_);
        } else {
            setPixel(i, 0, 0, 0);
        }
    }
    refreshIfDirty();
}

void LedMatrix::scrollGraphics(const uint8_t** graphics, uint8_t count, uint32_t frame, uint8_t speed) {
    if (led_strip_ == nullptr || graphics == nullptr || count == 0) return;

    const int8_t  spacing      = -2;
    const uint8_t col_offset   = 1;
    const uint8_t graphic_width = 8;
    uint32_t      scroll_offset = (frame * speed) / 10;

    for (uint8_t row = 0; row < kMatrixWidth_; row++) {
        for (uint8_t col = 0; col < kMatrixWidth_; col++) {
            uint32_t logical_col = col + scroll_offset;
            uint8_t  gi          = (logical_col / (graphic_width + spacing)) % count;
            uint8_t  gc          = (logical_col % (graphic_width + spacing)) + col_offset;
            uint8_t  pixel       = graphics[gi][row * graphic_width + gc];
            uint16_t led         = row * kMatrixWidth_ + col;
            if (pixel == 1) {
                setPixel(led, red_, green_, blue_);
            } else {
                setPixel(led, 0, 0, 0);
            }
        }
    }
    refreshIfDirty();
}

void LedMatrix::displayNumber(uint8_t number) {
    if (number > 30) {
        drawGraphic(kThirtyPlus);
        return;
    }
    drawGraphic(kNumbers[number]);
}

void LedMatrix::displayClock(const char* time_str, uint32_t frame) {
    const uint8_t* sequence[7] = {
        kNumbers[time_str[0] - '0'],
        kNumbers[time_str[1] - '0'],
        kColon,
        kNumbers[time_str[3] - '0'],
        kNumbers[time_str[4] - '0'],
        kBlank,
        kBlank,
    };
    scrollGraphics(sequence, 7, frame, 3);
}
