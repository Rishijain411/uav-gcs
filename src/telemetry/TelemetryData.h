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

/* ---------- Command Block Reasons ---------- */
enum class CommandBlockReason {
    NONE,
    FAILSAFE_ACTIVE,
    EKF_NOT_READY,
    BATTERY_LOW,
    VEHICLE_NOT_LANDED,
    VEHICLE_NOT_ARMED,
    MISSION_STATE_BLOCK
};

/* ---------- Human-readable mapping (AUDIT SAFE) ---------- */
inline const char* toString(CommandBlockReason reason) {
    switch (reason) {
    case CommandBlockReason::NONE:
        return "NONE";
    case CommandBlockReason::FAILSAFE_ACTIVE:
        return "FAILSAFE_ACTIVE";
    case CommandBlockReason::EKF_NOT_READY:
        return "EKF_NOT_READY";
    case CommandBlockReason::BATTERY_LOW:
        return "BATTERY_LOW";
    case CommandBlockReason::VEHICLE_NOT_LANDED:
        return "VEHICLE_NOT_LANDED";
    case CommandBlockReason::VEHICLE_NOT_ARMED:
        return "VEHICLE_NOT_ARMED";
    case CommandBlockReason::MISSION_STATE_BLOCK:
        return "MISSION_STATE_BLOCK";
    default:
        return "UNKNOWN_BLOCK_REASON";
    }
}

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

    // ---------- Phase 5: Last Command Block Reason ----------
    CommandBlockReason last_block_reason =
        CommandBlockReason::NONE;

    // ---------- Preflight Readiness (Derived, Authoritative) ----------
    bool isPreflightReady() const {
        return
            heartbeat_received &&
            ekf_received &&
            ekf_ok &&
            battery_received &&
            battery_ok &&
            extended_state_received &&
            !in_failsafe &&
            arm_state == ArmState::DISARMED &&
            flight_phase == FlightPhase::ON_GROUND;
    }

    // ---------- Last PX4 status text ----------
    char last_status_text[50] = {0};

    // ---------- Target / Engagement Telemetry ----------
    bool target_detected = false;
    double target_confidence = 0.0;
    double target_range_m = 0.0;
    double target_closing_speed = 0.0;

    // ---------- Derived Helpers ----------
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
