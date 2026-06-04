#pragma once
#include <FastLED.h>
#include "config.h"

extern LogLevel logLevel;

class LedMatrix {
    CRGB leds[PIXELS_NUM];
    void scrollGraphics(const uint8_t** graphicSequence,
                        uint8_t numGraphics,
                        unsigned long animationFrame,
                        uint8_t scrollSpeed
                    );
public:
    LedMatrix();
    void init();
    void clear();
    void setColor();
    void animateColor();
    void displayDeparture(uint8_t timeToDeparture);
    void displayClocktime(const char* timeStr, unsigned long animationFrame, uint8_t scrollSpeed = 3);
    void displayIcon(uint8_t index);
    void bootAnimation(unsigned long frame);
    void connectionAnimation(unsigned long frame);
    void sleepAnimation(unsigned long frame);
    void leftArrowAnimation();
};