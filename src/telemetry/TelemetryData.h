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
enum class ArmState {
    DISARMED,
    ARMED
};

enum class NavState {
    MANUAL,
    POSCTL,
    AUTO_TAKEOFF,
    AUTO_LOITER,
    AUTO_MISSION,
    AUTO_LAND,
    UNKNOWN
};
enum class FlightPhase {
    UNKNOWN,
    ON_GROUND,
    TAKING_OFF,
    IN_AIR,
    LANDING
};

enum class CommandBlockReason {
    NONE,
    EKF_NOT_READY,
    BATTERY_LOW,
    FAILSAFE_ACTIVE,
    VEHICLE_NOT_ARMED,
    VEHICLE_NOT_LANDED,
    UNKNOWN
};


struct TelemetryData {

    // ---------- Command ACK ----------
    CommandAckData last_command_ack;

    // ---------- Connection ----------
    bool heartbeat_received = false;
    uint8_t system_id = 0;
    uint8_t component_id = 0;

    std::chrono::steady_clock::time_point last_heartbeat_time;
    std::chrono::steady_clock::time_point last_mavlink_rx_time;

    // ---------- Safety ----------
    bool ekf_ok = false;
    bool battery_ok = false;
    bool ekf_received = false;
    bool battery_received = false;

    // ---------- Vehicle Awareness ----------
    ArmState arm_state = ArmState::DISARMED;
    NavState nav_state = NavState::UNKNOWN;
    bool in_failsafe = false;

    FlightPhase flight_phase = FlightPhase::UNKNOWN;
    bool extended_state_received = false;

    // ✅ Phase 5: WHY command is blocked
    CommandBlockReason last_block_reason =
        CommandBlockReason::NONE;
    // ---------- Phase 5: STATUSTEXT ----------
    bool preflight_ok = false;
    std::chrono::steady_clock::time_point last_preflight_clear_time;

    // Last human-readable reason from PX4
    char last_status_text[50] = {0};


    // ---------- Derived ----------
    bool isTelemetryReady() const {
        return heartbeat_received &&
               ekf_received &&
               battery_received &&
               extended_state_received;
    }

    bool isAirborne() const {
        return flight_phase == FlightPhase::IN_AIR;
    }

    bool isLanded() const {
        return flight_phase == FlightPhase::ON_GROUND;
    }
};


