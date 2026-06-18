#include "unity.h"

void test_display_boot_animation_runs_full_length_before_rendering_state(void);
void test_display_direction_animation_finishes_after_state_changes(void);
void test_display_shows_minutes_for_numeric_departure_text(void);
void test_display_shows_minutes_for_now_departure_text(void);
void test_display_shows_clock_for_clock_departure_text(void);
void test_display_shows_question_for_unknown_departure_text(void);

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_display_boot_animation_runs_full_length_before_rendering_state);
    RUN_TEST(test_display_direction_animation_finishes_after_state_changes);
    RUN_TEST(test_display_shows_minutes_for_numeric_departure_text);
    RUN_TEST(test_display_shows_minutes_for_now_departure_text);
    RUN_TEST(test_display_shows_clock_for_clock_departure_text);
    RUN_TEST(test_display_shows_question_for_unknown_departure_text);

    return UNITY_END();
}
