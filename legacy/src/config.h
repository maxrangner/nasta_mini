#pragma once
#include <stdint.h>

enum class LogLevel : uint8_t {
    NONE,
    ERROR,
    INFO,
    DEBUG
};

constexpr const uint8_t LED_PIN = 14;
constexpr const uint8_t PIXELS_NUM = 64;
constexpr const uint8_t BUTTON_PIN = 0;
constexpr const uint8_t QUEUE_LEN = 1;
constexpr const uint8_t QUEUE_ITEM_SIZE = 4;
constexpr const uint8_t NUM_DEPARTURES = 4;
constexpr const uint8_t SYSTEM_MANAGER_UPDATE = 50;
constexpr const uint16_t NETWORK_MANAGER_UPDATE = 1000;
constexpr const uint8_t FRAME_RATE = 50; // 50 = 20fps, 33 = 30fps, 20 = 50fps, 17 = 60fps
constexpr const char* API_URL = "https://transport.integration.sl.se/";