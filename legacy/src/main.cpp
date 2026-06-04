#include <Arduino.h>
#include "config.h"
#include "SystemManager.h"

/* To be included:
#include "WebServer.h"
#include "Preferences.h"
*/

SystemManager SysMng;
LogLevel logLevel;

void setup() {
  logLevel = LogLevel::NONE;
  Serial.begin(115200);
  delay(1500);
  Serial.println("BOOT!");
  SysMng.init();
}

void loop() {
  // Everything is run through the systemTask and networkTask in SystemManager
}
