#pragma once

#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

bool startSetupPortal(httpd_handle_t* server, QueueHandle_t system_in_queue);
void stopSetupPortal(httpd_handle_t* server);
void pollSetupPortal();
