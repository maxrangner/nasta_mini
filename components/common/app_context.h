#pragma once
#include "freertos/FreeRTOS.h"

struct Queues {
    QueueHandle_t system_in_queue;
    QueueHandle_t network_in_queue;
};
