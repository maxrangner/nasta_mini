#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "apiDataTypes.h"
#include "queuePackets.h"

extern LogLevel logLevel;

enum class NetworkState {
    INIT,
    CONNECTING_STA,
    CONNECTED_STA,
    DISCONNECTED,
    ERROR
};

class NetworkManager {
    QueueHandle_t dataQueue = nullptr;
    QueueHandle_t settingsQueue = nullptr;
    NetworkState networkState;
    NetworkState prevNetworkState;
    const unsigned long reconnectTiming = 10'000;
    const unsigned long apiTiming = 10'000;
    unsigned long prevApiFetch;
    unsigned long prevReconnectAttempt;
    uint8_t reconnectionAttempts;
    String apiSuffix1 = "v1/sites/";
    String apiStation = "9143";
    String apiSuffix2 = "/departures?&forecast=360";
    String apiCombinedURL = API_URL + apiSuffix1 + apiStation + apiSuffix2;
    QueuePacket latestData;
    SettingsPacket settingsData;
    bool hasNewData;

    // Methods
    void debugPrint();
    void onStateChange(EventType event);
    void wifiInit();
    void wifiReconnect();
    void fetchApi();
    void parseJson(JsonDocument doc);
    void updateFields(Direction& directionObject, JsonVariant source);
    bool sendToQueue();
    void waitForSettingsPackage();
    void eventUpdate(EventType event);
    void resetDirectionsCounts();
    TransportMode parseTransportMode(const char* input);
public:
    NetworkManager();
    void init(QueueHandle_t _dataQueue, QueueHandle_t _settingsQueue);
    void run();
};
