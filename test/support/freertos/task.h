#pragma once

#include "FreeRTOS.h"

typedef void (*TaskFunction_t)(void*);

BaseType_t xTaskCreatePinnedToCore(
    TaskFunction_t task_code,
    const char* name,
    uint32_t stack_depth,
    void* parameters,
    UBaseType_t priority,
    TaskHandle_t* task_handle,
    BaseType_t core_id
);
