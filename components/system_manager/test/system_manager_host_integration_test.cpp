#include "unity.h"

#include <string.h>

#define private public
#include "system_manager.h"
#undef private

struct HostQueue {
    UBaseType_t item_size = 0;
    UBaseType_t queue_length = 0;
    UBaseType_t count = 0;
    uint8_t items[10][sizeof(SystemEvent)] {};
};

static DeviceSettings loaded_settings_stub {};
static Queues test_queues {};
static SystemManager* system_manager = nullptr;

static DeviceSettings makeValidSettings() {
    DeviceSettings settings {};
    settings.setup.needs_setup = false;
    settings.wifi.ssid[0] = 'A';
    settings.site.site_id = 1;
    settings.startup_direction = 1;
    return settings;
}

static Queues makeQueues() {
    Queues queues {};
    queues.system_in_queue = xQueueCreate(kSystemQueueLength, sizeof(SystemEvent));
    queues.network_in_queue = xQueueCreate(kNetworkQueueLength, sizeof(NetworkCommand));
    return queues;
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

static void sendNetworkState(SystemManager& system_manager, const NetworkState& network_state) {
    SystemEvent event {};
    event.type = SystemEventType::NETWORK_STATE;
    event.network_state = network_state;
    system_manager.handleSystemEvent(event);
}

void setUp(void)
{
    loaded_settings_stub = makeValidSettings();
    test_queues = makeQueues();
    system_manager = new SystemManager(&test_queues);
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

void LedMatrix::init() {
}

void LedMatrix::setColor(uint8_t red, uint8_t green, uint8_t blue) {
    (void)red;
    (void)green;
    (void)blue;
}

void LedMatrix::displayIcon(MatrixIcon icon) {
    (void)icon;
}

void LedMatrix::displayDeparture(const char* display_text, uint32_t animation_frame) {
    (void)display_text;
    (void)animation_frame;
}

void LedMatrix::bootAnimation(uint32_t frame) {
    (void)frame;
}

void LedMatrix::connectionAnimation(uint32_t frame) {
    (void)frame;
}

void test_system_manager_init_queues_start_normal_mode_when_loaded_settings_are_valid(void)
{
    TEST_ASSERT_NOT_NULL(test_queues.system_in_queue);
    TEST_ASSERT_NOT_NULL(test_queues.network_in_queue);
    TEST_ASSERT_NOT_NULL(system_manager);

    system_manager->init();

    NetworkCommand command {};
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(test_queues.network_in_queue, &command, 0));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkCommandType::START_NORMAL_MODE),
        static_cast<int>(command.type)
    );
    TEST_ASSERT_EQUAL_UINT32(loaded_settings_stub.site.site_id, command.settings.site.site_id);
    TEST_ASSERT_EQUAL_UINT8(loaded_settings_stub.startup_direction, command.settings.startup_direction);
    TEST_ASSERT_EQUAL_CHAR(loaded_settings_stub.wifi.ssid[0], command.settings.wifi.ssid[0]);
}

void test_system_manager_enters_connecting_state_when_network_reports_connecting(void)
{
    NetworkState network_state {};
    network_state.phase = NetworkPhase::CONNECTING;

    sendNetworkState(*system_manager, network_state);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::CONNECTING),
        static_cast<int>(system_manager->getState())
    );
}

void test_system_manager_enters_connected_state_when_network_is_ready_with_no_departures(void)
{
    NetworkState network_state {};
    network_state.phase = NetworkPhase::READY;
    network_state.departure_state = DepartureState::NONE;

    sendNetworkState(*system_manager, network_state);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::CONNECTED),
        static_cast<int>(system_manager->getState())
    );
}

void test_system_manager_enters_api_error_state_when_network_reports_api_error(void)
{
    NetworkState network_state {};
    network_state.phase = NetworkPhase::READY;
    network_state.departure_state = DepartureState::API_ERROR;

    sendNetworkState(*system_manager, network_state);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::API_ERROR),
        static_cast<int>(system_manager->getState())
    );
}

void test_system_manager_enters_no_departures_state_when_network_is_ready_with_empty_departures(void)
{
    NetworkState network_state {};
    network_state.phase = NetworkPhase::READY;
    network_state.departure_state = DepartureState::READY;

    sendNetworkState(*system_manager, network_state);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::NO_DEPARTURES),
        static_cast<int>(system_manager->getState())
    );
}

void test_system_manager_enters_departures_state_when_network_has_departures(void)
{
    NetworkState network_state {};
    network_state.phase = NetworkPhase::READY;
    network_state.departure_state = DepartureState::READY;
    network_state.departures.directions[0].count = 1;
    memcpy(
        network_state.departures.directions[0].departures[0].display,
        "5 min",
        sizeof("5 min")
    );

    sendNetworkState(*system_manager, network_state);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::DEPARTURES),
        static_cast<int>(system_manager->getState())
    );
}
