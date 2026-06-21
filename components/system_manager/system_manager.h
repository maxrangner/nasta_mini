#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "app_context.h"
#include "button_driver.h"
#include "display.h"
#include "message_types.h"

class SystemManager {
    static constexpr uint32_t kMainButtonPin_ = 3;
    static constexpr bool kMainButtonHasPullup_ = true;
    static constexpr uint16_t kButtonDebounceMs_ = 50;
    static constexpr uint16_t kButtonLongPressMs_ = 3000;
    static constexpr BaseType_t kTaskCore_ = 0;
    static constexpr uint32_t kUpdateInterval_ = 100;

    TaskHandle_t task_system_manager_ = nullptr;
    QueueHandle_t system_in_queue_ = nullptr;
    QueueHandle_t network_in_queue_ = nullptr;
    Display* display_ = nullptr;
    button_t main_button_ {};
    DeviceSettings settings_ {};
    NetworkState network_state_ {};
    SystemState system_state_ = SystemState::BOOT;
    uint8_t selected_direction_ = 1;
    friend struct SystemManagerHostTestAccess;
public:
    SystemManager(Queues* queues, Display* display);
    void init();
private:
    static void systemTask(void* pvParameters);
    static void handleButtonCallback(button_event_t event, uint8_t gpio_num, void* user_data);
    void startRuntime();
    void handleSystemEvent(const SystemEvent& system_event);
    void handleNetworkState(const NetworkState& network_state);
    void handleToggleDirection();
    void handleForceSetup();
    void handleSetupConfig(const SetupConfig& setup_config);
    bool saveSetupConfig(const SetupConfig& setup_config);
    bool requestMode(NetworkCommandType command_type);
    void setSystemState(SystemState new_state);
    DisplayState buildDisplayState() const;
    static SystemState stateForNetworkStatus(NetworkStatus network_status);
};
