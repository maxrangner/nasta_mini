#pragma once
#include "freertos/FreeRTOS.h"

struct Queues {
    QueueHandle_t data_queue;
    QueueHandle_t settings_queue;
    QueueHandle_t wifi_event_queue;
};