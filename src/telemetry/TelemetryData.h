#pragma once
#include <cstdint>
#include <chrono>

struct TelemetryData {
    uint8_t system_id = 0;
    uint8_t component_id = 0;
    bool heartbeat_received = false;

    std::chrono::steady_clock::time_point last_heartbeat_time;
};
