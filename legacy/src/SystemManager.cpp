#include "SystemManager.h"
#include "networkDebug.h"

SystemManager::SystemManager() {
}

void SystemManager::init() {
    // Setup
    mainButton = buttonMng.addButton(BUTTON_PIN, true);
    systemState = SystemState::BOOT;
    prevSystemState = SystemState::ERROR;
    
    // Queues
    dataQueue = xQueueCreate(QUEUE_LEN, sizeof(QueuePacket));
    if (dataQueue != nullptr && logLevel >= LogLevel::DEBUG) printf("QueuePacket queue created!\n");
    else if (logLevel >= LogLevel::DEBUG) printf("Error creating dataQueue.\n");
    settingsQueue = xQueueCreate(QUEUE_LEN, sizeof(SettingsPacket));
    if (settingsQueue != nullptr && logLevel >= LogLevel::DEBUG) printf("Settings queue created!\n");
    else if (logLevel >= LogLevel::DEBUG) printf("Error creating settingsQueue.\n");

    // Modules inits
    matrix.init();
    networkManager.init(dataQueue, settingsQueue);
    
    // Variables
    settingsSent = false;
    now = millis();
    prevTime = 0;
    newData = false;
    animationFrame = 0;
    lastFrameTime = 0;
    bootFinished = false;
    bootSettingsSent = false;
    settingsPacketSent = false;

    // Settings
    settingsData.settingTransportMode = TransportMode::METRO;
    settingsData.settingDirectionCode = 1;
    strncpy(settingsData.settingSsid, "MaxGuest", sizeof(settingsData.settingSsid));
    strncpy(settingsData.settingPassword, "yourewelcome", sizeof(settingsData.settingPassword));
    
    // Create tasks
    xTaskCreatePinnedToCore(     // UI Task
      systemTask,                // Function to implement the task
      "systemUiTask",            // Name of the task
      8192,                      // Stack size in words
      this,                      // Task input parameter
      2,                         // Priority of the task
      &systemTaskHandle,         // Task handle.
      0                          // Core where the task should run
    ); 

    xTaskCreatePinnedToCore(     // Data Task
      networkTask,
      "networkTask",
      8192,
      &networkManager,
      1,
      &networkTaskHandle,
      1
    ); 
}

void SystemManager::run() {
    buttonMng.updateAll();
    if (mainButton->wasPushed()) {
        settingsData.settingDirectionCode = (settingsData.settingDirectionCode == 1) ? 2 : 1;
        newData = true;
        matrix.leftArrowAnimation();
    }

    if (bootFinished) checkForNewPackage();
    switch (systemState) {
        case SystemState::BOOT:
            if (!settingsPacketSent) {
                printSystemState();
                sendSettingsPackage();
                matrix.displayIcon(0);
                settingsPacketSent = true;
            }
            if (!bootFinished && animationFrame > 100) {
                    bootFinished = true;
                    setSystemState(EventType::NO_WIFI);
            }
            break;
        case SystemState::NO_WIFI:
            matrix.setColor();
            matrix.connectionAnimation(animationFrame);
            break;
        case SystemState::NO_DATA:
            matrix.setColor();
            matrix.displayIcon(6);
            break;
        case SystemState::DATA:
            matrix.setColor();
            showDeparture();
            break;
        case SystemState::NO_API_RESPONSE:
            matrix.setColor();
            matrix.displayIcon(2);
            break;
        case SystemState::SETUP:
            break;
    }
    updateAnimationFrame();
}

void SystemManager::showDeparture() {
    uint8_t direction = settingsData.settingDirectionCode - 1;
    Departure departure = receivedData.direction[direction].departures[0];
    uint8_t numDeparture = receivedData.direction[direction].count;

    if (newData && numDeparture > 0) { // !!! Separate the conditions
        if (numDeparture > 0 && departure.displayTimeType == TimeDisplayType::MINUTES) {
            matrix.displayDeparture(departure.minutes); 
            if (logLevel >= LogLevel::DEBUG) printf("Next departure: %d min\n", receivedData.direction[direction].departures[0].minutes);
        } else if (numDeparture > 0 && departure.displayTimeType == TimeDisplayType::CLOCK_TIME) {
            char timeBuffer[10];
            strcpy(timeBuffer, receivedData.direction[settingsData.settingDirectionCode - 1].departures[0].clock_time);
            matrix.displayClocktime(timeBuffer, animationFrame);
            if (logLevel >= LogLevel::DEBUG) printf("Next departure: %d\n", receivedData.direction[direction].departures[0].clock_time);
        }
    }
    // char timeBuffer[10];
    // strcpy(timeBuffer, receivedData.direction[settingsData.settingDirectionCode - 1].departures[2].clock_time);
    // matrix.displayClocktime(timeBuffer, animationFrame);
    // Serial.print("Third departure: "); Serial.println(receivedData.direction[direction].departures[2].clock_time);
    newData = false;
}

bool SystemManager::setSystemState(EventType event) {
    SystemState newState = systemState;

    switch (event) {
        case EventType::NO_WIFI:         newState = SystemState::NO_WIFI; break;
        case EventType::NO_DATA:         newState = SystemState::NO_DATA; break;
        case EventType::DATA:            newState = SystemState::DATA; break;
        case EventType::NO_API_RESPONSE: newState = SystemState::NO_API_RESPONSE; break;
    }

    if (newState == systemState) return false;
    systemState = newState;
    if (logLevel >= LogLevel::DEBUG) printSystemState();
    return true;
}

void SystemManager::checkForNewPackage() {
    if (xQueueReceive(dataQueue, (void *)&receivedData, 0) == pdTRUE) {
        if (logLevel >= LogLevel::DEBUG) printf("Packet received!\n");
        setSystemState(receivedData.type);
        if (receivedData.type == EventType::DATA) {
            newData = true;
        } 
        if (logLevel >= LogLevel::DEBUG) printPackage();
    }
}

void SystemManager::sendSettingsPackage() {
    BaseType_t returnStatus;
    xQueueOverwrite(settingsQueue, (void *)&settingsData);
}

void SystemManager::updateAnimationFrame() {
    animationFrame = (millis() / FRAME_RATE);
}

void SystemManager::systemTask(void* pvParameters) {
    if (logLevel >= LogLevel::DEBUG) printf("systemTask running on core %d\n", xPortGetCoreID());
    SystemManager* systemManager = static_cast<SystemManager*>(pvParameters);

    while(1) {
        systemManager->run();
        vTaskDelay(pdMS_TO_TICKS(SYSTEM_MANAGER_UPDATE));
    }
}

void SystemManager::networkTask(void* pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(200));
    if (logLevel >= LogLevel::DEBUG) printf("networkTask running on core %d\n", xPortGetCoreID());
    NetworkManager* network = static_cast<NetworkManager*>(pvParameters);

    while(1) {
        network->run();
        vTaskDelay(pdMS_TO_TICKS(NETWORK_MANAGER_UPDATE));
    } 
}

void SystemManager::printSystemState() {
    const char* stateStr = "UNKNOWN";
    switch (systemState) {
        case SystemState::BOOT:            stateStr = "BOOT"; break;
        case SystemState::NO_WIFI:         stateStr = "NO_WIFI"; break;
        case SystemState::NO_DATA:         stateStr = "NO_DATA"; break;
        case SystemState::NO_API_RESPONSE: stateStr = "NO_API_RESPONSE"; break;
        case SystemState::DATA:            stateStr = "DATA"; break;
        case SystemState::SETUP:           stateStr = "SETUP"; break;
        case SystemState::ERROR:           stateStr = "ERROR"; break;
    }
    Serial.print("SystemState: ");
    Serial.println(stateStr);
}

void SystemManager::printPackage() {
    char buf[512];
    size_t len = 0;
    len += snprintf(buf + len, sizeof(buf) - len,
                    "\n=== PACKET RECEIVED ===\n");
    const char* typeStr = "UNKNOWN";
    switch (receivedData.type) {
        case EventType::NO_WIFI:         typeStr = "NO_WIFI"; break;
        case EventType::NO_DATA:         typeStr = "NO_DATA"; break;
        case EventType::NO_API_RESPONSE: typeStr = "NO_API_RESPONSE"; break;
        case EventType::DATA:            typeStr = "DATA"; break;
    }
    len += snprintf(buf + len, sizeof(buf) - len,
                    "Type: %s\n\n", typeStr);
    if (receivedData.type != EventType::DATA) {
        len += snprintf(buf + len, sizeof(buf) - len,
                        "=======================\n");
        Serial.print(buf);
        return;
    }
    for (uint8_t d = 0; d < 2; d++) {
        Direction& dir = receivedData.direction[d];
        len += snprintf(buf + len, sizeof(buf) - len,
                        "Direction %u (count: %u)\n",
                        d, dir.count);
        for (uint8_t i = 0; i < dir.count && i < NUM_DEPARTURES; i++) {
            Departure& dep = dir.departures[i];
            if (dep.displayTimeType == TimeDisplayType::MINUTES) {
                len += snprintf(buf + len, sizeof(buf) - len,
                                "  [%u] %u min\n",
                                i, dep.minutes);
            }
            else if (dep.displayTimeType == TimeDisplayType::CLOCK_TIME) {
                if (dep.clock_time[0] != '\0') {
                    len += snprintf(buf + len, sizeof(buf) - len,
                                    "  [%u] %s\n",
                                    i, dep.clock_time);
                } else {
                    len += snprintf(buf + len, sizeof(buf) - len,
                                    "  [%u] ?? (empty clock_time)\n",
                                    i);
                }
            }
            else {
                len += snprintf(buf + len, sizeof(buf) - len,
                                "  [%u] ?? (invalid type)\n",
                                i);
            }
        }
        len += snprintf(buf + len, sizeof(buf) - len, "\n");
    }
    len += snprintf(buf + len, sizeof(buf) - len,
                    "=======================\n");
    Serial.print(buf);
}
