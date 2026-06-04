// #ifndef LEDMATRIX_H
// #define LEDMATRIX_H

// #include <Arduino.h>
// #include <driver/spi_master.h>
// #include <driver/gpio.h>
// #include "Graphics.h"
// #include "config.h"

// #define SPI_CLOCK_SPEED 3200000  // 3.2 MHz för WS2812 timing
// #define LED_DATA_PIN GPIO_NUM_14

// class LedMatrix {
// private:
//     spi_device_handle_t spi;
//     uint8_t* spiBuffer;
//     size_t spiBufferSize;
    
//     // Konvertera en byte till SPI bit-pattern för WS2812
//     void byteToSPI(uint8_t byte, uint8_t* dest);
    
// public:
//     LedMatrix();
//     ~LedMatrix();
//     void init();
//     void displayDeparture(uint8_t timeToDeparture);
//     void clear();
//     void show();
//     void stressTest();
//     void testAllRed();
//     void testSinglePixel();
// };

// #endif