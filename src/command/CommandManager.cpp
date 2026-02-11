#include "command/CommandManager.h"
#include "command/MavlinkCommandSender.h"

#include <iostream>
#include <chrono>
#include "mission/MissionAbortReason.h"
static constexpr int COMMAND_ACK_TIMEOUT_MS = 1000;

// --------------------------------------------------
// Phase 3B + 4.2 — Safety gating (AUTHORITATIVE)
// --------------------------------------------------
bool CommandManager::isCommandAllowed(
    VehicleCommand cmd,
    SystemState system_state,
    mission::MissionState mission_state,
    const TelemetryData& telemetry,
    CommandBlockReason& out_reason) const
{
    out_reason = CommandBlockReason::NONE;

    if (system_state == SystemState::FAILSAFE) {
        out_reason = CommandBlockReason::FAILSAFE_ACTIVE;
        return false;
    }

    switch (cmd) {

    case VehicleCommand::ARM:
        if (!telemetry.isPreflightReady()) {
            out_reason = telemetry.last_block_reason;
            return false;
        }
        return true;

    case VehicleCommand::DISARM:
        return telemetry.arm_state == ArmState::ARMED;

    case VehicleCommand::TAKEOFF:
        if (telemetry.arm_state != ArmState::ARMED) {
            out_reason = CommandBlockReason::VEHICLE_NOT_ARMED;
            return false;
        }
        if (!telemetry.isLanded()) {
            out_reason = CommandBlockReason::VEHICLE_NOT_LANDED;
            return false;
        }
        return true;

    case VehicleCommand::SET_MODE_AUTO:
        return MAV_CMD_DO_SET_MODE;

    case VehicleCommand::SET_MODE_RTL:
    case VehicleCommand::SET_MODE_LOITER:
        if (telemetry.arm_state != ArmState::ARMED) {
            out_reason = CommandBlockReason::VEHICLE_NOT_ARMED;
            return false;
        }
        return true;

    case VehicleCommand::LAND:
        if (telemetry.arm_state != ArmState::ARMED) {
            out_reason = CommandBlockReason::VEHICLE_NOT_ARMED;
            return false;
        }
        return true;

    default:
        out_reason = CommandBlockReason::MISSION_STATE_BLOCK;
        return false;
    }
}

// --------------------------------------------------
// Phase 4.2 — Request command
// --------------------------------------------------
bool CommandManager::requestCommand(
    VehicleCommand cmd,
    SystemState system_state,
    mission::MissionState mission_state,
    const TelemetryData& telemetry)
{
    if (active_command_.has_value()) {
        std::cout << "[CMD] Rejected: command already active\n";
        return false;
    }

    if (!sender_) {
        std::cout << "[CMD] Rejected: no sender\n";
        return false;
    }

    CommandBlockReason block_reason = CommandBlockReason::NONE;

    if (!isCommandAllowed(
            cmd,
            system_state,
            mission_state,
            telemetry,
            block_reason))
    {
        std::cout << "[CMD BLOCKED] reason="
                  << toString(block_reason)
                  << std::endl;
        return false;
    }

    // ----------------------------
    // Create tracked command
    // ----------------------------
    TrackedCommand tc;
    tc.logical_cmd = cmd;
    tc.mavlink_cmd_id = mapToMavlinkCommand(cmd);

    std::cout << "[DEBUG] logical_cmd=" << int(cmd)
              << " mapped_mavlink_id="
              << tc.mavlink_cmd_id
              << std::endl;

    tc.retry_count = 0;
    tc.max_retries = 3;
    tc.last_sent_time = std::chrono::steady_clock::now();

    sender_->sendRawCommand(tc.mavlink_cmd_id);

    std::cout << "[CMD SENT] MAV_CMD="
              << tc.mavlink_cmd_id
              << std::endl;

    active_command_ = tc;
    command_timed_out_ = false;

    return true;
}


// --------------------------------------------------
// Phase 4.2 — Update loop
// --------------------------------------------------
void CommandManager::update(
    const TelemetryData& telemetry,
    SystemState& system_state)
{
    if (!active_command_)
        return;

    handleAck(telemetry, system_state);
    handleRetry();
}

// --------------------------------------------------
// ACK handling
// --------------------------------------------------
void CommandManager::handleAck(
    const TelemetryData& telemetry,
    SystemState& system_state)
{
    if (!telemetry.last_command_ack.valid || !active_command_)
        return;

    auto& cmd = active_command_.value();

    if (telemetry.last_command_ack.command_id != cmd.mavlink_cmd_id)
        return;

    const_cast<CommandAckData&>(telemetry.last_command_ack).valid = false;

    if (telemetry.last_command_ack.result == MAV_RESULT_ACCEPTED) {
        std::cout << "[CMD ACK] ACCEPTED\n";

        if (cmd.logical_cmd == VehicleCommand::DISARM)
            system_state = SystemState::CONNECTED;
    } else {
        std::cout << "[CMD ACK] REJECTED\n";
    }

    active_command_.reset();
}

// --------------------------------------------------
// Retry handling (AUTHORITATIVE)
// --------------------------------------------------
void CommandManager::handleRetry()
{
    if (!sender_ || !active_command_)
        return;

    auto& cmd = active_command_.value();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - cmd.last_sent_time).count();

    if (elapsed < COMMAND_ACK_TIMEOUT_MS)
        return;

    if (cmd.retry_count >= cmd.max_retries) {
        std::cout << "[CMD TIMEOUT] Giving up\n";
        command_timed_out_ = true;
        active_command_.reset();
        return;
    }

    cmd.retry_count++;
    cmd.last_sent_time = std::chrono::steady_clock::now();
    sender_->sendRawCommand(cmd.mavlink_cmd_id);

    std::cout << "[CMD RETRY] count=" << cmd.retry_count << std::endl;
}
// ----------------------------
//  Search waypoint forwarding
// ----------------------------
void CommandManager::sendSearchWaypoint(const GeoPoint& waypoint)
{
    if (!sender_) {
        std::cout << "[CMD] No command sender bound (waypoint dropped)\n";
        return;
    }

    sender_->sendSearchWaypoint(waypoint);
}


// --------------------------------------------------
// MAVLink command mapping (PURE)
// --------------------------------------------------
uint16_t CommandManager::mapToMavlinkCommand(
    VehicleCommand cmd) const {

    switch (cmd) {

    case VehicleCommand::ARM:
    case VehicleCommand::DISARM:
        return MAV_CMD_COMPONENT_ARM_DISARM;

    case VehicleCommand::SET_MODE_AUTO:
        return MAV_CMD_DO_SET_MODE;

    case VehicleCommand::SET_MODE_RTL:
        return MAV_CMD_NAV_RETURN_TO_LAUNCH;

    case VehicleCommand::SET_MODE_LOITER:
        return MAV_CMD_NAV_LOITER_UNLIM;

    case VehicleCommand::TAKEOFF:
        return MAV_CMD_NAV_TAKEOFF;

    case VehicleCommand::LAND:
        return MAV_CMD_NAV_LAND;

    case VehicleCommand::SET_HOME:
        return MAV_CMD_DO_SET_HOME;

    case VehicleCommand::NONE:
    default:
        std::cerr << "[CMD ERROR] Invalid VehicleCommand mapping\n";
        return 0;
    }
}

bool CommandManager::hasActiveCommand() const {
    return active_command_.has_value();
}
