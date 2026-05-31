#pragma once

#include "types.h"
#include <cstdint>

TransportMode toTransportMode(const char* transport_mode);
const char* toTransportModeApiString(TransportMode transport_mode);
