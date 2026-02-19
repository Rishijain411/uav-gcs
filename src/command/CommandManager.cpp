#include "command/CommandManager.h"
#include "command/MavlinkCommandSender.h"

#include <iostream>
#include <chrono>
#include "mission/MissionAbortReason.h"
static constexpr int COMMAND_ACK_TIMEOUT_MS = 5000;

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
        #ifdef SITL_MODE
            // In SITL, allow ARM even if preflight checks fail
            return true;
        #else
            if (!telemetry.isPreflightReady()) {
                out_reason = telemetry.last_block_reason;
                return false;
            }
            return true;
        #endif

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
    tc.max_retries = 5;
    tc.last_sent_time = std::chrono::steady_clock::now();

    // Special handling for ARM command to send force-arm parameter
    if (cmd == VehicleCommand::ARM) {
        sender_->sendArm();  // Uses force-arm param2=21196 in SITL_MODE
    } else {
        sender_->sendRawCommand(tc.mavlink_cmd_id);  // Generic command with no parameters
    }

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
// ACK handling - Comprehensive response code handling
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

    uint8_t result_code = telemetry.last_command_ack.result;
    const_cast<CommandAckData&>(telemetry.last_command_ack).valid = false;

    switch (result_code) {
    
    case MAV_RESULT_ACCEPTED: {
        std::cout << "[CMD ACK] ✓ ACCEPTED (cmd=" << cmd.mavlink_cmd_id << ")\n";
        
        // Set ARM ACK flag immediately so state machine can use it
        if (cmd.logical_cmd == VehicleCommand::ARM) {
            arm_ack_received_ = true;
            std::cout << "[CMD ACK] ARM ACK received - state machine can proceed\n";
        }
        
        if (cmd.logical_cmd == VehicleCommand::DISARM)
            system_state = SystemState::CONNECTED;
        
        active_command_.reset();
        break;
    }
    
    case MAV_RESULT_TEMPORARILY_REJECTED: {
        std::cout << "[CMD ACK] ⏳ TEMPORARILY_REJECTED - will retry "
                  << "(cmd=" << cmd.mavlink_cmd_id << ", retry=" 
                  << int(cmd.retry_count) << "/" << int(cmd.max_retries) << ")\n";
        
        // Do NOT reset active_command_ - let handleRetry() manage retry logic
        // This allows the existing retry mechanism to handle temporary rejections
        break;
    }
    
    case MAV_RESULT_DENIED: {
        std::cout << "[CMD ACK] ✗ DENIED - permanent failure "
                  << "(cmd=" << cmd.mavlink_cmd_id << ")\n";
        std::cout << "    [CMD ACK] Reason: Parameter/precondition invalid\n";
        
        // Permanent failure - do not retry
        active_command_.reset();
        command_timed_out_ = true;
        break;
    }
    
    case MAV_RESULT_UNSUPPORTED: {
        std::cout << "[CMD ACK] ⚠ UNSUPPORTED - command not supported by vehicle\n";
        active_command_.reset();
        command_timed_out_ = true;
        break;
    }
    
    case MAV_RESULT_FAILED: {
        std::cout << "[CMD ACK] ✗ FAILED - execution failed "
                  << "(cmd=" << cmd.mavlink_cmd_id << ")\n";
        std::cout << "    [CMD ACK] Reason: Non-recoverable error\n";
        
        active_command_.reset();
        command_timed_out_ = true;
        break;
    }
    
    case MAV_RESULT_IN_PROGRESS: {
        std::cout << "[CMD ACK] ⟳ IN_PROGRESS - awaiting completion "
                  << "(cmd=" << cmd.mavlink_cmd_id << ")\n";
        
        // Do NOT reset - command is still executing
        // Vehicle will send final ACK with ACCEPTED, DENIED, or FAILED
        break;
    }
    
    case MAV_RESULT_CANCELLED: {
        std::cout << "[CMD ACK] ⊘ CANCELLED by vehicle\n";
        active_command_.reset();
        break;
    }
    
    case MAV_RESULT_COMMAND_LONG_ONLY: {
        std::cout << "[CMD ACK] ⚠ COMMAND_LONG_ONLY - resend as COMMAND_LONG\n";
        active_command_.reset();
        break;
    }
    
    case MAV_RESULT_COMMAND_INT_ONLY: {
        std::cout << "[CMD ACK] ⚠ COMMAND_INT_ONLY - resend as COMMAND_INT\n";
        active_command_.reset();
        break;
    }
    
    default: {
        std::cout << "[CMD ACK] ❓ UNKNOWN result code: " << int(result_code) << "\n";
        active_command_.reset();
        break;
    }
    }
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
void CommandManager::sendAccelerationCommand(const Vector3D& accel) {
    if (!sender_) return;
    sender_->sendAccelerationSetpoint(accel);
}