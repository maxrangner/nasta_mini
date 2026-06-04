#include "utils.h"
#include <string.h>

TransportMode toTransportMode(const char* transport_mode) {
    if (strcmp(transport_mode, "METRO") == 0) return TransportMode::METRO;
    if (strcmp(transport_mode, "TRAM") == 0) return TransportMode::TRAM;
    if (strcmp(transport_mode, "TRAIN") == 0) return TransportMode::TRAIN;
    if (strcmp(transport_mode, "BUS") == 0) return TransportMode::BUS;
    if (strcmp(transport_mode, "SHIP") == 0) return TransportMode::SHIP;
    if (strcmp(transport_mode, "FERRY") == 0) return TransportMode::FERRY;
    if (strcmp(transport_mode, "TAXI") == 0) return TransportMode::TAXI;
    return TransportMode::UNKNOWN;
}

const char* toTransportModeApiString(TransportMode transport_mode) {
    switch (transport_mode) {
        case TransportMode::METRO:
            return "METRO";
        case TransportMode::TRAM:
            return "TRAM";
        case TransportMode::TRAIN:
            return "TRAIN";
        case TransportMode::BUS:
            return "BUS";
        case TransportMode::SHIP:
            return "SHIP";
        case TransportMode::FERRY:
            return "FERRY";
        case TransportMode::TAXI:
            return "TAXI";
        case TransportMode::UNKNOWN:
        default:
            return "";
    }
}
