#include "command/CommandManager.h"
#include "command/MavlinkCommandSender.h"

#include <iostream>
#include <chrono>

using namespace std;

static constexpr int COMMAND_ACK_TIMEOUT_MS = 3000;
static CommandBlockReason last_printed_reason = CommandBlockReason::NONE;

// -------------------------------------------------
bool CommandManager::isCommandAllowed(
    VehicleCommand cmd,
    SystemState state,
    const TelemetryData& telemetry,
    CommandBlockReason& out_reason) const {

    out_reason = CommandBlockReason::NONE;

    if (state == SystemState::FAILSAFE) {
        out_reason = CommandBlockReason::FAILSAFE_ACTIVE;
        return false;
    }

    switch (cmd) {

    case VehicleCommand::ARM:
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
        if (telemetry.arm_state != ArmState::DISARMED) {
            out_reason = CommandBlockReason::VEHICLE_NOT_ARMED;
            return false;
        }
        return true;

    case VehicleCommand::SET_MODE_AUTO:
        return telemetry.arm_state == ArmState::ARMED;

    case VehicleCommand::TAKEOFF:
        return telemetry.arm_state == ArmState::ARMED &&
               telemetry.isLanded();

    case VehicleCommand::LAND:
        return telemetry.isAirborne();

    default:
        return false;
    }
}

// -------------------------------------------------
bool CommandManager::requestCommand(
    VehicleCommand cmd,
    SystemState state,
    const TelemetryData& telemetry) {

    if (active_command_ || !sender_)
        return false;

    const CommandDefinition* def = findCommand(cmd);
    if (!def)
        return false;

    if (!def->allowed(telemetry)) {
        cout << "[CMD BLOCKED] Rule denied\n";
        return false;
    }

    TrackedCommand tc;
    tc.logical_cmd = cmd;
    tc.mavlink_cmd_id = def->mavlink_id;
    tc.retry_count = 0;
    tc.max_retries = 3;
    tc.last_sent_time = chrono::steady_clock::now();

    sender_->sendRawCommand(tc.mavlink_cmd_id);
    cout << "[CMD] " << tc.mavlink_cmd_id << " SENT\n";

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
    handleRetry(telemetry);  
}


// -------------------------------------------------
void CommandManager::handleRetry(const TelemetryData& telemetry) {

    if (!sender_ || !active_command_)
        return;

    auto& cmd = active_command_.value();

    auto elapsed =
        chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - cmd.last_sent_time).count();

    if (elapsed < COMMAND_ACK_TIMEOUT_MS)
        return;

    if (cmd.retry_count >= cmd.max_retries) {
        cout << "[CMD] TIMEOUT — giving up\n";
        active_command_.reset();
        return;
    }

    cmd.retry_count++;
    cmd.last_sent_time = chrono::steady_clock::now();
    sender_->sendRawCommand(cmd.mavlink_cmd_id);

    cout << "[CMD] RETRY " << cmd.retry_count << endl;
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
