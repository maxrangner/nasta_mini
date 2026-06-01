#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_context.h"
#include "button_driver.h"
#include "message_types.h"

// SystemManager owns app-facing state and system-side behavior.
class SystemManager {
    static constexpr uint32_t kMainButtonPin_ = 0;
    static constexpr bool kMainButtonHasPullup_ = true;
    static constexpr uint16_t kButtonDebounceMs_ = 50;
    static constexpr uint16_t kButtonLongPressMs_ = 3000;

    TaskHandle_t task_system_manager_ = nullptr;
    QueueHandle_t system_in_queue_ = nullptr;
    QueueHandle_t network_in_queue_ = nullptr;
    static constexpr uint32_t kUpdateInterval_ = 100;
    button_t main_button_ {};
    SystemMessage message_ {};
    NetworkSnapshot network_state_ {};
    SystemState system_state_ = SystemState::BOOT;
    RenderState render_state_ {};
    uint8_t selected_direction_ = 1;
public:
    SystemManager(Queues* queues);
    void init();
    static void systemTask(void* pvParameters);
    static void handleButtonEvent(button_event_t event, uint8_t gpio_num, void* user_data);
    void setState(SystemState new_state);
    void handleInputEvent(SystemInputEvent event);
    void updateSystemState();
    void updateRenderState();
    const RenderState& getRenderState() const;
};
