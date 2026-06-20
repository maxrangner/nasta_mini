#include "unity.h"

#include <string.h>

#include "display.h"
#include "led_matrix.h"

enum class RenderKind : uint8_t {
    NONE,
    CLEAR,
    BOOT,
    CONNECTING,
    CONNECTED,
    SETUP,
    DEPARTURE_MINUTES,
    DEPARTURE_CLOCK,
    DEPARTURE_UNKNOWN,
    NO_DEPARTURES,
    API_ERROR,
    NETWORK_ERROR,
    DIRECTION_LEFT,
    DIRECTION_RIGHT
};

struct RenderCall {
    RenderKind kind = RenderKind::NONE;
    uint8_t brightness = 0;
    uint32_t frame = 0;
    uint8_t minutes = 0;
    char text[6] = {};
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
};

static RenderCall last_render {};
static uint8_t current_brightness = kDisplayBrightnessHighValue;

static void clearLastRender() {
    uint8_t brightness = current_brightness;
    last_render = RenderCall {};
    last_render.brightness = brightness;
}

void LedMatrix::init() {
    current_brightness = kDisplayBrightnessHighValue;
    clearLastRender();
}

void LedMatrix::setBrightness(uint8_t brightness) {
    current_brightness = brightness;
    last_render.brightness = brightness;
}

void LedMatrix::clear() {
    clearLastRender();
    last_render.kind = RenderKind::CLEAR;
}

void LedMatrix::showBootFrame(uint32_t frame) {
    clearLastRender();
    last_render.kind = RenderKind::BOOT;
    last_render.frame = frame;
}

void LedMatrix::showConnecting(uint32_t frame) {
    clearLastRender();
    last_render.kind = RenderKind::CONNECTING;
    last_render.frame = frame;
}

void LedMatrix::showConnected() {
    clearLastRender();
    last_render.kind = RenderKind::CONNECTED;
}

void LedMatrix::showSetup() {
    clearLastRender();
    last_render.kind = RenderKind::SETUP;
}

void LedMatrix::showDepartureMinutes(uint8_t minutes, uint8_t r, uint8_t g, uint8_t b) {
    clearLastRender();
    last_render.kind = RenderKind::DEPARTURE_MINUTES;
    last_render.minutes = minutes;
    last_render.red = r;
    last_render.green = g;
    last_render.blue = b;
}

void LedMatrix::showDepartureClock(const char* time_str, uint32_t frame, uint8_t r, uint8_t g, uint8_t b) {
    clearLastRender();
    last_render.kind = RenderKind::DEPARTURE_CLOCK;
    last_render.frame = frame;
    last_render.red = r;
    last_render.green = g;
    last_render.blue = b;

    if (time_str != nullptr) {
        memcpy(last_render.text, time_str, 5);
        last_render.text[5] = '\0';
    }
}

void LedMatrix::showDepartureUnknown() {
    clearLastRender();
    last_render.kind = RenderKind::DEPARTURE_UNKNOWN;
}

void LedMatrix::showNoDepartures() {
    clearLastRender();
    last_render.kind = RenderKind::NO_DEPARTURES;
}

void LedMatrix::showApiError() {
    clearLastRender();
    last_render.kind = RenderKind::API_ERROR;
}

void LedMatrix::showNetworkError() {
    clearLastRender();
    last_render.kind = RenderKind::NETWORK_ERROR;
}

void LedMatrix::showDirectionLeft() {
    clearLastRender();
    last_render.kind = RenderKind::DIRECTION_LEFT;
}

void LedMatrix::showDirectionRight() {
    clearLastRender();
    last_render.kind = RenderKind::DIRECTION_RIGHT;
}

void setUp(void)
{
    displayInit();
    displayPlayAnimation(DisplayAnimation::NONE);
    displaySetState(DisplayState {});
    current_brightness = kDisplayBrightnessHighValue;
    clearLastRender();
}

void tearDown(void)
{
}

void test_display_boot_animation_runs_full_length_before_rendering_state(void)
{
    DisplayState state {};
    state.system_state = SystemState::CONNECTING;
    displaySetState(state);
    displayPlayAnimation(DisplayAnimation::BOOT);

    for (uint32_t frame = 0; frame < LedMatrix::kBootFrameCount; frame++) {
        clearLastRender();
        displayUpdate();
        TEST_ASSERT_EQUAL_INT(static_cast<int>(RenderKind::BOOT), static_cast<int>(last_render.kind));
        TEST_ASSERT_EQUAL_UINT32(frame, last_render.frame);
    }

    clearLastRender();
    displayUpdate();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RenderKind::CONNECTING), static_cast<int>(last_render.kind));
}

void test_display_direction_animation_finishes_after_state_changes(void)
{
    DisplayState state {};
    state.system_state = SystemState::CONNECTING;
    displaySetState(state);
    displayPlayAnimation(DisplayAnimation::DIRECTION_LEFT);

    clearLastRender();
    displayUpdate();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RenderKind::DIRECTION_LEFT), static_cast<int>(last_render.kind));

    state.system_state = SystemState::CONNECTED;
    displaySetState(state);

    clearLastRender();
    displayUpdate();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RenderKind::DIRECTION_LEFT), static_cast<int>(last_render.kind));

    bool finished = false;
    for (uint8_t i = 0; i < 10; i++) {
        clearLastRender();
        displayUpdate();

        if (last_render.kind == RenderKind::CONNECTED) {
            finished = true;
            break;
        }

        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RenderKind::DIRECTION_LEFT),
            static_cast<int>(last_render.kind)
        );
    }

    TEST_ASSERT_TRUE(finished);
}

void test_display_shows_minutes_for_numeric_departure_text(void)
{
    DisplayState state {};
    state.system_state = SystemState::DEPARTURES;
    state.walk_time_minutes = 5;
    state.gradient_minutes = 5;
    memcpy(state.departure_text, "5 min", sizeof("5 min"));
    displaySetState(state);

    clearLastRender();
    displayUpdate();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RenderKind::DEPARTURE_MINUTES),
        static_cast<int>(last_render.kind)
    );
    TEST_ASSERT_EQUAL_UINT8(5, last_render.minutes);
    TEST_ASSERT_EQUAL_UINT8(5, last_render.red);
    TEST_ASSERT_EQUAL_UINT8(0, last_render.green);
    TEST_ASSERT_EQUAL_UINT8(0, last_render.blue);
}

void test_display_shows_minutes_for_nu_departure_text(void)
{
    DisplayState state {};
    state.system_state = SystemState::DEPARTURES;
    memcpy(state.departure_text, "Nu", sizeof("Nu"));
    state.walk_time_minutes = 5;
    state.gradient_minutes = 5;
    displaySetState(state);

    clearLastRender();
    displayUpdate();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RenderKind::DEPARTURE_MINUTES),
        static_cast<int>(last_render.kind)
    );
    TEST_ASSERT_EQUAL_UINT8(0, last_render.minutes);
    TEST_ASSERT_EQUAL_UINT8(5, last_render.red);
    TEST_ASSERT_EQUAL_UINT8(0, last_render.green);
    TEST_ASSERT_EQUAL_UINT8(0, last_render.blue);
}

void test_display_keeps_far_departure_green_after_walk_time_offset(void)
{
    DisplayState state {};
    state.system_state = SystemState::DEPARTURES;
    state.walk_time_minutes = 5;
    state.gradient_minutes = 5;
    memcpy(state.departure_text, "12 min", sizeof("12 min"));
    displaySetState(state);

    clearLastRender();
    displayUpdate();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RenderKind::DEPARTURE_MINUTES),
        static_cast<int>(last_render.kind)
    );
    TEST_ASSERT_EQUAL_UINT8(0, last_render.red);
    TEST_ASSERT_EQUAL_UINT8(5, last_render.green);
    TEST_ASSERT_EQUAL_UINT8(0, last_render.blue);
}

void test_display_turns_red_at_walk_time_plus_buffer(void)
{
    DisplayState state {};
    state.system_state = SystemState::DEPARTURES;
    state.walk_time_minutes = 5;
    state.gradient_minutes = 5;
    memcpy(state.departure_text, "7 min", sizeof("7 min"));
    displaySetState(state);

    clearLastRender();
    displayUpdate();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RenderKind::DEPARTURE_MINUTES),
        static_cast<int>(last_render.kind)
    );
    TEST_ASSERT_EQUAL_UINT8(5, last_render.red);
    TEST_ASSERT_EQUAL_UINT8(0, last_render.green);
    TEST_ASSERT_EQUAL_UINT8(0, last_render.blue);
}

void test_display_stays_near_red_just_above_red_cutoff(void)
{
    DisplayState state {};
    state.system_state = SystemState::DEPARTURES;
    state.walk_time_minutes = 5;
    state.gradient_minutes = 5;
    memcpy(state.departure_text, "8 min", sizeof("8 min"));
    displaySetState(state);

    clearLastRender();
    displayUpdate();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RenderKind::DEPARTURE_MINUTES),
        static_cast<int>(last_render.kind)
    );
    TEST_ASSERT_EQUAL_UINT8(4, last_render.red);
    TEST_ASSERT_EQUAL_UINT8(1, last_render.green);
    TEST_ASSERT_EQUAL_UINT8(0, last_render.blue);
}

void test_display_uses_low_brightness_setting_for_departure_colors(void)
{
    DisplayState state {};
    state.system_state = SystemState::DEPARTURES;
    state.brightness = DisplayBrightness::LOW;
    state.walk_time_minutes = 5;
    state.gradient_minutes = 5;
    memcpy(state.departure_text, "12 min", sizeof("12 min"));
    displaySetState(state);

    clearLastRender();
    displayUpdate();

    TEST_ASSERT_EQUAL_UINT8(kDisplayBrightnessLowValue, last_render.brightness);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RenderKind::DEPARTURE_MINUTES),
        static_cast<int>(last_render.kind)
    );
    TEST_ASSERT_EQUAL_UINT8(0, last_render.red);
    TEST_ASSERT_EQUAL_UINT8(1, last_render.green);
    TEST_ASSERT_EQUAL_UINT8(0, last_render.blue);
}

void test_display_shows_clock_for_clock_departure_text(void)
{
    DisplayState state {};
    state.system_state = SystemState::DEPARTURES;
    memcpy(state.departure_text, "12:34", sizeof("12:34"));
    displaySetState(state);

    clearLastRender();
    displayUpdate();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RenderKind::DEPARTURE_CLOCK),
        static_cast<int>(last_render.kind)
    );
    TEST_ASSERT_EQUAL_STRING("12:34", last_render.text);
}

void test_display_shows_question_for_unknown_departure_text(void)
{
    DisplayState state {};
    state.system_state = SystemState::DEPARTURES;
    memcpy(state.departure_text, "Soon", sizeof("Soon"));
    displaySetState(state);

    clearLastRender();
    displayUpdate();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RenderKind::DEPARTURE_UNKNOWN),
        static_cast<int>(last_render.kind)
    );
}

void test_display_shows_no_departures_for_empty_selected_direction(void)
{
    DisplayState state {};
    state.system_state = SystemState::DEPARTURES;
    displaySetState(state);

    clearLastRender();
    displayUpdate();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RenderKind::DEPARTURE_UNKNOWN),
        static_cast<int>(last_render.kind)
    );
}
