#pragma once

#include <optional>
#include <chrono>
#include <iostream>

using namespace std;

#include "VehicleCommand.h"
#include "core/SystemState.h"
#include "telemetry/TelemetryData.h"

class MavlinkCommandSender;

class CommandManager {
public:
    bool isCommandAllowed(
        VehicleCommand cmd,
        SystemState state,
        const TelemetryData& telemetry,
        CommandBlockReason& out_reason) const;

    bool requestCommand(
        VehicleCommand cmd,
        SystemState state,
        const TelemetryData& telemetry);

    void update(
        const TelemetryData& telemetry,
        SystemState& state);

    bool hasActiveCommand() const;

    void setCommandSender(MavlinkCommandSender* sender) {
        sender_ = sender;
    }

private:
    struct TrackedCommand {
        VehicleCommand logical_cmd;
        uint16_t mavlink_cmd_id;
        int retry_count = 0;
        int max_retries = 3;
        chrono::steady_clock::time_point last_sent_time;
    };

    uint16_t mapToMavlinkCommand(VehicleCommand cmd) const;

    void handleAck(
        const TelemetryData& telemetry,
        SystemState& state);

    void handleRetry(const TelemetryData& telemetry);

    optional<TrackedCommand> active_command_;
    MavlinkCommandSender* sender_ = nullptr;
    CommandBlockReason last_logged_block_ = CommandBlockReason::NONE;
};
