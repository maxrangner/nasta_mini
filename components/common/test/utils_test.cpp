#include "utils.h"
#include "unity.h"

void setUp(void)
{

}

TEST_CASE("returns TransportMode::METRO when transport_mode is 'METRO'", "[utils]")
{
        const char* transport_mode = "METRO";
        TransportMode mode = toTransportMode(transport_mode);
        TEST_ASSERT_EQUAL(TransportMode::METRO, mode);
}

TEST_CASE("returns 'METRO' when transport_mode is TransportMode::METRO", "[utils]")
{
        TransportMode transport_mode = TransportMode::METRO;
        const char* mode = toTransportModeApiString(transport_mode);
        TEST_ASSERT_EQUAL_STRING("METRO", mode);
}

void tearDown(void) 
{

}