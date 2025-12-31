#include "command/CommandManager.h"
#include "command/MavlinkCommandSender.h"

#include <iostream>
#include <chrono>

static constexpr int COMMAND_ACK_TIMEOUT_MS = 1000;

// ----------------------------
// Phase 3B + 4.2 — Safety gating
// ----------------------------
bool CommandManager::isCommandAllowed(
    VehicleCommand cmd,
    SystemState state,
    const TelemetryData& telemetry) const {

    if (state == SystemState::FAILSAFE)
        return false;

    switch (cmd) {

    case VehicleCommand::ARM:
        // Telemetry must be READY first
        if (!telemetry.ekf_received || !telemetry.battery_received) {
            std::cout << "[CMD] ARM blocked: telemetry not ready" << std::endl;
            return false;
        }

        if (!telemetry.ekf_ok || !telemetry.battery_ok) {
            std::cout << "[CMD] ARM blocked by safety (EKF/Battery)" << std::endl;
            return false;
        }

        return state == SystemState::CONNECTED;

    case VehicleCommand::DISARM:
        return state == SystemState::ARMED;

    case VehicleCommand::TAKEOFF:
        return state == SystemState::ARMED && telemetry.ekf_ok;

    case VehicleCommand::LAND:
        return state == SystemState::ARMED;

    default:
        return false;
    }
}

// ----------------------------
// Phase 4.2 — Request command
// ----------------------------
bool CommandManager::requestCommand(
    VehicleCommand cmd,
    SystemState state,
    const TelemetryData& telemetry) {

    if (active_command_.has_value()) {
        std::cout << "[CMD] Rejected: another command active" << std::endl;
        return false;
    }

    if (!isCommandAllowed(cmd, state, telemetry)) {
        return false;
    }

    if (!sender_) {
        std::cout << "[CMD] No command sender bound" << std::endl;
        return false;
    }

    TrackedCommand tc;
    tc.logical_cmd = cmd;
    tc.mavlink_cmd_id = mapToMavlinkCommand(cmd);
    tc.retry_count = 0;
    tc.max_retries = 3;
    tc.last_sent_time = std::chrono::steady_clock::now();

    sender_->sendRawCommand(tc.mavlink_cmd_id);

    std::cout << "[CMD] " << tc.mavlink_cmd_id << " SENT" << std::endl;

    active_command_ = tc;
    return true;
}

// ----------------------------
// Phase 4.2 — Update loop
// ----------------------------
void CommandManager::update(
    const TelemetryData& telemetry,
    SystemState& state) {

    if (!active_command_.has_value())
        return;

    handleAck(telemetry, state);
    handleRetry();
}

// ----------------------------
// ACK handling
// ----------------------------
void CommandManager::handleAck(
    const TelemetryData& telemetry,
    SystemState& state) {

    if (!telemetry.last_command_ack.valid)
        return;

    auto& cmd = active_command_.value();

    if (telemetry.last_command_ack.command_id != cmd.mavlink_cmd_id)
        return;

    const_cast<CommandAckData&>(telemetry.last_command_ack).valid = false;

    if (telemetry.last_command_ack.result == MAV_RESULT_ACCEPTED) {

        std::cout << "[CMD] " << cmd.mavlink_cmd_id
                  << " ACK ACCEPTED" << std::endl;

        if (cmd.logical_cmd == VehicleCommand::ARM)
            state = SystemState::ARMED;
        else if (cmd.logical_cmd == VehicleCommand::DISARM ||
                 cmd.logical_cmd == VehicleCommand::LAND)
            state = SystemState::CONNECTED;
    } else {
        std::cout << "[CMD] " << cmd.mavlink_cmd_id
                  << " ACK REJECTED (" << int(telemetry.last_command_ack.result)
                  << ")" << std::endl;
    }

    active_command_.reset();
}

// ----------------------------
// Retry handling
// ----------------------------
void CommandManager::handleRetry() {

    if (!sender_ || !active_command_)
        return;

    auto& cmd = active_command_.value();

    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - cmd.last_sent_time).count();

    if (elapsed_ms < COMMAND_ACK_TIMEOUT_MS)
        return;

    if (cmd.retry_count >= cmd.max_retries) {
        std::cout << "[CMD] " << cmd.mavlink_cmd_id
                  << " TIMEOUT — giving up" << std::endl;
        active_command_.reset();
        return;
    }

    cmd.retry_count++;
    cmd.last_sent_time = now;

    sender_->sendRawCommand(cmd.mavlink_cmd_id);

    std::cout << "[CMD] " << cmd.mavlink_cmd_id
              << " RETRY " << cmd.retry_count << std::endl;
}

// ----------------------------
// Command mapping (PURE)
// ----------------------------
uint16_t CommandManager::mapToMavlinkCommand(
    VehicleCommand cmd) const {

    switch (cmd) {
    case VehicleCommand::ARM:
    case VehicleCommand::DISARM:
        return MAV_CMD_COMPONENT_ARM_DISARM;

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
