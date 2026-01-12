#include <iostream>
#include <chrono>

using namespace std;

#include "comm/UdpTransport.h"
#include "telemetry/TelemetryParser.h"
#include "telemetry/TelemetryData.h"
#include "core/StateManager.h"
#include "command/CommandManager.h"
#include "command/MavlinkCommandSender.h"
#include "comm/GcsHeartbeat.h"

#include "mission/Mission.h"
#include "mission/MissionEvent.h"

// Authority layers
#include "authority/OperatorAuthorization.h"
#include "authority/AuditLogger.h"
#include "authority/MissionTransitionAuthority.h"

// 🔹 NEW: Mission controller (Search + Engagement binding)
#include "mission/MissionController.h"

constexpr int HEARTBEAT_TIMEOUT_MS = 2000;

// ------------------------------------------------------------
// SAFE DISPATCH WRAPPER (OITL + AUDIT + COMMAND EXECUTION)
// ------------------------------------------------------------
bool dispatchCommand(
    CommandManager& cmd_mgr,
    VehicleCommand cmd,
    SystemState system_state,
    mission::MissionState mission_state,
    const TelemetryData& telemetry)
{
    // -------- Operator Authorization --------
    if (!OperatorAuthorization::hasPending()) {
            OperatorAuthorization::request(cmd, mission_state);
            return false;
        }

    auto decision = OperatorAuthorization::pollDecision();
    if (!decision.has_value())
        return false;

    if (!decision.value()) {
        cout << "[AUTH] Operator denied command\n";
        return false;
    }


    // -------- Command Execution --------
    bool ok = cmd_mgr.requestCommand(
        cmd,
        system_state,
        mission_state,
        telemetry);

    if (ok) {
        AuditLogger::logDecision(
            cmd,
            mission_state,
            "EXECUTED");
    } else {
        AuditLogger::logDecision(
            cmd,
            mission_state,
            "BLOCKED",
            "CommandManager rejected");
    }

    return ok;
}

// ============================================================
//                           MAIN
// ============================================================
int main() {

    UdpTransport udp;
    TelemetryData telemetry;
    StateManager stateManager;
    CommandManager commandManager;
    TelemetryParser parser(telemetry, stateManager);

    mission::Mission mission;

    // 🔹 NEW: Mission-level controller
    MissionController missionController;

    if (!udp.start(14550)) {
        cerr << "Failed to start UDP transport\n";
        return -1;
    }

    // ---------------- GCS Heartbeat ----------------
    GcsHeartbeat gcsHeartbeat(udp.getSocketFd());
    auto last_hb = chrono::steady_clock::now();
    auto last_failsafe_check = chrono::steady_clock::now();

    cout << "[GCS] Heartbeat sender initialized\n";

    // ---------------- Command sender ----------------
    MavlinkCommandSender* cmdSender = nullptr;
    bool sender_initialized = false;

    uint8_t buffer[2048];

    // ================= MAIN LOOP =================
    while (true) {

        auto now = chrono::steady_clock::now();

        // ---------- Send GCS heartbeat ----------
        if (chrono::duration_cast<chrono::seconds>(now - last_hb).count() >= 1) {
            gcsHeartbeat.send();
            last_hb = now;
        }

        // ---------- Receive MAVLink ----------
        int len = udp.receive(buffer, sizeof(buffer));
        if (len > 0) {
            for (int i = 0; i < len; i++)
                parser.parse(buffer[i]);
        }

        // ---------- Command lifecycle ----------
        commandManager.update(telemetry, stateManager.getMutableState());

        // ---------- Init command sender ----------
        if (!sender_initialized && telemetry.heartbeat_received) {
            cmdSender = new MavlinkCommandSender(
                udp.getSocketFd(),
                telemetry.system_id
            );
            commandManager.setCommandSender(cmdSender);
            sender_initialized = true;
            cout << "[GCS] Command sender initialized\n";
        }

        // ---------- FAILSAFE CHECK ----------
        if (telemetry.heartbeat_received &&
            chrono::duration_cast<chrono::milliseconds>(
                now - last_failsafe_check).count() >= 200) {

            last_failsafe_check = now;

            auto hb_elapsed =
                chrono::duration_cast<chrono::milliseconds>(
                    now - telemetry.last_heartbeat_time).count();

            auto link_elapsed =
                chrono::duration_cast<chrono::milliseconds>(
                    now - telemetry.last_mavlink_rx_time).count();

            if (hb_elapsed > HEARTBEAT_TIMEOUT_MS &&
                link_elapsed > HEARTBEAT_TIMEOUT_MS &&
                stateManager.getState() != SystemState::FAILSAFE) {

                stateManager.setState(SystemState::FAILSAFE);

                AuditLogger::logDecision(
                    VehicleCommand::NONE,
                    mission.state(),
                    "FAILSAFE",
                    "MAVLink timeout");

                cout << "[FAILSAFE] MAVLink timeout\n";
            }
        }

        // ====================================================
        //              MISSION AUTHORITY LOOP
        // ====================================================
        if (!cmdSender ||
            !telemetry.isTelemetryReady() ||
            commandManager.hasActiveCommand())
            continue;

        // 🔹 NEW: Mission-level autonomy binding
        missionController.update(mission, telemetry);

        switch (mission.state()) {

        case mission::MissionState::INIT:
            MissionTransitionAuthority::requestTransition(
                mission,
                mission::MissionEvent::LOAD_MISSION);
            break;

        case mission::MissionState::PREFLIGHT:
            if (telemetry.isPreflightReady()) {
            MissionTransitionAuthority::requestTransition(
                mission,
                mission::MissionEvent::PREFLIGHT_OK);
        }

            break;

        case mission::MissionState::ARMED:
            dispatchCommand(
                commandManager,
                VehicleCommand::ARM,
                stateManager.getState(),
                mission.state(),
                telemetry);
            break;

        case mission::MissionState::TRANSIT:
            dispatchCommand(
                commandManager,
                VehicleCommand::SET_MODE_AUTO,
                stateManager.getState(),
                mission.state(),
                telemetry);
            break;

        case mission::MissionState::SEARCH:
            // SearchPattern handled inside MissionController
            break;

        case mission::MissionState::ENGAGE:
            // EngagementPolicy handled inside MissionController
            break;

        case mission::MissionState::RTB:
            dispatchCommand(
                commandManager,
                VehicleCommand::LAND,
                stateManager.getState(),
                mission.state(),
                telemetry);
            break;

        default:
            break;
        }
    }

    return 0;
}
