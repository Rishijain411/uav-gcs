#pragma once

#include <optional>
#include <chrono>

#include "VehicleCommand.h"
#include "core/SystemState.h"
#include "telemetry/TelemetryData.h"

class MavlinkCommandSender;  // forward declaration

class CommandManager {
public:
    bool isCommandAllowed(
        VehicleCommand cmd,
        SystemState state,
        const TelemetryData& telemetry) const;

    bool requestCommand(
        VehicleCommand cmd,
        SystemState state,
        const TelemetryData& telemetry);

    void update(
        const TelemetryData& telemetry,
        SystemState& state);

    bool hasActiveCommand() const;

    // ✅ Phase 4.2: lifecycle notification
    bool commandFinished() const { return command_finished_; }
    void clearCommandFinished() { command_finished_ = false; }

    void setCommandSender(MavlinkCommandSender* sender) {
        sender_ = sender;
    }

private:
    struct TrackedCommand {
        VehicleCommand logical_cmd;
        uint16_t mavlink_cmd_id;
        int retry_count = 0;
        int max_retries = 3;
        bool awaiting_ack = true;
        std::chrono::steady_clock::time_point last_sent_time;
    };

    uint16_t mapToMavlinkCommand(VehicleCommand cmd) const;

    void handleAck(
        const TelemetryData& telemetry,
        SystemState& state);

    void handleRetry();

    std::optional<TrackedCommand> active_command_;

    MavlinkCommandSender* sender_ = nullptr;

    // ✅ ADD THIS LINE (THIS IS WHAT WAS MISSING)
    bool command_finished_ = false;
};
