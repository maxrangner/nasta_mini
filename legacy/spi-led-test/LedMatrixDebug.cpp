// #include "LedMatrixDebug.h"

// // WS2812 timing via SPI bit-patterns (3.2 MHz SPI)
// // T0H: 0.4µs (0 bit) → SPI pattern: 0b100 (3 bits)
// // T1H: 0.8µs (1 bit) → SPI pattern: 0b110 (3 bits)
// #define WS2812_0 0b100
// #define WS2812_1 0b110

// LedMatrix::LedMatrix() {
//     // Varje LED = 24 bits RGB
//     // Varje bit expanderas till 3 SPI bits
//     // 64 LEDs * 24 bits * 3 = 4608 bits = 576 bytes
//     spiBufferSize = PIXELS_NUM * 24 * 3 / 8;
//     spiBuffer = (uint8_t*)heap_caps_malloc(spiBufferSize, MALLOC_CAP_DMA);
//     memset(spiBuffer, 0, spiBufferSize);
// }

// LedMatrix::~LedMatrix() {
//     if (spiBuffer) {
//         heap_caps_free(spiBuffer);
//     }
//     spi_bus_remove_device(spi);
//     spi_bus_free(SPI2_HOST);
// }

// void LedMatrix::init() {
//     Serial.println("LedMatrix init() - SPI-DMA mode");
    
//     // Konfigurera SPI bus
//     spi_bus_config_t buscfg = {};
//     buscfg.mosi_io_num = LED_DATA_PIN;
//     buscfg.miso_io_num = -1;
//     buscfg.sclk_io_num = -1;
//     buscfg.quadwp_io_num = -1;
//     buscfg.quadhd_io_num = -1;
//     buscfg.max_transfer_sz = spiBufferSize;
    
//     esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
//     if (ret != ESP_OK) {
//         Serial.printf("SPI bus init failed: %d\n", ret);
//         return;
//     }
    
//     // Konfigurera SPI device
//     spi_device_interface_config_t devcfg = {};
//     devcfg.clock_speed_hz = SPI_CLOCK_SPEED;
//     devcfg.mode = 0;
//     devcfg.spics_io_num = -1;
//     devcfg.queue_size = 1;
//     devcfg.flags = SPI_DEVICE_NO_DUMMY;
    
//     ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
//     if (ret != ESP_OK) {
//         Serial.printf("SPI device add failed: %d\n", ret);
//         return;
//     }

//     clear();
//     show();
    
//     delay(1000);
//     testSinglePixel();  // Testa
//     delay(2000);
//     clear();
//     show();
    
//     clear();
//     show();
// }

// void LedMatrix::byteToSPI(uint8_t byte, uint8_t* dest) {
//     // Varje bit blir 3 SPI bits
//     // Totalt 8 bits → 24 SPI bits = 3 bytes
    
//     uint32_t output = 0;
    
//     for (int i = 0; i < 8; i++) {
//         uint8_t bit = (byte >> (7 - i)) & 0x01;
        
//         // Varje bit expanderas till 3 bits
//         if (bit) {
//             output |= (0b110 << ((7 - i) * 3));  // T1H: 110
//         } else {
//             output |= (0b100 << ((7 - i) * 3));  // T0H: 100
//         }
//     }
    
//     // Packa 24 bits i 3 bytes (big-endian)
//     dest[0] = (output >> 16) & 0xFF;
//     dest[1] = (output >> 8) & 0xFF;
//     dest[2] = output & 0xFF;
// }

// void LedMatrix::clear() {
//     memset(spiBuffer, 0, spiBufferSize);
// }

// void LedMatrix::show() {
//     spi_transaction_t trans = {};
//     trans.length = spiBufferSize * 8;
//     trans.tx_buffer = spiBuffer;
    
//     esp_err_t ret = spi_device_transmit(spi, &trans);
//     if (ret != ESP_OK) {
//         Serial.printf("SPI transmit failed: %d\n", ret);
//     }
    
//     // Ändra från 50 till 300 µs
//     delayMicroseconds(300);
// }

// void LedMatrix::displayDeparture(uint8_t timeToDeparture) {
//     if (timeToDeparture >= 30) timeToDeparture = 29;
    
//     clear();
    
//     const uint8_t* departure = numbers[timeToDeparture];
    
//     for (uint8_t i = 0; i < PIXELS_NUM; i++) {
//         uint8_t pixel = departure[i];
        
//         uint8_t r = (pixel == 1) ? 5 : 0;
//         uint8_t g = 0;
//         uint8_t b = 0;
        
//         uint8_t* pixelStart = spiBuffer + (i * 9);
//         memset(pixelStart, 0, 9);
        
//         byteToSPI(r, pixelStart);
//         byteToSPI(g, pixelStart + 3);
//         byteToSPI(b, pixelStart + 6);
//     }
    
//     show();
// }

// void LedMatrix::stressTest() {
//     Serial.println("\n=== LED STRESSTEST START (SPI-DMA) ===");
//     Serial.println("Uppdaterar pixels.show() 1000 gånger...");
//     Serial.println("Loggar ENDAST verkliga timing-spikes (>6ms)");
//     Serial.println("------------------------------------------------");
    
//     const uint32_t SPIKE_THRESHOLD_US = 6000;
//     uint32_t maxShow = 0;
//     uint32_t spikeCount = 0;
//     uint32_t totalCalls = 0;
    
//     for (int i = 0; i < 1000; i++) {
//         uint32_t start = micros();
//         displayDeparture(1);
//         uint32_t dt = micros() - start;
        
//         totalCalls++;
        
//         if (dt > maxShow) {
//             maxShow = dt;
//         }
        
//         if (dt > SPIKE_THRESHOLD_US) {
//             spikeCount++;
//             Serial.printf(
//                 "[SPIKE] iter=%d | show=%lu µs | heap=%u\n",
//                 i,
//                 dt,
//                 ESP.getFreeHeap()
//             );
//         }
        
//         if ((i + 1) % 100 == 0) {
//             Serial.printf(
//                 "[STATS] %d calls | spikes=%lu | max=%lu µs\n",
//                 i + 1,
//                 spikeCount,
//                 maxShow
//             );
//         }
        
//         vTaskDelay(pdMS_TO_TICKS(100));
//     }
    
//     Serial.println("------------------------------------------------");
//     Serial.println("=== STRESSTEST KLAR ===");
//     Serial.printf("Totalt calls: %lu\n", totalCalls);
//     Serial.printf("Totala spikes (>6ms): %lu\n", spikeCount);
//     Serial.printf("Max show(): %lu µs\n", maxShow);
//     Serial.println("Observera: Visuella glitches ska matcha SPIKE-loggar.");
// }

// void LedMatrix::testAllRed() {
//     clear();
    
//     // Sätt alla 64 pixlar till röd (G=0, R=255, B=0)
//     for (uint8_t i = 0; i < PIXELS_NUM; i++) {
//         uint8_t* pixelStart = spiBuffer + (i * 9);
//         memset(pixelStart, 0, 9);
        
//         byteToSPI(0, pixelStart);      // G
//         byteToSPI(255, pixelStart + 3); // R
//         byteToSPI(0, pixelStart + 6);   // B
//     }
    
//     show();
// }

// void LedMatrix::testSinglePixel() {
//     clear();
    
//     // Tänd ENDAST pixel 0 med LÅG intensitet (R=5)
//     uint8_t* pixelStart = spiBuffer;
//     memset(pixelStart, 0, 9);
    
//     byteToSPI(5, pixelStart);      // G
//     byteToSPI(0, pixelStart + 3);  // R (låg intensitet)
//     byteToSPI(0, pixelStart + 6);  // B
    
//     show();
// }