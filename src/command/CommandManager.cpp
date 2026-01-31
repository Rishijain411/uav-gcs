#include "command/CommandManager.h"
#include "command/MavlinkCommandSender.h"
#include "telemetry/TelemetryData.h"
#include "utils/EnumStrings.h"
#include <iostream>
#include <chrono>

using namespace std;

static constexpr int COMMAND_ACK_TIMEOUT_MS = 3000;
static CommandBlockReason last_printed_reason = CommandBlockReason::NONE;

// -------------------------------------------------
bool CommandManager::isCommandAllowed(
    VehicleCommand cmd,
    SystemState system_state,
    mission::MissionState mission_state,
    const TelemetryData& telemetry,
    CommandBlockReason& out_reason) const {

    out_reason = CommandBlockReason::NONE;

    // ---------------------------
    // MISSION AUTHORITY (PRD)
    // ---------------------------
    switch (cmd) {

    case VehicleCommand::ARM:
        // ARM allowed in PREFLIGHT (before transition) or ARMED (after transition but before actual arm)
        if (mission_state != mission::MissionState::PREFLIGHT &&
            mission_state != mission::MissionState::ARMED) {
            out_reason = CommandBlockReason::MISSION_STATE_BLOCK;
            return false;
        }
        break;

    case VehicleCommand::TAKEOFF:
        if (mission_state != mission::MissionState::ARMED) {
            out_reason = CommandBlockReason::MISSION_STATE_BLOCK;
            return false;
        }
        break;

    case VehicleCommand::SET_MODE_AUTO:
        if (mission_state != mission::MissionState::TRANSIT &&
            mission_state != mission::MissionState::SEARCH) {
            out_reason = CommandBlockReason::MISSION_STATE_BLOCK;
            return false;
        }
        break;

    case VehicleCommand::LAND:
        if (mission_state != mission::MissionState::RTB) {
            out_reason = CommandBlockReason::MISSION_STATE_BLOCK;
            return false;
        }
        break;

    default:
        break;
    }

    // ---------------------------
    // SYSTEM SAFETY (LOCAL)
    // ---------------------------
    if (system_state == SystemState::FAILSAFE) {
        out_reason = CommandBlockReason::FAILSAFE_ACTIVE;
        return false;
    }

    // ---------------------------
    // VEHICLE HEALTH (PX4 INPUT)
    // ---------------------------
    switch (cmd) {

    case VehicleCommand::ARM:
        // Don't ARM if already armed
        if (telemetry.arm_state == ArmState::ARMED) {
            out_reason = CommandBlockReason::VEHICLE_NOT_ARMED;  // Reusing enum, but means "already armed"
            return false;
        }
        if (!telemetry.ekf_ok) {
            out_reason = CommandBlockReason::EKF_NOT_READY;
            return false;
        }
        if (!telemetry.battery_ok) {
            out_reason = CommandBlockReason::BATTERY_LOW;
            return false;
        }
        if (!telemetry.isLanded()) {
            out_reason = CommandBlockReason::VEHICLE_NOT_LANDED;
            return false;
        }
        return true;

    case VehicleCommand::TAKEOFF:
        return telemetry.arm_state == ArmState::ARMED &&
               telemetry.isLanded();

    case VehicleCommand::SET_MODE_AUTO:
        // SET_MODE_AUTO requires vehicle to be armed
        if (telemetry.arm_state != ArmState::ARMED) {
            out_reason = CommandBlockReason::VEHICLE_NOT_ARMED;
            return false;
        }
        return true;

    case VehicleCommand::LAND:
        return telemetry.isAirborne();

    default:
        return false;
    }
}


// -------------------------------------------------
bool CommandManager::requestCommand(
    VehicleCommand cmd,
    SystemState system_state,
    mission::MissionState mission_state,
    const TelemetryData& telemetry) {

    if (active_command_ || !sender_)
        return false;

    CommandBlockReason reason = CommandBlockReason::NONE;

    if (!isCommandAllowed(cmd, system_state, mission_state, telemetry, reason)) {
        cout << "[CMD BLOCKED] reason=" << toString(reason) << endl;
        return false;
    }


    TrackedCommand tc;
    tc.logical_cmd = cmd;
    tc.mavlink_cmd_id = mapToMavlinkCommand(cmd);
    tc.last_sent_time = chrono::steady_clock::now();

    sender_->sendRawCommand(tc.mavlink_cmd_id);
    cout << "[CMD SENT] mavlink_id=" << tc.mavlink_cmd_id << endl;

    active_command_ = tc;
    return true;
}


// -------------------------------------------------
void CommandManager::update(
    const TelemetryData& telemetry,
    SystemState& state) {

    if (!active_command_)
        return;

    handleAck(telemetry, state);
    handleRetry();

}


// -------------------------------------------------
void CommandManager::handleRetry() {

    if (!sender_ || !active_command_)
        return;

    auto& cmd = active_command_.value();

    auto elapsed =
        chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - cmd.last_sent_time).count();

    if (elapsed < 3000)
        return;

    if (cmd.retry_count >= cmd.max_retries) {
        cout << "[CMD TIMEOUT] aborting\n";
        active_command_.reset();
        return;
    }

    cmd.retry_count++;
    cmd.last_sent_time = chrono::steady_clock::now();
    sender_->sendRawCommand(cmd.mavlink_cmd_id);

    cout << "[CMD RETRY] count=" << cmd.retry_count << endl;
}

void CommandManager::handleAck(
    const TelemetryData& telemetry,
    SystemState& state) {

    if (!telemetry.last_command_ack.valid)
        return;

    auto& cmd = active_command_.value();

    if (telemetry.last_command_ack.command_id != cmd.mavlink_cmd_id)
        return;

    // consume ACK
    const_cast<CommandAckData&>(telemetry.last_command_ack).valid = false;

    if (telemetry.last_command_ack.result == MAV_RESULT_ACCEPTED) {

        cout << "[CMD] ACK ACCEPTED" << endl;

        switch (cmd.logical_cmd) {
        case VehicleCommand::ARM:
            state = SystemState::ARMED;
            break;

        case VehicleCommand::DISARM:
            state = SystemState::CONNECTED;
            break;

        default:
            break;
        }

    } else {
        cout << "[CMD] ACK REJECTED ("
             << int(telemetry.last_command_ack.result)
             << ")" << endl;
    }

    active_command_.reset();
}

// -------------------------------------------------
uint16_t CommandManager::mapToMavlinkCommand(
    VehicleCommand cmd) const {

    switch (cmd) {
    case VehicleCommand::ARM:
    case VehicleCommand::DISARM:
        return MAV_CMD_COMPONENT_ARM_DISARM;
    case VehicleCommand::SET_MODE_AUTO:
        return MAV_CMD_DO_SET_MODE;
    case VehicleCommand::TAKEOFF:
        return MAV_CMD_NAV_TAKEOFF;
    case VehicleCommand::LAND:
        return MAV_CMD_NAV_LAND;
    default:
        return 0;
    }
}

bool CommandManager::hasActiveCommand() const {
    return active_command_.has_value();
}
// -------------------------------------------------
// Phase C: Search waypoint publishing
// -------------------------------------------------
void CommandManager::sendSearchWaypoint(const GeoPoint& waypoint) {
    if (!sender_) {
        cout << "[SEARCH WP] No command sender available\n";
        return;
    }

    cout << "[SEARCH WP] Sending waypoint: lat=" << waypoint.lat
         << " lon=" << waypoint.lon
         << " alt=" << waypoint.alt << "\n";

    sender_->sendSearchWaypoint(waypoint);
}