#include "led_matrix.h"

#include "esp_err.h"
#include "matrix_animations.h"
#include "matrix_graphics.h"
#include "soc/soc_caps.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void LedMatrix::init() {
    led_strip_config_t strip_config = {
        .strip_gpio_num = kLedPin_,
        .max_leds = kLedCount_,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
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

void LedMatrix::clear() {
    if (led_strip_ == nullptr) {
        return;
    }

    ESP_ERROR_CHECK(led_strip_clear(led_strip_));
    memset(pixel_buffer_, 0, sizeof(pixel_buffer_));
    frame_dirty_ = false;
}

void LedMatrix::setColor(uint8_t red, uint8_t green, uint8_t blue) {
    red_ = red;
    green_ = green;
    blue_ = blue;
}

void LedMatrix::displayIcon(MatrixIcon icon) {
    drawGraphic(kIcons[static_cast<uint8_t>(icon)]);
}

void LedMatrix::displayDeparture(const char* display_text, uint32_t animation_frame) {
    uint8_t minutes = 0;

    if (display_text == nullptr || display_text[0] == '\0') {
        displayIcon(MatrixIcon::QUESTION);
        return;
    }

    if (isClockDisplay(display_text)) {
        displayClock(display_text, animation_frame);
        return;
    }

    if (parseMinutes(display_text, &minutes)) {
        displayNumber(minutes);
        return;
    }

    displayIcon(MatrixIcon::QUESTION);
}

void LedMatrix::bootAnimation(uint32_t frame) {
    if (led_strip_ == nullptr) {
        return;
    }

    const uint8_t* graphic = kBootAnimation[frame % 8];
    uint8_t red = static_cast<uint8_t>((frame % 6) * 2);
    uint8_t green = static_cast<uint8_t>(((frame + 2) % 6) * 2);
    uint8_t blue = static_cast<uint8_t>(((frame + 4) % 6) * 2);

    for (uint16_t i = 0; i < kLedCount_; i++) {
        if (graphic[i] == 1) {
            setPixel(i, red, green, blue);
        } else {
            setPixel(i, 0, 0, 0);
        }
    }

    refreshIfDirty();
}

void LedMatrix::connectionAnimation(uint32_t frame) {
    drawGraphic(kWorkAnimation[frame % 4]);
}

void LedMatrix::setPixel(uint16_t index, uint8_t red, uint8_t green, uint8_t blue) {
    if (pixel_buffer_[index][0] == red &&
        pixel_buffer_[index][1] == green &&
        pixel_buffer_[index][2] == blue) {
        return;
    }

    pixel_buffer_[index][0] = red;
    pixel_buffer_[index][1] = green;
    pixel_buffer_[index][2] = blue;
    frame_dirty_ = true;
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip_, index, red, green, blue));
}

void LedMatrix::refreshIfDirty() {
    if (led_strip_ == nullptr || !frame_dirty_) {
        return;
    }

    ESP_ERROR_CHECK(led_strip_refresh(led_strip_));
    frame_dirty_ = false;
}

void LedMatrix::drawGraphic(const uint8_t* graphic) {
    if (led_strip_ == nullptr || graphic == nullptr) {
        return;
    }

    for (uint16_t i = 0; i < kLedCount_; i++) {
        if (graphic[i] == 1) {
            setPixel(i, red_, green_, blue_);
        } else {
            setPixel(i, 0, 0, 0);
        }
    }

    refreshIfDirty();
}

void LedMatrix::scrollGraphics(const uint8_t** graphic_sequence,
    uint8_t num_graphics,
    uint32_t animation_frame,
    uint8_t scroll_speed) {
    if (led_strip_ == nullptr || graphic_sequence == nullptr || num_graphics == 0) {
        return;
    }

    const int8_t spacing = -2;
    const uint8_t column_offset = 1;
    const uint8_t graphic_width = 8;
    uint32_t scroll_offset = (animation_frame * scroll_speed) / 10;

    for (uint8_t row = 0; row < kMatrixWidth_; row++) {
        for (uint8_t col = 0; col < kMatrixWidth_; col++) {
            uint32_t logical_col = col + scroll_offset;
            uint8_t graphic_index = logical_col / (graphic_width + spacing);
            uint8_t graphic_col = (logical_col % (graphic_width + spacing)) + column_offset;
            uint16_t led_index = row * kMatrixWidth_ + col;
            uint8_t pixel_value = 0;

            graphic_index = graphic_index % num_graphics;
            pixel_value = graphic_sequence[graphic_index][row * graphic_width + graphic_col];

            if (pixel_value == 1) {
                setPixel(led_index, red_, green_, blue_);
            } else {
                setPixel(led_index, 0, 0, 0);
            }
        }
    }

    refreshIfDirty();
}

void LedMatrix::displayNumber(uint8_t number) {
    if (number > 30) {
        displayIcon(MatrixIcon::THIRTY_PLUS);
        return;
    }

    drawGraphic(kNumbers[number]);
}

void LedMatrix::displayClock(const char* time_str, uint32_t animation_frame) {
    const uint8_t* graphic_sequence[7] = {
        kNumbers[time_str[0] - '0'],
        kNumbers[time_str[1] - '0'],
        kIcons[static_cast<uint8_t>(MatrixIcon::COLON)],
        kNumbers[time_str[3] - '0'],
        kNumbers[time_str[4] - '0'],
        kIcons[static_cast<uint8_t>(MatrixIcon::BLANK)],
        kIcons[static_cast<uint8_t>(MatrixIcon::BLANK)],
    };

    scrollGraphics(graphic_sequence, 7, animation_frame, 3);
}

bool LedMatrix::parseMinutes(const char* display_text, uint8_t* minutes) const {
    if (display_text == nullptr || minutes == nullptr) {
        return false;
    }

    while (*display_text == ' ') {
        display_text++;
    }

    if (strcmp(display_text, "Nu") == 0 ||
        strcmp(display_text, "NU") == 0 ||
        strcmp(display_text, "Now") == 0 ||
        strcmp(display_text, "NOW") == 0) {
        *minutes = 0;
        return true;
    }

    char* end = nullptr;
    unsigned long value = strtoul(display_text, &end, 10);
    if (end == display_text) {
        return false;
    }

    if (value > 255) {
        value = 255;
    }

    *minutes = static_cast<uint8_t>(value);
    return true;
}

bool LedMatrix::isClockDisplay(const char* display_text) const {
    if (display_text == nullptr || strlen(display_text) != 5) {
        return false;
    }

    return isdigit(static_cast<unsigned char>(display_text[0])) &&
        isdigit(static_cast<unsigned char>(display_text[1])) &&
        display_text[2] == ':' &&
        isdigit(static_cast<unsigned char>(display_text[3])) &&
        isdigit(static_cast<unsigned char>(display_text[4]));
}
