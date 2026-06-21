#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_context.h"
#include "display.h"
#include "system_manager.h"
#include "network_manager.h"
#include "message_types.h"
#include "system_init.h"

extern "C" void app_main(void)
{
    systemInit();

    Queues queues = {
        .system_in_queue = xQueueCreate(kSystemQueueLength, sizeof(SystemEvent)),
        .network_in_queue = xQueueCreate(kNetworkQueueLength, sizeof(NetworkCommand)),
    };

    Display display;
    display.init();

    SystemManager system_manager(&queues, &display);
    NetworkManager network_manager(&queues);
    network_manager.init();
    system_manager.init();

    while(1) {
        vTaskDelay(portMAX_DELAY);
    }
}
