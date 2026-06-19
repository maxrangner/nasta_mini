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
    uint32_t frame = 0;
    uint8_t minutes = 0;
    char text[6] = {};
};

static RenderCall last_render {};

static void clearLastRender() {
    last_render = RenderCall {};
}

void LedMatrix::init() {
    clearLastRender();
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

void LedMatrix::showDepartureMinutes(uint8_t minutes) {
    clearLastRender();
    last_render.kind = RenderKind::DEPARTURE_MINUTES;
    last_render.minutes = minutes;
}

void LedMatrix::showDepartureClock(const char* time_str, uint32_t frame) {
    clearLastRender();
    last_render.kind = RenderKind::DEPARTURE_CLOCK;
    last_render.frame = frame;

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
    memcpy(state.departure_text, "5 min", sizeof("5 min"));
    displaySetState(state);

    clearLastRender();
    displayUpdate();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RenderKind::DEPARTURE_MINUTES),
        static_cast<int>(last_render.kind)
    );
    TEST_ASSERT_EQUAL_UINT8(5, last_render.minutes);
}

void test_display_shows_minutes_for_now_departure_text(void)
{
    DisplayState state {};
    state.system_state = SystemState::DEPARTURES;
    memcpy(state.departure_text, "Nu", sizeof("Nu"));
    displaySetState(state);

    clearLastRender();
    displayUpdate();

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RenderKind::DEPARTURE_MINUTES),
        static_cast<int>(last_render.kind)
    );
    TEST_ASSERT_EQUAL_UINT8(0, last_render.minutes);
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
