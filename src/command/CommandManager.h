#pragma once

#include <optional>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

#include "VehicleCommand.h"
#include "core/SystemState.h"
#include "telemetry/TelemetryData.h"
#include "mission/MissionState.h"
#include "mission/search/SearchPattern.h"

class MavlinkCommandSender;
struct Vector3D;

class CommandManager {
public:
    bool isCommandAllowed(
        VehicleCommand cmd,
        SystemState system_state,
        mission::MissionState mission_state,
        const TelemetryData& telemetry,
        CommandBlockReason& out_reason) const;

    bool requestCommand(
        VehicleCommand cmd,
        SystemState system_state,
        mission::MissionState mission_state,
        const TelemetryData& telemetry);

    void update(
        const TelemetryData& telemetry,
        SystemState& system_state);

    bool hasActiveCommand() const;

    bool hasCommandTimedOut() const { return command_timed_out_; }
    void clearCommandTimeout() { command_timed_out_ = false; }

    // ARM ACK tracking for state machine
    bool hasArmAckBeenReceived() const { return arm_ack_received_; }
    void clearArmAckFlag() { arm_ack_received_ = false; }

    // Phase C: Search waypoint publishing
    void sendSearchWaypoint(const GeoPoint& waypoint);

    void sendAccelerationCommand(const Vector3D& accel);

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
        SystemState& system_state);

    void handleRetry();

    optional<TrackedCommand> active_command_;
    MavlinkCommandSender* sender_ = nullptr;
    bool command_timed_out_ = false;
    bool arm_ack_received_ = false;  // Track ARM ACK for state machine
};
