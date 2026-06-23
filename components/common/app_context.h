#pragma once
#include "freertos/FreeRTOS.h"

static constexpr UBaseType_t kSystemQueueLength = 10;
static constexpr UBaseType_t kNetworkQueueLength = 10;
static constexpr char kFirmwareVersion[] = "1.0.0";

struct Queues {
    QueueHandle_t system_in_queue;
    QueueHandle_t network_in_queue;
};
