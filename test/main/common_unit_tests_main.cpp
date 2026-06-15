#include "unity.h"

void test_decideBootMode_returns_setup_when_needs_setup_is_true(void);
void test_decideBootMode_returns_setup_when_wifi_ssid_is_empty(void);
void test_decideBootMode_returns_setup_when_site_id_is_zero(void);
void test_decideBootMode_returns_normal_when_settings_are_valid(void);
void test_toTransportMode_returns_metro_for_metro_string(void);
void test_toTransportModeApiString_returns_metro_for_transport_mode_metro(void);

void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_decideBootMode_returns_setup_when_needs_setup_is_true);
    RUN_TEST(test_decideBootMode_returns_setup_when_wifi_ssid_is_empty);
    RUN_TEST(test_decideBootMode_returns_setup_when_site_id_is_zero);
    RUN_TEST(test_decideBootMode_returns_normal_when_settings_are_valid);
    RUN_TEST(test_toTransportMode_returns_metro_for_metro_string);
    RUN_TEST(test_toTransportModeApiString_returns_metro_for_transport_mode_metro);

    return UNITY_END();
}
