#pragma once
#include "NetworkManager.h"

namespace NetworkDebug {

inline static void debugPrintQueueMessage(const QueuePacket& data) {
    char buffer[512];
    size_t offset = 0;

    for (uint8_t dir = 0; dir < 2; dir++) {

        if (offset >= sizeof(buffer)) break;

        offset += snprintf(
            buffer + offset,
            sizeof(buffer) - offset,
            "Direction %u     Num entries: %u\n",
            dir,
            data.direction[dir].count
        );

        uint8_t n = min(data.direction[dir].count, NUM_DEPARTURES);

        for (uint8_t i = 0; i < n; i++) {

            if (offset >= sizeof(buffer)) break;

            const Departure& dep = data.direction[dir].departures[i];

            if (dep.displayTimeType == TimeDisplayType::MINUTES) {
                offset += snprintf(
                    buffer + offset,
                    sizeof(buffer) - offset,
                    "  [%u] time = %u min     dir = %u\n",
                    i,
                    dep.minutes,
                    dep.directionCode
                );
            } else if (dep.displayTimeType == TimeDisplayType::CLOCK_TIME) {
                offset += snprintf(
                    buffer + offset,
                    sizeof(buffer) - offset,
                    "  [%u] time = %s     dir = %u\n",
                    i,
                    dep.clock_time,
                    dep.directionCode
                );
            } else {
                offset += snprintf(
                    buffer + offset,
                    sizeof(buffer) - offset,
                    "  [%u] time = ?     dir = %u\n",
                    i,
                    dep.directionCode
                );
            }
        }
    }

    Serial.print(buffer);
}

}


