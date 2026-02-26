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
        .data_queue = xQueueCreate(1, sizeof(DataPacket)),
        .settings_queue = xQueueCreate(1, sizeof(SettingsPacket)),
        .wifi_event_queue = xQueueCreate(10, sizeof(WifiEvent))
    };
    SystemManager system_manager(&queues);
    NetworkManager network_manager(&queues);
    network_manager.init();

    while(1) {
        vTaskDelay(portMAX_DELAY);
    }
}
