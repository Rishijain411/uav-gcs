#include <iostream>
#include <chrono>

#include "comm/UdpTransport.h"
#include "telemetry/TelemetryParser.h"
#include "telemetry/TelemetryData.h"
#include "core/StateManager.h"
#include "command/CommandManager.h"
#include "command/MavlinkCommandSender.h"
#include "comm/GcsHeartbeat.h"

constexpr int HEARTBEAT_TIMEOUT_MS = 2000;

int main() {

    UdpTransport udp;
    TelemetryData telemetry;
    StateManager stateManager;
    CommandManager commandManager;
    TelemetryParser parser(telemetry, stateManager);

    bool arm_intent_sent = false;

    if (!udp.start(14550)) {
        std::cerr << "Failed to start UDP transport" << std::endl;
        return -1;
    }

    // ---------- GCS Heartbeat ----------
    GcsHeartbeat gcsHeartbeat(udp.getSocketFd());
    auto last_hb = std::chrono::steady_clock::now();
    auto last_failsafe_check = std::chrono::steady_clock::now();

    std::cout << "[GCS] Heartbeat sender initialized" << std::endl;

    // ---------- Command sender ----------
    MavlinkCommandSender* cmdSender = nullptr;
    bool sender_initialized = false;

    uint8_t buffer[2048];

    while (true) {

        auto now = std::chrono::steady_clock::now();

        // ---------- Send GCS heartbeat ----------
        if (std::chrono::duration_cast<std::chrono::seconds>(
                now - last_hb).count() >= 1) {

            gcsHeartbeat.send();
            last_hb = now;
        }

        // ---------- Receive UDP ----------
        int len = udp.receive(buffer, sizeof(buffer));
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                parser.parse(buffer[i]);
            }
        }

        // ---------- Command lifecycle ----------
        commandManager.update(telemetry, stateManager.getMutableState());
        if (commandManager.commandFinished()) {
            commandManager.clearCommandFinished();
        }

        // ---------- Init command sender ----------
        if (!sender_initialized && telemetry.heartbeat_received) {
            cmdSender = new MavlinkCommandSender(
                udp.getSocketFd(),
                telemetry.system_id,
                telemetry.component_id
            );
            commandManager.setCommandSender(cmdSender);
            sender_initialized = true;
        }

        // ---------- FAILSAFE CHECK (PRODUCTION-GRADE) ----------
        if (telemetry.heartbeat_received &&
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_failsafe_check).count() >= 200) {

            last_failsafe_check = now;

            auto hb_elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - telemetry.last_heartbeat_time).count();

            auto link_elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - telemetry.last_mavlink_rx_time).count();

            if (hb_elapsed > HEARTBEAT_TIMEOUT_MS &&
                link_elapsed > HEARTBEAT_TIMEOUT_MS &&
                stateManager.getState() != SystemState::FAILSAFE) {

                stateManager.setState(SystemState::FAILSAFE);
                std::cout << "[FAILSAFE] MAVLink link timeout" << std::endl;
            }
        }

        // ---------- FAILSAFE RECOVERY ----------
        if (stateManager.isInFailsafe()) {

            auto link_elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - telemetry.last_mavlink_rx_time).count();

            if (link_elapsed <= HEARTBEAT_TIMEOUT_MS) {
                stateManager.setState(SystemState::CONNECTED);
                std::cout << "[RECOVERY] Link restored" << std::endl;
            }
        }

        // ---------- COMMAND ORCHESTRATION ----------
        if (!cmdSender || !telemetry.heartbeat_received)
            continue;

        // ----- ARM (single-shot intent, telemetry-gated) -----
        if (!arm_intent_sent &&
            telemetry.isTelemetryReady() &&
            stateManager.getState() == SystemState::CONNECTED &&
            !commandManager.hasActiveCommand()) {

            bool accepted = commandManager.requestCommand(
                VehicleCommand::ARM,
                stateManager.getState(),
                telemetry
            );

            arm_intent_sent = true;  // latch intent ONCE telemetry is ready

            if (accepted) {
                std::cout << "[CMD] ARM requested" << std::endl;
            }
        }
        //takeoff cmd
        static bool takeoff_intent_sent = false;

        if (stateManager.getState() == SystemState::ARMED &&
            telemetry.isTelemetryReady() &&
            !commandManager.hasActiveCommand() &&
            !takeoff_intent_sent) {

            bool accepted = commandManager.requestCommand(
                VehicleCommand::TAKEOFF,
                stateManager.getState(),
                telemetry
            );

            if (accepted) {
                takeoff_intent_sent = true;
                std::cout << "[CMD] TAKEOFF requested" << std::endl;
            }
        }



    }

    return 0;
}
