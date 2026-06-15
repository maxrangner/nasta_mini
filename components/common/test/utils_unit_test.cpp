#include "utils.h"
#include "unity.h"

void test_toTransportMode_returns_metro_for_metro_string(void)
{
    const char* transport_mode = "METRO";

    TransportMode mode = toTransportMode(transport_mode);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransportMode::METRO),
        static_cast<int>(mode)
    );
}

void test_toTransportModeApiString_returns_metro_for_transport_mode_metro(void)
{
    TransportMode transport_mode = TransportMode::METRO;

    const char* mode = toTransportModeApiString(transport_mode);

    TEST_ASSERT_EQUAL_STRING("METRO", mode);
}
