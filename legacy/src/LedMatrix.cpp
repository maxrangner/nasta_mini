#include "LedMatrix.h"
#include "graphics.h"
#include "animations.h"

LedMatrix::LedMatrix() {
}

void LedMatrix::init() {
    if (logLevel >= LogLevel::DEBUG) printf("FastLED init()");
    FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, PIXELS_NUM);
    FastLED.clear();
    FastLED.show();
}

void LedMatrix::clear() {
    FastLED.clear();
    FastLED.show();
}

void LedMatrix::setColor() {
    // Set color
}

void LedMatrix::animateColor() {
    // Animate color
}

void LedMatrix::displayDeparture(uint8_t timeToDeparture) {
    const uint8_t* departure = numbers[timeToDeparture];
    
    for (uint8_t i = 0; i < PIXELS_NUM; i++) {
        if (departure[i] == 1) {
            leds[i].r = 0;
            leds[i].g = 1;
            leds[i].b = 0;
        } else {
            leds[i].r = 0;
            leds[i].g = 0;
            leds[i].b = 0;
        }
    }
    FastLED.show();
}

void LedMatrix::displayIcon(uint8_t index) {
    const uint8_t* departure = icons[index];
    
    for (uint8_t i = 0; i < PIXELS_NUM; i++) {
        if (departure[i] == 1) {
            leds[i].r = 0;
            leds[i].g = 1;
            leds[i].b = 0;
        } else {
            leds[i].r = 0;
            leds[i].g = 0;
            leds[i].b = 0;
        }
    }
    FastLED.show();
}

void LedMatrix::bootAnimation(unsigned long frame) {
    const uint8_t* animationFrame = animations[frame % 8 + 2];
    
    for (uint8_t i = 0; i < PIXELS_NUM; i++) {
        if (animationFrame[i] == 1) {
            leds[i].r = frame % 6 ;
            leds[i].g = (frame + 2) % 6;
            leds[i].b = (frame + 4) % 6;
        } else {
            leds[i].r = 0;
            leds[i].g = 0;
            leds[i].b = 0;
        }
    }
    FastLED.show();
}

void LedMatrix::connectionAnimation(unsigned long frame) {
    uint8_t holdFrame = 1;
    uint8_t numFrames = 4;
    const uint8_t* animationFrame = animations[((frame / holdFrame) % numFrames) + 10]; // + 10 is position of animation in array
    
    for (uint8_t i = 0; i < PIXELS_NUM; i++) {
        if (animationFrame[i] == 1) {
            leds[i].r = frame % 6 ;
            leds[i].g = (frame + 2) % 6;
            leds[i].b = (frame + 4) % 6;
        } else {
            leds[i].r = 0;
            leds[i].g = 0;
            leds[i].b = 0;
        }
    }
    FastLED.show();
}

void LedMatrix::sleepAnimation(unsigned long frame) {
    uint8_t holdFrame = 20;
    uint8_t numFrames = 2;
    const uint8_t* animationFrame = animations[(frame / holdFrame) % numFrames];
    
    for (uint8_t i = 0; i < PIXELS_NUM; i++) {
        if (animationFrame[i] == 1) {
            leds[i].r = 1;
            leds[i].g = 1;
            leds[i].b = 1;
        } else {
            leds[i].r = 0;
            leds[i].g = 0;
            leds[i].b = 0;
        }
    }
    FastLED.show();
}

void LedMatrix::leftArrowAnimation() {
    uint8_t holdFrame = 1;
    uint8_t numFrames = 11;
    unsigned long startFrame = millis();
    unsigned long prevFrame = 1;
    
    for (unsigned long frame = 0; frame < numFrames - 1;) {
        frame = ((millis() - startFrame) / (holdFrame * FRAME_RATE));
        if (frame != prevFrame) {
            FastLED.clear();
            for (uint8_t i = 0; i < PIXELS_NUM; i++) {
                const uint8_t* animationFrame = animations[frame + 14]; // + 14 is position of animation in array
                if (animationFrame[i] == 1) {
                    leds[i].r = 1;
                    leds[i].g = 1;
                    leds[i].b = 1;
                }
            }
            FastLED.show();
        }
        prevFrame = frame;
    }
}

void LedMatrix::scrollGraphics(const uint8_t** graphicSequence, uint8_t numGraphics, unsigned long animationFrame, uint8_t scrollSpeed) {
    unsigned long scrollOffset = (animationFrame * scrollSpeed) / 10;
    const int8_t SPACING = -2;  // start with -1, not -3
    const uint8_t COL_OFFSET = 1;  // skip first column
    const uint8_t GRAPHIC_WIDTH = 8;
    
    for (uint8_t row = 0; row < 8; row++) {
        for (uint8_t col = 0; col < 8; col++) {
            unsigned long logicalCol = col + scrollOffset;
            uint8_t digitIndex = logicalCol / (GRAPHIC_WIDTH + SPACING);
            digitIndex = digitIndex % numGraphics;
            uint8_t digitCol = (logicalCol % (GRAPHIC_WIDTH + SPACING)) + COL_OFFSET;
            
            uint8_t pixelValue = graphicSequence[digitIndex][row * GRAPHIC_WIDTH + digitCol];
            
            uint8_t ledIndex = row * 8 + col;
            if (pixelValue == 1) {
                leds[ledIndex].r = 0;
                leds[ledIndex].g = 1;
                leds[ledIndex].b = 0;
            } else {
                leds[ledIndex].r = 0;
                leds[ledIndex].g = 0;
                leds[ledIndex].b = 0;
            }
        }
    }
    FastLED.show();
}

void LedMatrix::displayClocktime(const char* timeStr, unsigned long animationFrame, uint8_t scrollSpeed) {
    const uint8_t* digitSequence[7];
    
    
    digitSequence[0] = numbers[timeStr[0] - '0'];
    digitSequence[1] = numbers[timeStr[1] - '0'];
    digitSequence[2] = icons[1];
    digitSequence[3] = numbers[timeStr[3] - '0'];
    digitSequence[4] = numbers[timeStr[4] - '0'];
    digitSequence[5] = icons[11];
    digitSequence[6] = icons[11];
    scrollGraphics(digitSequence, 7, animationFrame, scrollSpeed);
}
