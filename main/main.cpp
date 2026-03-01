// #include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_context.h"
#include "system_manager.h"
#include "network_manager.h"
#include "message_types.h"
#include "system_init.h"

extern "C" void app_main(void)
{
    systemInit();
    Queues queues = {
        .system_in_queue = xQueueCreate(10, sizeof(DataPacket)),
        .network_in_queue = xQueueCreate(10, sizeof(DataPacket)),
    };
    SystemManager system_manager(&queues);
    NetworkManager network_manager(&queues);

    while(1) {
        vTaskDelay(portMAX_DELAY);
    }
}
