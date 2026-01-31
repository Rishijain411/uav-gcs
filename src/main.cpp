#include <iostream>
#include <chrono>
#include <string>
#include <fstream>
#include <vector>

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

// Phase B: Mission Planning
#include "mission/MissionProfile.h"
#include "mission/MissionProfileParser.h"
#include "mission/PreFlightBit.h"

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
        OperatorAuthorization::consumeDecision();  // NEW: Mark decision as consumed
        
        cout << "[AUTH] Operator denied command\n";
        return false;
    }

    OperatorAuthorization::consumeDecision();  // NEW: Mark decision as consumed
    
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
int main(int argc, char* argv[]) {

    // Phase B: Parse command-line arguments for mission file
    string mission_file;
    
    if (argc > 1) {
        mission_file = argv[1];
    } else {
        // Try to find config file in multiple locations
        vector<string> possible_paths = {
            "config/sample_mission.json",           // Current directory
            "../config/sample_mission.json",        // Parent directory (if running from build/)
            "../../config/sample_mission.json"      // Two levels up
        };
        
        for (const auto& path : possible_paths) {
            ifstream test(path);
            if (test.is_open()) {
                mission_file = path;
                test.close();
                break;
            }
        }
        
        if (mission_file.empty()) {
            cerr << "[ERROR] Cannot find config/sample_mission.json\n";
            cerr << "[ERROR] Please provide mission file path as argument:\n";
            cerr << "  ./my_gcs /path/to/mission.json\n";
            return -1;
        }
    }
    
    cout << "[GCS] Starting GCS-Vyuha\n";
    cout << "[GCS] Mission file: " << mission_file << "\n";

    UdpTransport udp;
    TelemetryData telemetry;
    StateManager stateManager;
    CommandManager commandManager;
    TelemetryParser parser(telemetry, stateManager);

    mission::Mission mission;
    
    // Phase B: Load mission profile
    mission::MissionProfile profile;
    string error_msg;
    if (!mission::MissionProfileParser::parseFromJson(mission_file, profile, error_msg)) {
        cerr << "[ERROR] Failed to load mission profile: " << error_msg << "\n";
        cerr << "[ERROR] Cannot proceed without valid mission profile (PRD requirement)\n";
        return -1;
    }
    
    if (!mission.loadProfile(profile)) {
        cerr << "[ERROR] Invalid mission profile\n";
        return -1;
    }
    
    cout << "[GCS] Mission profile loaded successfully\n";
    if (profile.target_id.has_value()) {
        cout << "[GCS] Target ID: " << profile.target_id.value() << "\n";
    }
    cout << "[GCS] Search area: " << profile.search_area.vertices.size() << " vertices\n";
    cout << "[GCS] Waypoints: " << profile.waypoints.size() << "\n";

    // 🔹 NEW: Mission-level controller
    MissionController missionController;

    if (!udp.start(14550)) {
        cerr << "Failed to start UDP transport\n";
        return -1;
    }

    // Phase C: Wire CommandManager to MissionController for waypoint publishing
    missionController.setCommandManager(&commandManager);

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
        // Check every 200ms if we've lost the heartbeat
        if (chrono::duration_cast<chrono::milliseconds>(
                now - last_failsafe_check).count() >= 200) {

            last_failsafe_check = now;

            // Only check failsafe if we previously had heartbeat contact
            if (telemetry.heartbeat_received) {
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

                    cout << "[FAILSAFE] MAVLink timeout detected after " 
                         << hb_elapsed << "ms\n";
                }
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
            // Phase B: PRD Rule - No mission can arm without validated MissionProfile
            if (!mission.hasValidProfile()) {
                static bool profile_warning_shown = false;
                if (!profile_warning_shown) {
                    cerr << "[PREFLIGHT] BLOCKED: No valid mission profile loaded (PRD requirement)\n";
                    profile_warning_shown = true;
                }
                break;
            }
            
            // Phase B: Run Pre-Flight BIT checks
            if (mission.isStateNewlyEntered()) {
                cout << "[PREFLIGHT] Running Built-In Test (BIT)...\n";
                auto bit_status = mission::PreFlightBit::runChecks(telemetry);
                mission.setBitStatus(bit_status);
                
                if (!bit_status.allPassed()) {
                    cout << "[PREFLIGHT] BIT FAILED - Cannot proceed to ARMED\n";
                    mission.apply_event(mission::MissionEvent::PREFLIGHT_FAIL);
                    break;
                }
                cout << "[PREFLIGHT] All BIT checks passed\n";
                mission.markStateHandled();
            }
            
            // Check preflight readiness conditions
            if (telemetry.isTelemetryReady() &&
                telemetry.ekf_ok &&
                telemetry.battery_ok &&
                telemetry.isLanded() &&
                telemetry.arm_state == ArmState::DISARMED &&
                !telemetry.in_failsafe &&
                mission.getBitStatus().allPassed()) {
                
                // Keep requesting until authorization granted
                bool approved = MissionTransitionAuthority::requestTransition(
                    mission,
                    mission::MissionEvent::PREFLIGHT_OK);
                
                if (approved) {
                    cout << "[PREFLIGHT] Approved, advancing to ARMED\n";
                }
            }
            break;

        case mission::MissionState::ARMED:
            if (mission.isStateNewlyEntered()) {
                // Keep trying to dispatch ARM command until it succeeds
                bool arm_success = dispatchCommand(
                    commandManager,
                    VehicleCommand::ARM,
                    stateManager.getState(),
                    mission.state(),
                    telemetry);
                
                if (arm_success) {
                    mission.markStateHandled();
                    cout << "[ARMED] ARM command sent successfully\n";
                }
            } else {
                // Debug: Check conditions MORE FREQUENTLY
                static int debug_counter = 0;
                if (++debug_counter % 10 == 0) {  // Print every ~1 second at 10Hz
                    cout << "[ARMED_DEBUG] hasActiveCommand=" << commandManager.hasActiveCommand() 
                         << " arm_state=" << (int)telemetry.arm_state 
                         << " (0=DISARMED, 1=ARMED)\n";
                }
                
                // After ARM command completes and vehicle is armed, 
                // request operator approval to transition to TRANSIT
                if (!commandManager.hasActiveCommand() &&
                    telemetry.arm_state == ArmState::ARMED) {
                    // Vehicle is armed, request operator approval to advance
                    cout << "[ARMED] ✓ Vehicle confirmed ARMED! Requesting transition to TRANSIT\n";
                    MissionTransitionAuthority::requestTransition(
                        mission,
                        mission::MissionEvent::TRANSIT_REACHED);
                }
            }
            break;

        case mission::MissionState::TRANSIT:
            if (mission.isStateNewlyEntered()) {
                if (dispatchCommand(
                    commandManager,
                    VehicleCommand::SET_MODE_AUTO,
                    stateManager.getState(),
                    mission.state(),
                    telemetry)) {
                    mission.markStateHandled();  // Only mark handled when command succeeds
                }
            } else {
                // Phase C: After SET_MODE_AUTO command completes, transition to SEARCH
                // Command completed means vehicle is in AUTO mode and ready for search
                if (!commandManager.hasActiveCommand()) {
                    // SET_MODE_AUTO command completed, transition to SEARCH
                    static bool transit_transition_requested = false;
                    if (!transit_transition_requested) {
                        transit_transition_requested = true;
                        cout << "[TRANSIT] AUTO mode set, transitioning to SEARCH\n";
                        MissionTransitionAuthority::requestTransition(
                            mission,
                            mission::MissionEvent::TRANSIT_REACHED);
                    }
                }
            }
            break;

        case mission::MissionState::SEARCH:
            // SearchPattern handled inside MissionController
            break;

        case mission::MissionState::ENGAGE:
            // EngagementPolicy handled inside MissionController
            break;

        case mission::MissionState::RTB:
            if (mission.isStateNewlyEntered()) {
                if (dispatchCommand(
                    commandManager,
                    VehicleCommand::LAND,
                    stateManager.getState(),
                    mission.state(),
                    telemetry)) {
                    mission.markStateHandled();
                }
            }
            break;

        default:
            break;
        }
    }

    return 0;
}
