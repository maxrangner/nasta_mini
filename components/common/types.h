#pragma once

#include <stdint.h>

enum class TransportMode : uint8_t {
    METRO,
    TRAM,
    TRAIN,
    BUS,
    SHIP,
    FERRY,
    TAXI,
    UNKNOWN
};
