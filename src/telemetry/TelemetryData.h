#pragma once

#include <chrono>
#include <cstdint>

extern "C" {
#include "mavlink/common/mavlink.h"
}

/* ---------- Phase 4: Command ACK ---------- */
struct CommandAckData {
    uint16_t command_id = 0;
    uint8_t result = MAV_RESULT_FAILED;
    bool valid = false;
};

struct TelemetryData {

    // ---------- Command ACK ----------
    CommandAckData last_command_ack;

    // ---------- Connection ----------
    bool heartbeat_received = false;

    uint8_t system_id = 0;
    uint8_t component_id = 0;

    // Last HEARTBEAT timestamp
    std::chrono::steady_clock::time_point last_heartbeat_time;

    // Last time *any* MAVLink message was received
    std::chrono::steady_clock::time_point last_mavlink_rx_time;

    // ---------- Safety ----------
    bool ekf_ok = false;
    bool battery_ok = false;
    bool ekf_received = false;
    bool battery_received = false;

    bool isTelemetryReady() const {
        return heartbeat_received &&
               ekf_received &&
               battery_received;
    }
};
