#include "unity.h"

void test_display_boot_animation_runs_full_length_before_rendering_state(void);
void test_display_direction_animation_finishes_after_state_changes(void);
void test_display_shows_minutes_for_numeric_departure_text(void);
void test_display_shows_minutes_for_nu_departure_text(void);
void test_display_uses_white_for_departure_five_minutes_after_walk_time(void);
void test_display_moves_toward_green_one_minute_after_walk_time(void);
void test_display_uses_midpoint_color_two_minutes_after_walk_time(void);
void test_display_uses_green_four_minutes_after_walk_time(void);
void test_display_clamps_to_white_beyond_top_of_color_map(void);
void test_display_keeps_global_low_brightness_with_direct_color_map(void);
void test_display_shows_clock_for_clock_departure_text(void);
void test_display_shows_question_for_unknown_departure_text(void);
void test_display_shows_no_departures_for_empty_selected_direction(void);

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_display_boot_animation_runs_full_length_before_rendering_state);
    RUN_TEST(test_display_direction_animation_finishes_after_state_changes);
    RUN_TEST(test_display_shows_minutes_for_numeric_departure_text);
    RUN_TEST(test_display_shows_minutes_for_nu_departure_text);
    RUN_TEST(test_display_uses_white_for_departure_five_minutes_after_walk_time);
    RUN_TEST(test_display_moves_toward_green_one_minute_after_walk_time);
    RUN_TEST(test_display_uses_midpoint_color_two_minutes_after_walk_time);
    RUN_TEST(test_display_uses_green_four_minutes_after_walk_time);
    RUN_TEST(test_display_clamps_to_white_beyond_top_of_color_map);
    RUN_TEST(test_display_keeps_global_low_brightness_with_direct_color_map);
    RUN_TEST(test_display_shows_clock_for_clock_departure_text);
    RUN_TEST(test_display_shows_question_for_unknown_departure_text);
    RUN_TEST(test_display_shows_no_departures_for_empty_selected_direction);

    return UNITY_END();
}
