#include "common/message_types.h"
#include "unity.h"

void setUp(void)
{

}

TEST_CASE("returns BootMode::SETUP when settings.setup.needs_setup is true", "[settings]")
{
        // DeviceSettings settings {};
        // settings.setup.needs_setup = true;
        // BootMode mode = decideBootMode(settings);
        // TEST_ASSERT_EQUAL(BootMode::SETUP, mode);

        SystemEvent event {};
        event.type = SystemEventType::NETWORK_STATE;
        event.network_state.phase = NetworkPhase::CONNECTING;
        
        // SystemState new_state = SystemState::CONNECTING;
}

void tearDown(void) 
{

}