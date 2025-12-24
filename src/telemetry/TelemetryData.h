#pragma once
#include <cstdint>

struct TelemetryData {
    uint8_t system_id = 0;
    uint8_t component_id = 0;
    bool heartbeat_received = false;
};
