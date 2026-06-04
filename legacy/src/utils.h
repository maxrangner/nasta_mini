#pragma once
#include <stdint.h>
#include "NetworkManager.h"

namespace Utils {
    inline void writeCharArray(char* target, size_t targetSize, const char* source) {
        if (targetSize == 0 || source == nullptr) return;
    
        target[0] = '\0';
        size_t i;
        for (i = 0; i < targetSize - 1 && source[i] != '\0'; i++) {
            target[i] = source[i];
        }
        target[i] = '\0';
    }

    inline uint8_t convertTextToMinutes(const char* source) {
        char buffer[10];
        size_t i;
        for (i = 0; source[i] != '\0'; i++) {
            if (source[i] == ' ') break;
            buffer[i] = source[i];
        }
        buffer[i] = '\0';
        long v = strtol(buffer, nullptr, 10);
        return static_cast<uint8_t>(v);
    }
}

