#include <iostream>
#include <chrono>
#include <string>
#include <fstream>
#include <vector>

using namespace std;

#include "comm/UdpTransport.h"
#include "comm/LinkManager.h"
#include "telemetry/TelemetryParser.h"
#include "telemetry/TelemetryData.h"
#include "core/StateManager.h"
#include "command/CommandManager.h"
#include "command/MavlinkCommandSender.h"
#include "comm/GcsHeartbeat.h"

// Secure Channel
#include "security/SecureChannel.h"
#include "security/SecurityManager.h"
#include "mission/Mission.h"
#include "mission/MissionEvent.h"

// Authority layers
#include "authority/OperatorAuthorization.h"
#include "authority/AuditLogger.h"
#include "authority/MissionTransitionAuthority.h"

// Mission controller
#include "mission/MissionController.h"

// Phase B: Mission Planning
#include "mission/MissionProfile.h"
#include "mission/MissionProfileParser.h"
#include "mission/PreFlightBit.h"
#include "utils/MissionUploadBackoff.h"

// Video streamming
#include "video/VideoLink.h"
#include "video/JetsonVideoLink.h"
#include "video/NullVideoLink.h"

constexpr int HEARTBEAT_TIMEOUT_MS = 2000;
constexpr int MISSION_UPLOAD_MAX_RETRIES = 5;
constexpr int MISSION_UPLOAD_BASE_BACKOFF_MS = 500;
constexpr int MISSION_UPLOAD_TIMEOUT_MS = 15000;

// ------------------------------------------------------------
// Mission checksum (for audit/consistency)
// ------------------------------------------------------------
static uint32_t missionChecksum(const mission::MissionProfile& profile) {
    uint32_t hash = 2166136261u;
    auto mix = [&hash](uint32_t v) {
        hash ^= v;
        hash *= 16777619u;
    };

    for (const auto& wp : profile.waypoints) {
        mix(static_cast<uint32_t>(wp.lat * 1e7));
        mix(static_cast<uint32_t>(wp.lon * 1e7));
        mix(static_cast<uint32_t>(wp.alt * 100));
    }
    mix(static_cast<uint32_t>(profile.search_area.vertices.size()));
    mix(static_cast<uint32_t>(profile.waypoints.size()));
    return hash;
}

static void writeMissionUploadStatus(const std::string& path,
                                     const std::string& status,
                                     int ack_type,
                                     int retries) {
    std::ofstream out(path);
    if (!out.is_open()) {
        return;
    }
    out << "{\n";
    out << "  \"status\": \"" << status << "\",\n";
    out << "  \"ack_type\": " << ack_type << ",\n";
    out << "  \"retries\": " << retries << "\n";
    out << "}\n";
}

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
    if (!OperatorAuthorization::hasPending()) {
        OperatorAuthorization::request(cmd, mission_state);
        return false;
    }

    auto decision = OperatorAuthorization::pollDecision();
    if (!decision.has_value())
        return false;

    if (!decision.value()) {
        OperatorAuthorization::consumeDecision();
        cout << "[AUTH] Operator denied command\n";
        return false;
    }

    OperatorAuthorization::consumeDecision();

    bool ok = cmd_mgr.requestCommand(
        cmd,
        system_state,
        mission_state,
        telemetry);

    AuditLogger::logDecision(
        cmd,
        mission_state,
        ok ? "EXECUTED" : "BLOCKED");

    return ok;
}

// ============================================================
//                           MAIN
// ============================================================
int main(int argc, char* argv[]) {

    string mission_file;

    if (argc > 1) {
        mission_file = argv[1];
    } else {
        vector<string> possible_paths = {
            "config/sample_mission.json",
            "../config/sample_mission.json",
            "../../config/sample_mission.json"
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
            cerr << "[ERROR] Mission file not found\n";
            return -1;
        }
    }

    // Backend initialized

    // 🔐 Security Mode Audit (PRD requirement)

    if (!SecurityManager::enabled()) {
        AuditLogger::logDecision(
            VehicleCommand::NONE,
            mission::MissionState::INIT,
            "SECURITY_MODE",
            "PLAINTEXT (Jetson absent, dev mode)"
        );
    } else {
        AuditLogger::logDecision(
            VehicleCommand::NONE,
            mission::MissionState::INIT,
            "SECURITY_MODE",
            "ENCRYPTED (Jetson mode)"
        );
    }

    TelemetryData telemetry;
    StateManager stateManager;
    CommandManager commandManager;
    TelemetryParser parser(telemetry, stateManager);
    mission::Mission mission;

    // Video Link (Jetson-ready, flag gated)
    std::unique_ptr<VideoLink> video;

    // ---------------- Mission Profile ----------------
    mission::MissionProfile profile;
    string error_msg;

    if (!mission::MissionProfileParser::parseFromJson(
            mission_file, profile, error_msg)) {
        cerr << "[ERROR] Mission profile load failed: " << error_msg << "\n";
        return -1;
    }

    if (!mission.loadProfile(profile)) {
        cerr << "[ERROR] Invalid mission profile\n";
        return -1;
    }

    // cout << "[GCS] Mission profile loaded\n";

    // Video link selection (security flag gated)
    if (SecurityManager::enabled()) {
        video = std::make_unique<JetsonVideoLink>();
        cout << "[VIDEO] Jetson video link selected\n";
    } else {
        // Plaintext / dev mode → no video
        video = std::make_unique<NullVideoLink>();
        cout << "[VIDEO] Video disabled (no Jetson)\n";
    }

    // Start video subsystem (safe no-op in NullVideoLink)
    video->start();


    //  Secure Channel (MISSION SCOPED)
    SecureChannel secureChannel(profile.crypto.mission_key);

    // 🔐 Transport Links (Security gated by flag)
    // RF link (direct PX4 path)
    UdpTransport rf_udp(
        SecurityManager::enabled() ? &secureChannel : nullptr,
        LinkType::RF,
        SecurityManager::enabled()
    );

    // LTE / Jetson link
    UdpTransport lte_udp(
        SecurityManager::enabled() ? &secureChannel : nullptr,
        LinkType::LTE,
        SecurityManager::enabled()
    );


    rf_udp.start(14550);     // RF port
    lte_udp.start(15550);    // LTE fallback port

    LinkManager linkManager(rf_udp, lte_udp);

    // Mission controller
    MissionController missionController;
    missionController.setCommandManager(&commandManager);

    // Heartbeat
    GcsHeartbeat gcsHeartbeat(linkManager);
    auto last_hb = chrono::steady_clock::now();
    auto last_failsafe_check = chrono::steady_clock::now();

    // Command sender
    MavlinkCommandSender* cmdSender = nullptr;
    bool sender_initialized = false;
    bool mission_upload_started = false;
    bool mission_upload_complete = false;
    bool mission_upload_failed = false;
    bool mission_clear_sent = false;
    int mission_upload_retries = 0;
    int mission_waypoints_sent = 0;
    auto mission_upload_start = chrono::steady_clock::now();
    auto next_mission_retry_at = chrono::steady_clock::now();

    uint8_t buffer[2048];

    // ================= MAIN LOOP =================
    while (true) {

        auto now = chrono::steady_clock::now();

        // ---------- GCS heartbeat ----------
        if (chrono::duration_cast<chrono::seconds>(now - last_hb).count() >= 1) {
            gcsHeartbeat.send();
            last_hb = now;
        }

        // ---------- Receive encrypted MAVLink ----------
        int len = linkManager.receive(buffer, sizeof(buffer));

        if (len > 0) {
            for (int i = 0; i < len; i++)
                parser.parse(buffer[i]);
        }

        // ---------- Command lifecycle ----------
        commandManager.update(telemetry, stateManager.getMutableState());

        // ---------- Init command sender ----------
        if (!sender_initialized && telemetry.heartbeat_received) {
            cmdSender = new MavlinkCommandSender(
            linkManager,telemetry.system_id);

            commandManager.setCommandSender(cmdSender);
            sender_initialized = true;
            // cout << "[GCS] Command sender initialized\n";
        }

        // ---------- MISSION UPLOAD HANDSHAKE ----------
        if (sender_initialized && !mission_upload_started) {
            if (!mission_clear_sent) {
                cmdSender->sendMissionClearAll();
                mission_clear_sent = true;
                // cout << "[GCS] Mission clear all sent\n";
            }
            cmdSender->sendMissionCount(static_cast<uint16_t>(profile.waypoints.size()));
            mission_upload_started = true;
            telemetry.mission_upload_in_progress = true;
            mission_upload_start = now;
            next_mission_retry_at = now + chrono::milliseconds(MISSION_UPLOAD_BASE_BACKOFF_MS);
            // cout << "[GCS] Mission upload started (MISSION_COUNT sent)\n";
            writeMissionUploadStatus("config/mission_upload_status.json", "IN_PROGRESS", -1, mission_upload_retries);
        }

        if (mission_upload_started && !mission_upload_complete && !mission_upload_failed) {
            if (now >= next_mission_retry_at && mission_upload_retries < MISSION_UPLOAD_MAX_RETRIES) {
                cmdSender->sendMissionCount(static_cast<uint16_t>(profile.waypoints.size()));
                mission_upload_retries++;
                const int backoff_ms = computeMissionUploadBackoffMs(mission_upload_retries, MISSION_UPLOAD_BASE_BACKOFF_MS);
                next_mission_retry_at = now + chrono::milliseconds(backoff_ms);
                // cout << "[GCS] Mission upload retry (MISSION_COUNT resent)\n";
                writeMissionUploadStatus("config/mission_upload_status.json", "IN_PROGRESS", -1, mission_upload_retries);
            }

            const auto total_elapsed = chrono::duration_cast<chrono::milliseconds>(now - mission_upload_start).count();
            if (total_elapsed >= MISSION_UPLOAD_TIMEOUT_MS) {
                mission_upload_failed = true;
                telemetry.mission_upload_failed = true;
                telemetry.mission_upload_in_progress = false;
                // cout << "[GCS] Mission upload timeout\n";
                writeMissionUploadStatus("config/mission_upload_status.json", "FAILED", -1, mission_upload_retries);
            }
        }

        if (mission_upload_started && !mission_upload_complete && telemetry.mission_request_received) {
            const uint16_t seq = telemetry.last_mission_request_seq;
            telemetry.mission_request_received = false;
            if (seq < profile.waypoints.size()) {
                cmdSender->sendMissionItemInt(seq, profile.waypoints[seq], seq == 0, true);
                // cout << "[GCS] Sent MISSION_ITEM_INT seq=" << seq << "\n";
                mission_waypoints_sent = seq + 1;
            } else {
                // cout << "[GCS] Mission request out of range: " << seq << "\n";
                cmdSender->sendMissionCount(static_cast<uint16_t>(profile.waypoints.size()));
                next_mission_retry_at = now + chrono::milliseconds(MISSION_UPLOAD_BASE_BACKOFF_MS);
            }
        }

        if (mission_upload_started && !mission_upload_complete && telemetry.last_mission_ack.valid) {
            const uint8_t result = telemetry.last_mission_ack.type;
            telemetry.last_mission_ack.valid = false;
            
            // Only accept mission complete if all waypoints have been sent
            if (mission_waypoints_sent >= static_cast<int>(profile.waypoints.size())) {
                if (result == MAV_MISSION_ACCEPTED) {
                    mission_upload_complete = true;
                    telemetry.mission_upload_complete = true;
                    telemetry.mission_upload_in_progress = false;
                    // cout << "[GCS] Mission upload accepted (" << mission_waypoints_sent << "/" << profile.waypoints.size() << " waypoints)\n";
                    writeMissionUploadStatus("config/mission_upload_status.json", "ACCEPTED", result, mission_upload_retries);
                } else {
                    mission_upload_failed = true;
                    telemetry.mission_upload_failed = true;
                    telemetry.mission_upload_in_progress = false;
                    // cout << "[GCS] Mission upload failed. ACK type=" << int(result) << "\n";
                    writeMissionUploadStatus("config/mission_upload_status.json", "FAILED", result, mission_upload_retries);
                }
            }
        }

        // ---------- FAILSAFE CHECK ----------
        if (chrono::duration_cast<chrono::milliseconds>(
                now - last_failsafe_check).count() >= 200) {

            last_failsafe_check = now;

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
                        "MAVLink timeout over secure link");

                    cout << "[FAILSAFE] Encrypted link lost\n";
                }
            }
        }

        // ====================================================
        //              MISSION AUTHORITY LOOP
        // ====================================================
        if (!cmdSender ||
            !telemetry.isTelemetryReady())
            continue;

        // Skip mission control if command is active
        if (commandManager.hasActiveCommand())
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

        case mission::MissionState::ARM_REQUESTED:
            if (mission.isStateNewlyEntered()) {
                if (dispatchCommand(
                    commandManager,
                    VehicleCommand::ARM,
                    stateManager.getState(),
                    mission.state(),
                    telemetry)) {
                    mission.markStateHandled();
                }
            } else {
                // Wait for ARM command to complete, then transition to ARMED
                if (!commandManager.hasActiveCommand() && telemetry.arm_state == ArmState::ARMED) {
                    MissionTransitionAuthority::requestTransition(
                        mission,
                        mission::MissionEvent::VEHICLE_ARMED);
                }
            }
            break;

        case mission::MissionState::ARMED:
            // Wait for operator to request mission start
            if (mission.isStateNewlyEntered()) {
                mission.markStateHandled();
            }
            
            // After ARM succeeds, automatically transition to TRANSIT
            // This will trigger SET_MODE_AUTO command in TRANSIT state
            static bool transit_requested = false;
            if (mission.isStateNewlyEntered()) {
                transit_requested = false;  // Reset flag when entering ARMED
            }
            
            if (!mission.isStateNewlyEntered() && 
                !commandManager.hasActiveCommand() &&
                telemetry.arm_state == ArmState::ARMED &&
                !transit_requested) {
                // ARM confirmed, transition to TRANSIT (will send SET_MODE_AUTO)
                transit_requested = true;
                cout << "[ARMED] Vehicle armed, transitioning to TRANSIT\n";
                MissionTransitionAuthority::requestTransition(
                    mission,
                    mission::MissionEvent::OPERATOR_AUTO_CONFIRM);
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
    if (video) {
    video->stop();
}


    return 0;
}
