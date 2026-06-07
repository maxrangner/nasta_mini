#include "settings.h"
#include "unity.h"

void setUp(void)
{

}

TEST_CASE("returns BootMode::SETUP when settings.setup.needs_setup is true", "[settings]")
{
        DeviceSettings settings {};
        settings.setup.needs_setup = true;
        BootMode mode = decideBootMode(settings);
        TEST_ASSERT_EQUAL(BootMode::SETUP, mode);
}

TEST_CASE("returns BootMode::SETUP when settings.wifi.ssid is empty", "[settings]")
{
        DeviceSettings settings {};
        settings.setup.needs_setup = false;
        settings.site.site_id = 1;
        settings.wifi.ssid[0] = '\0';
        BootMode mode = decideBootMode(settings);
        TEST_ASSERT_EQUAL(BootMode::SETUP, mode);
}

TEST_CASE("returns BootMode::SETUP when settings.site.site_id is 0", "[settings]")
{
        DeviceSettings settings {};
        settings.setup.needs_setup = false;
        settings.wifi.ssid[0] = 'A';
        settings.site.site_id = 0;
        BootMode mode = decideBootMode(settings);
        TEST_ASSERT_EQUAL(BootMode::SETUP, mode);
}

TEST_CASE("returns BootMode::NORMAL when all settings are valid", "[settings]")
{
        DeviceSettings settings {};
        settings.setup.needs_setup = false;
        settings.wifi.ssid[0] = 'A';
        settings.site.site_id = 1;
        BootMode mode = decideBootMode(settings);
        TEST_ASSERT_EQUAL(BootMode::NORMAL, mode);
}

void tearDown(void) 
{

}