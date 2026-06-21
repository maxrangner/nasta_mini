#include "unity.h"

#include <string.h>

#include "display.h"
#include "system_manager.h"

static constexpr UBaseType_t kHostQueueCapacity = 10;
static constexpr size_t kHostQueueItemSize =
    sizeof(SystemEvent) > sizeof(NetworkCommand)
        ? sizeof(SystemEvent)
        : sizeof(NetworkCommand);

struct HostQueue {
    UBaseType_t item_size = 0;
    UBaseType_t queue_length = 0;
    UBaseType_t count = 0;
    uint8_t items[kHostQueueCapacity][kHostQueueItemSize] {};
};

static DeviceSettings loaded_settings_stub {};
static Queues test_queues {};
static SystemManager* system_manager = nullptr;
static Display test_display {};
static DisplayState last_display_state {};
static DisplayAnimation last_animation = DisplayAnimation::NONE;

struct SystemManagerHostTestAccess {
    static void startRuntime(SystemManager& system_manager) {
        system_manager.startRuntime();
    }

    static void handleEvent(SystemManager& system_manager, const SystemEvent& system_event) {
        system_manager.handleSystemEvent(system_event);
    }

    static DisplayState buildDisplayState(const SystemManager& system_manager) {
        return system_manager.buildDisplayState();
    }
};

static DeviceSettings makeValidSettings() {
    DeviceSettings settings {};
    settings.setup.needs_setup = false;
    settings.wifi.ssid[0] = 'A';
    settings.site.site_id = 1;
    settings.startup_direction = 1;
    settings.walk_time_minutes = 4;
    settings.brightness = DisplayBrightness::HIGH;
    settings.flip_direction_arrows = false;
    return settings;
}

static DeviceSettings makeSetupModeSettings() {
    DeviceSettings settings = makeValidSettings();
    strcpy(settings.wifi.ssid, "Saved WiFi");
    strcpy(settings.wifi.password, "SavedPass");
    settings.site.site_id = 9192;
    settings.site.transport_filter = TransportMode::TRAIN;
    settings.startup_direction = 2;
    settings.walk_time_minutes = 6;
    settings.brightness = DisplayBrightness::LOW;
    settings.flip_direction_arrows = true;
    settings.setup.needs_setup = true;
    return settings;
}

static Queues makeQueues() {
    Queues queues {};
    queues.system_in_queue = xQueueCreate(kSystemQueueLength, sizeof(SystemEvent));
    queues.network_in_queue = xQueueCreate(kNetworkQueueLength, sizeof(NetworkCommand));
    return queues;
}

static void sendNetworkState(const NetworkState& network_state) {
    SystemEvent system_event {};
    system_event.type = SystemEventType::NETWORK_STATE;
    system_event.network_state = network_state;
    SystemManagerHostTestAccess::handleEvent(*system_manager, system_event);
}

static DisplayState currentDisplayState() {
    return SystemManagerHostTestAccess::buildDisplayState(*system_manager);
}

static void deleteQueues(Queues* queues) {
    if (queues == nullptr) {
        return;
    }

    if (queues->system_in_queue != nullptr) {
        vQueueDelete(queues->system_in_queue);
        queues->system_in_queue = nullptr;
    }

    if (queues->network_in_queue != nullptr) {
        vQueueDelete(queues->network_in_queue);
        queues->network_in_queue = nullptr;
    }
}

void setUp(void)
{
    loaded_settings_stub = makeValidSettings();
    test_queues = makeQueues();
    last_display_state = DisplayState {};
    last_animation = DisplayAnimation::NONE;
    test_display = Display {};
    system_manager = new SystemManager(&test_queues, &test_display);
    system_manager->init();
}

void tearDown(void)
{
    delete system_manager;
    system_manager = nullptr;
    deleteQueues(&test_queues);
}

QueueHandle_t xQueueCreate(UBaseType_t queue_length, UBaseType_t item_size) {
    HostQueue* queue = new HostQueue;
    queue->item_size = item_size;
    queue->queue_length = queue_length;
    return queue;
}

BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t ticks_to_wait) {
    (void)ticks_to_wait;

    if (queue == nullptr || item == nullptr) {
        return pdFALSE;
    }

    if (queue->count >= queue->queue_length ||
        queue->count >= (sizeof(queue->items) / sizeof(queue->items[0])) ||
        queue->item_size > sizeof(queue->items[0])) {
        return pdFALSE;
    }

    memcpy(queue->items[queue->count], item, queue->item_size);
    queue->count++;
    return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t queue, void* buffer, TickType_t ticks_to_wait) {
    (void)ticks_to_wait;

    if (queue == nullptr || buffer == nullptr || queue->count == 0) {
        return pdFALSE;
    }

    memcpy(buffer, queue->items[0], queue->item_size);

    for (UBaseType_t i = 1; i < queue->count; i++) {
        memcpy(queue->items[i - 1], queue->items[i], queue->item_size);
    }

    queue->count--;
    return pdTRUE;
}

void vQueueDelete(QueueHandle_t queue) {
    delete queue;
}

BaseType_t xTaskCreatePinnedToCore(
    TaskFunction_t task_code,
    const char* name,
    uint32_t stack_depth,
    void* parameters,
    UBaseType_t priority,
    TaskHandle_t* task_handle,
    BaseType_t core_id
) {
    static int task_handle_stub = 0;

    (void)task_code;
    (void)name;
    (void)stack_depth;
    (void)parameters;
    (void)priority;
    (void)core_id;

    if (task_handle != nullptr) {
        *task_handle = &task_handle_stub;
    }

    return pdPASS;
}

TickType_t xTaskGetTickCount(void) {
    return 0;
}

void vTaskDelayUntil(TickType_t* previous_wake_time, TickType_t time_increment) {
    (void)previous_wake_time;
    (void)time_increment;
}

extern "C" void button_service_init() {
}

extern "C" void button_init(const button_cfg_t* cfg, button_t* button_handle) {
    if (cfg == nullptr || button_handle == nullptr) {
        return;
    }

    button_handle->gpio_num = cfg->gpio_num;
    button_handle->pressed_level = false;
    button_handle->current_edge = false;
    button_handle->long_press_dur = cfg->long_press_dur;
    button_handle->btn_callback = cfg->btn_callback;
    button_handle->user_data = cfg->user_data;
    button_handle->timer = nullptr;
    button_handle->press_time = 0;
}

bool loadDeviceSettings(DeviceSettings* settings) {
    if (settings == nullptr) {
        return false;
    }

    *settings = loaded_settings_stub;
    return true;
}

bool saveDeviceSettings(const DeviceSettings& settings) {
    (void)settings;
    return true;
}

void Display::init() {
}

void Display::setState(const DisplayState& display_state) {
    last_display_state = display_state;
}

void Display::playAnimation(DisplayAnimation animation) {
    last_animation = animation;
}

void Display::update() {
}

void test_system_manager_start_runtime_queues_start_normal_mode_when_loaded_settings_are_valid(void)
{
    TEST_ASSERT_NOT_NULL(test_queues.system_in_queue);
    TEST_ASSERT_NOT_NULL(test_queues.network_in_queue);
    TEST_ASSERT_NOT_NULL(system_manager);

    SystemManagerHostTestAccess::startRuntime(*system_manager);

    NetworkCommand command {};
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(test_queues.network_in_queue, &command, 0));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkCommandType::START_NORMAL_MODE),
        static_cast<int>(command.type)
    );
    TEST_ASSERT_EQUAL_UINT32(loaded_settings_stub.site.site_id, command.settings.site.site_id);
    TEST_ASSERT_EQUAL_UINT8(loaded_settings_stub.startup_direction, command.settings.startup_direction);
    TEST_ASSERT_EQUAL_UINT8(loaded_settings_stub.walk_time_minutes, command.settings.walk_time_minutes);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(loaded_settings_stub.brightness),
        static_cast<int>(command.settings.brightness)
    );
    TEST_ASSERT_EQUAL(loaded_settings_stub.flip_direction_arrows, command.settings.flip_direction_arrows);
    TEST_ASSERT_EQUAL_CHAR(loaded_settings_stub.wifi.ssid[0], command.settings.wifi.ssid[0]);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayAnimation::BOOT),
        static_cast<int>(last_animation)
    );
}

void test_system_manager_start_runtime_queues_start_setup_mode_with_loaded_settings(void)
{
    loaded_settings_stub = makeSetupModeSettings();
    delete system_manager;
    system_manager = new SystemManager(&test_queues, &test_display);
    system_manager->init();

    SystemManagerHostTestAccess::startRuntime(*system_manager);

    NetworkCommand command {};
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(test_queues.network_in_queue, &command, 0));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkCommandType::START_SETUP_MODE),
        static_cast<int>(command.type)
    );
    TEST_ASSERT_EQUAL_UINT32(loaded_settings_stub.site.site_id, command.settings.site.site_id);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(loaded_settings_stub.site.transport_filter),
        static_cast<int>(command.settings.site.transport_filter)
    );
    TEST_ASSERT_EQUAL_UINT8(loaded_settings_stub.startup_direction, command.settings.startup_direction);
    TEST_ASSERT_EQUAL_UINT8(loaded_settings_stub.walk_time_minutes, command.settings.walk_time_minutes);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(loaded_settings_stub.brightness),
        static_cast<int>(command.settings.brightness)
    );
    TEST_ASSERT_EQUAL(loaded_settings_stub.flip_direction_arrows, command.settings.flip_direction_arrows);
    TEST_ASSERT_EQUAL_STRING(loaded_settings_stub.wifi.ssid, command.settings.wifi.ssid);
    TEST_ASSERT_EQUAL_STRING(loaded_settings_stub.wifi.password, command.settings.wifi.password);
}

void test_system_manager_enters_connecting_state_when_network_reports_connecting(void)
{
    NetworkState network_state {};
    network_state.status = NetworkStatus::CONNECTING;

    sendNetworkState(network_state);

    DisplayState display_state = currentDisplayState();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::CONNECTING),
        static_cast<int>(display_state.system_state)
    );
}

void test_system_manager_enters_connected_state_when_network_is_ready_with_no_departures(void)
{
    NetworkState network_state {};
    network_state.status = NetworkStatus::CONNECTED;

    sendNetworkState(network_state);

    DisplayState display_state = currentDisplayState();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::CONNECTED),
        static_cast<int>(display_state.system_state)
    );
}

void test_system_manager_enters_api_error_state_when_network_reports_api_error(void)
{
    NetworkState network_state {};
    network_state.status = NetworkStatus::API_ERROR;

    sendNetworkState(network_state);

    DisplayState display_state = currentDisplayState();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::API_ERROR),
        static_cast<int>(display_state.system_state)
    );
}

void test_system_manager_enters_no_departures_state_when_network_is_ready_with_empty_departures(void)
{
    NetworkState network_state {};
    network_state.status = NetworkStatus::NO_DEPARTURES;

    sendNetworkState(network_state);

    DisplayState display_state = currentDisplayState();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::NO_DEPARTURES),
        static_cast<int>(display_state.system_state)
    );
}

void test_system_manager_enters_departures_state_when_network_has_departures(void)
{
    NetworkState network_state {};
    network_state.status = NetworkStatus::DEPARTURES;
    network_state.departures.directions[0].count = 1;
    memcpy(
        network_state.departures.directions[0].departures[0].display,
        "5 min",
        sizeof("5 min")
    );

    sendNetworkState(network_state);

    DisplayState display_state = currentDisplayState();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::DEPARTURES),
        static_cast<int>(display_state.system_state)
    );
    TEST_ASSERT_EQUAL_STRING("5 min", display_state.departure_text);
    TEST_ASSERT_EQUAL_UINT8(loaded_settings_stub.walk_time_minutes, display_state.walk_time_minutes);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(loaded_settings_stub.brightness),
        static_cast<int>(display_state.brightness)
    );
}

void test_system_manager_uses_default_arrow_mapping_when_flip_setting_is_off(void)
{
    SystemEvent system_event {};
    system_event.type = SystemEventType::TOGGLE_DIRECTION;

    SystemManagerHostTestAccess::handleEvent(*system_manager, system_event);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayAnimation::DIRECTION_RIGHT),
        static_cast<int>(last_animation)
    );
}

void test_system_manager_flips_arrow_mapping_when_setting_is_on(void)
{
    loaded_settings_stub = makeValidSettings();
    loaded_settings_stub.flip_direction_arrows = true;
    delete system_manager;
    system_manager = new SystemManager(&test_queues, &test_display);
    system_manager->init();

    SystemEvent system_event {};
    system_event.type = SystemEventType::TOGGLE_DIRECTION;

    SystemManagerHostTestAccess::handleEvent(*system_manager, system_event);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayAnimation::DIRECTION_LEFT),
        static_cast<int>(last_animation)
    );
}
