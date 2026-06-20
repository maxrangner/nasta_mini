#include "settings.h"
#include "unity.h"

void test_decideBootMode_returns_setup_when_needs_setup_is_true(void)
{
    DeviceSettings settings {};
    settings.setup.needs_setup = true;

    BootMode mode = decideBootMode(settings);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BootMode::SETUP),
        static_cast<int>(mode)
    );
}

void test_decideBootMode_returns_setup_when_wifi_ssid_is_empty(void)
{
    DeviceSettings settings {};
    settings.setup.needs_setup = false;
    settings.site.site_id = 1;
    settings.wifi.ssid[0] = '\0';

    BootMode mode = decideBootMode(settings);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BootMode::SETUP),
        static_cast<int>(mode)
    );
}

void test_decideBootMode_returns_setup_when_site_id_is_zero(void)
{
    DeviceSettings settings {};
    settings.setup.needs_setup = false;
    settings.wifi.ssid[0] = 'A';
    settings.site.site_id = 0;

    BootMode mode = decideBootMode(settings);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BootMode::SETUP),
        static_cast<int>(mode)
    );
}

void test_decideBootMode_returns_normal_when_settings_are_valid(void)
{
    DeviceSettings settings {};
    settings.setup.needs_setup = false;
    settings.wifi.ssid[0] = 'A';
    settings.site.site_id = 1;

    BootMode mode = decideBootMode(settings);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BootMode::NORMAL),
        static_cast<int>(mode)
    );
}

void test_applySetupConfig_copies_walk_time_minutes(void)
{
    DeviceSettings settings {};
    SetupConfig config {};
    config.wifi.ssid[0] = 'A';
    config.site.site_id = 1;
    config.startup_direction = 2;
    config.walk_time_minutes = 7;
    config.gradient_minutes = 9;
    config.brightness = DisplayBrightness::LOW;

    applySetupConfig(settings, config);

    TEST_ASSERT_EQUAL_UINT8(7, settings.walk_time_minutes);
    TEST_ASSERT_EQUAL_UINT8(9, settings.gradient_minutes);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayBrightness::LOW),
        static_cast<int>(settings.brightness)
    );
    TEST_ASSERT_FALSE(settings.setup.needs_setup);
}
