#include "led_matrix.h"

#include "esp_err.h"
#include "matrix_animations.h"
#include "matrix_graphics.h"
#include "soc/soc_caps.h"
#include <string.h>

static uint8_t bootAnimationLevel(uint8_t brightness, uint8_t step) {
    uint8_t level = static_cast<uint8_t>(step * 2);
    return level < brightness ? level : brightness;
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

void LedMatrix::clear() {
    if (led_strip_ == nullptr) return;
    ESP_ERROR_CHECK(led_strip_clear(led_strip_));
    memset(pixel_buffer_, 0, sizeof(pixel_buffer_));
    frame_dirty_ = false;
}

void LedMatrix::showBootFrame(uint32_t frame) {
    if (led_strip_ == nullptr) return;
    const uint8_t* graphic = kBootAnimation[frame % kBootFrameCount];
    uint8_t r = bootAnimationLevel(brightness_, static_cast<uint8_t>(frame % 6));
    uint8_t g = bootAnimationLevel(brightness_, static_cast<uint8_t>((frame + 2) % 6));
    uint8_t b = bootAnimationLevel(brightness_, static_cast<uint8_t>((frame + 4) % 6));
    for (uint16_t i = 0; i < kLedCount_; i++) {
        if (graphic[i] == 1) {
            setPixel(i, r, g, b);
        } else {
            setPixel(i, 0, 0, 0);
        }
    }
    refreshIfDirty();
}

void LedMatrix::showConnecting(uint32_t frame) {
    setColor(0, 0, brightness_);
    drawGraphic(kWorkAnimation[frame % 4]);
}

void LedMatrix::showConnected() {
    setColor(0, brightness_, 0);
    drawGraphic(kOk);
}

void LedMatrix::showSetup() {
    setColor(0, brightness_, brightness_);
    drawGraphic(kHeart);
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
    setColor(0, brightness_, 0);
    drawGraphic(kArrowLeft);
}

void LedMatrix::showDirectionRight() {
    setColor(0, brightness_, 0);
    drawGraphic(kArrowRight);
}

void LedMatrix::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
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
