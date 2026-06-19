#pragma once

#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "settings.h"

bool startSetupPortal(httpd_handle_t* server, QueueHandle_t system_in_queue, const DeviceSettings& settings);
void stopSetupPortal(httpd_handle_t* server);
void pollSetupPortal();
