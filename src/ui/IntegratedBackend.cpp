#include "ui/IntegratedBackend.h"
#include "ui/GCSBackendInterface.h"
#include "telemetry/TelemetryData.h"
#include "mission/MissionState.h"
#include "comm/UdpTransport.h"
#include "comm/LinkManager.h"
#include "telemetry/TelemetryParser.h"
#include "core/StateManager.h"
#include "command/CommandManager.h"
#include "command/MavlinkCommandSender.h"
#include "comm/GcsHeartbeat.h"
#include "security/SecureChannel.h"
#include "mission/Mission.h"
#include "mission/MissionController.h"
#include "mission/MissionProfileParser.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <QString>

IntegratedBackend::IntegratedBackend(GCSBackendInterface* ui_interface)
    : ui_interface_(ui_interface), running_(false), mission_load_requested_(false),
      arm_requested_(false), disarm_requested_(false), takeoff_requested_(false),
      abort_requested_(false), engage_requested_(false) {}

IntegratedBackend::~IntegratedBackend() {
    stop();
}

void IntegratedBackend::start() {
    running_ = true;
    backend_thread_ = std::make_unique<std::thread>([this]() {
        runBackendLoop();
    });
}

void IntegratedBackend::stop() {
    running_ = false;
    if (backend_thread_ && backend_thread_->joinable()) {
        backend_thread_->join();
    }
}

void IntegratedBackend::loadMission(const std::string& mission_file) {
    mission_file_ = mission_file;
    mission_load_requested_ = true;
}

void IntegratedBackend::sendArmCommand() {
    arm_requested_ = true;
}

void IntegratedBackend::sendDisarmCommand() {
    disarm_requested_ = true;
}

void IntegratedBackend::sendTakeoffCommand() {
    takeoff_requested_ = true;
}

void IntegratedBackend::sendAbortCommand() {
    abort_requested_ = true;
}

void IntegratedBackend::sendEngageCommand() {
    engage_requested_ = true;
}

void IntegratedBackend::sendRtlCommand() {
    rtl_requested_ = true;
}

void IntegratedBackend::sendLandCommand() {
    land_requested_ = true;
}

void IntegratedBackend::runBackendLoop() {
    TelemetryData telemetry;
    StateManager stateManager;
    CommandManager commandManager;
    TelemetryParser parser(telemetry, stateManager);
    mission::Mission mission;
    
    // No security in UI mode
    UdpTransport rf_udp(nullptr, LinkType::RF, false);
    UdpTransport lte_udp(nullptr, LinkType::LTE, false);
    
    rf_udp.start(14550);
    lte_udp.start(15550);
    
    LinkManager linkManager(rf_udp, lte_udp);
    GcsHeartbeat gcsHeartbeat(linkManager);
    MissionController missionController;
    missionController.setCommandManager(&commandManager);
    
    std::cout << "[BACKEND] Started, listening on UDP ports 14550 and 15550\n";
    
    MavlinkCommandSender* cmdSender = nullptr;
    bool sender_initialized = false;
    bool mission_loaded = false;
    mission::MissionProfile profile;
    
    // Mission upload state
    bool mission_upload_started = false;
    bool mission_upload_complete = false;
    bool mission_upload_failed = false;
    int mission_waypoints_sent = 0;
    int mission_upload_retries = 0;
    uint16_t last_request_seq_processed = 0xFFFF;  // Track which seq we last processed
    auto mission_upload_start = std::chrono::steady_clock::now();
    auto next_mission_retry_at = std::chrono::steady_clock::now();
    const int MISSION_UPLOAD_MAX_RETRIES = 10;
    const int MISSION_UPLOAD_BASE_BACKOFF_MS = 500;
    const int MISSION_UPLOAD_TIMEOUT_MS = 20000;
    
    auto last_hb = std::chrono::steady_clock::now();
    
    while (running_) {
        auto now = std::chrono::steady_clock::now();
        
        // Send heartbeat
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hb).count() >= 1000) {
            gcsHeartbeat.send();
            last_hb = now;
        }
        
        // Process MAVLink messages
        uint8_t buf[2048];
        ssize_t n = linkManager.receive(buf, sizeof(buf));
        if (n > 0) {
            for (ssize_t i = 0; i < n; i++) {
                parser.parse(buf[i]);
            }
        }
        
        // Initialize command sender when heartbeat received
        if (!sender_initialized && telemetry.heartbeat_received) {
            cmdSender = new MavlinkCommandSender(linkManager, telemetry.system_id);
            commandManager.setCommandSender(cmdSender);
            sender_initialized = true;
            
            std::cout << "[BACKEND] Connected to vehicle (SysID: " << static_cast<int>(telemetry.system_id) << ")\n";
            
            // Clear any existing mission on the vehicle (clean slate)
            std::cout << "[BACKEND] Clearing any existing mission on vehicle...\n";
            cmdSender->sendMissionClearAll();
            
            // If mission was already loaded, start upload now (only if not already complete)
            if (mission_loaded && !mission_upload_started && !mission_upload_complete) {
                std::cout << "[BACKEND] Starting mission upload after connection...\n";
                mission_upload_started = true;
                mission_upload_complete = false;
                mission_upload_failed = false;
                mission_waypoints_sent = 0;
                mission_upload_retries = 0;
                mission_upload_start = now;
                next_mission_retry_at = now + std::chrono::milliseconds(2000); // Give PX4 time to respond
                telemetry.mission_request_received = false; // Clear before starting
                
                cmdSender->sendMissionCount(static_cast<uint16_t>(profile.waypoints.size()));
                emit ui_interface_->missionUploadProgress(0, profile.waypoints.size());
            }
            
            // Notify UI of connection
            emit ui_interface_->connectionStatusChanged("CONNECTED");
        }
        
        // Load mission when requested
        if (mission_load_requested_ && !mission_loaded && !mission_file_.empty()) {
            std::string error;
            std::cout << "[BACKEND] Loading mission from: " << mission_file_ << "\n";
            
            if (mission::MissionProfileParser::parseFromJson(mission_file_, profile, error)) {
                std::cout << "[BACKEND] Mission parsed: " << profile.waypoints.size() << " waypoints\n";
                
                if (mission.loadProfile(profile)) {
                    mission_loaded = true;
                    mission_load_requested_ = false;
                    
                    std::cout << "[BACKEND] Mission loaded into memory\n";
                    
                    // Trigger state transition from INIT → PREFLIGHT
                    MissionTransitionAuthority::requestTransition(
                        mission,
                        mission::MissionEvent::LOAD_MISSION);
                    
                    std::cout << "[BACKEND] Mission state transitioned to PREFLIGHT\n";
                    
                    // Start mission upload if connected
                    if (sender_initialized && cmdSender) {
                        mission_upload_started = true;
                        mission_upload_complete = false;
                        mission_upload_failed = false;
                        mission_waypoints_sent = 0;
                        mission_upload_retries = 0;
                        mission_upload_start = now;
                        next_mission_retry_at = now + std::chrono::milliseconds(2000); // Give PX4 time to respond
                        
                        // Set telemetry flags for UI
                        telemetry.mission_upload_in_progress = true;
                        telemetry.mission_upload_complete = false;
                        telemetry.mission_upload_failed = false;
                        telemetry.mission_request_received = false; // Clear before starting
                        
                        std::cout << "[BACKEND] Starting mission upload...\n";
                        
                        cmdSender->sendMissionClearAll();
                        cmdSender->sendMissionCount(static_cast<uint16_t>(profile.waypoints.size()));
                        
                        emit ui_interface_->missionUploadProgress(0, profile.waypoints.size());
                    } else {
                        std::cout << "[BACKEND] Cannot upload: not connected to vehicle\n";
                    }
                } else {
                    std::cout << "[BACKEND] Failed to load mission profile into Mission object\n";
                }
            } else {
                std::cout << "[BACKEND] Failed to parse mission JSON: " << error << "\n";
            }
        }
        
        // Handle mission upload state machine
        if (mission_upload_started && !mission_upload_complete && !mission_upload_failed) {
            // Retry MISSION_COUNT if no response
            if (now >= next_mission_retry_at && mission_upload_retries < MISSION_UPLOAD_MAX_RETRIES) {
                if (!telemetry.mission_request_received) {
                    cmdSender->sendMissionCount(static_cast<uint16_t>(profile.waypoints.size()));
                    mission_upload_retries++;
                    const int backoff_ms = MISSION_UPLOAD_BASE_BACKOFF_MS * mission_upload_retries;
                    next_mission_retry_at = now + std::chrono::milliseconds(backoff_ms);
                }
            }
            
            // Timeout check
            const auto total_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - mission_upload_start).count();
            if (total_elapsed >= MISSION_UPLOAD_TIMEOUT_MS) {
                mission_upload_failed = true;
                telemetry.mission_upload_failed = true;
                telemetry.mission_upload_in_progress = false;
                emit ui_interface_->errorOccurred("Mission upload timeout");
            }
        }
        
        // Send mission items when requested
        if (mission_upload_started && !mission_upload_complete && telemetry.mission_request_received) {
            const uint16_t seq = telemetry.last_mission_request_seq;
            
            // Only process if this is a NEW request (different sequence number)
            if (seq != last_request_seq_processed) {
                last_request_seq_processed = seq;
                telemetry.mission_request_received = false;
                
                // Reset retry timer since PX4 is responding
                next_mission_retry_at = now + std::chrono::milliseconds(2000);
                
                if (seq < profile.waypoints.size()) {
                    std::cout << "[BACKEND] Sending waypoint " << seq << ": lat=" 
                              << profile.waypoints[seq].lat << " lon=" << profile.waypoints[seq].lon 
                              << " alt=" << profile.waypoints[seq].alt << "\n";
                    
                    cmdSender->sendMissionItemInt(seq, profile.waypoints[seq], seq == 0, true);
                    mission_waypoints_sent = seq + 1;
                    
                    // Update progress
                    emit ui_interface_->missionUploadProgress(mission_waypoints_sent, profile.waypoints.size());
                } else {
                    std::cout << "[BACKEND] Mission request out of range: " << seq << "\n";
                    // Out of range request, resend count
                    cmdSender->sendMissionCount(static_cast<uint16_t>(profile.waypoints.size()));
                    next_mission_retry_at = now + std::chrono::milliseconds(MISSION_UPLOAD_BASE_BACKOFF_MS);
                }
            }
        }
        
        // Handle mission ACK
        if (mission_upload_started && !mission_upload_complete && telemetry.last_mission_ack.valid) {
            const uint8_t result = telemetry.last_mission_ack.type;
            telemetry.last_mission_ack.valid = false;
            
            // Only accept if all waypoints sent
            if (mission_waypoints_sent >= static_cast<int>(profile.waypoints.size())) {
                if (result == MAV_MISSION_ACCEPTED) {
                    mission_upload_complete = true;
                    last_request_seq_processed = 0xFFFF;  // Reset for next upload
                    telemetry.mission_upload_complete = true;
                    telemetry.mission_upload_in_progress = false;
                    std::cout << "[BACKEND] Mission upload successful!\n";
                    emit ui_interface_->missionUploadSuccess();  // Enable preflight in UI
                } else {
                    mission_upload_failed = true;
                    last_request_seq_processed = 0xFFFF;  // Reset for retry
                    telemetry.mission_upload_failed = true;
                    telemetry.mission_upload_in_progress = false;
                    emit ui_interface_->errorOccurred("Mission upload rejected");
                }
            }
        }
        
        // Handle ARM/DISARM/ABORT commands
        if (arm_requested_ && sender_initialized && cmdSender) {
            std::cout << "[BACKEND] Sending ARM command\n";
            cmdSender->sendArm();
            arm_requested_ = false;
        }
        
        if (disarm_requested_ && sender_initialized && cmdSender) {
            std::cout << "[BACKEND] Sending DISARM command\n";
            cmdSender->sendDisarm();
            disarm_requested_ = false;
        }
        
        if (abort_requested_ && sender_initialized && cmdSender) {
            std::cout << "[BACKEND] Sending ABORT (Land) command\n";
            cmdSender->sendLand();
            abort_requested_ = false;
        }
        
        if (takeoff_requested_ && sender_initialized && cmdSender) {
            std::cout << "[BACKEND] Sending TAKEOFF command\n";
            cmdSender->sendTakeoff(20.0);  // Takeoff to 20m altitude
            takeoff_requested_ = false;
        }
        
        if (engage_requested_ && sender_initialized && cmdSender) {
            std::cout << "[BACKEND] ENGAGE command received (engagement authorized)\n";
            // Set mode to AUTO to start mission execution
            std::cout << "[BACKEND] Setting flight mode to AUTO for mission execution\n";
            cmdSender->sendSetModeAuto();
            engage_requested_ = false;
        }
        
        if (rtl_requested_ && sender_initialized && cmdSender) {
            std::cout << "[BACKEND] Sending RTL (Return to Launch) command\n";
            cmdSender->sendSetModeRTL();
            rtl_requested_ = false;
        }
        
        if (land_requested_ && sender_initialized && cmdSender) {
            std::cout << "[BACKEND] Sending LAND command\n";
            cmdSender->sendLand();
            land_requested_ = false;
        }
        
        // Update mission controller
        commandManager.update(telemetry, stateManager.getMutableState());
        missionController.update(mission, telemetry);
        
        // Emit telemetry updates to UI
        if (telemetry.heartbeat_received) {
            emit ui_interface_->positionUpdated(
                telemetry.latitude_deg, 
                telemetry.longitude_deg, 
                telemetry.relative_alt_m
            );
            
            emit ui_interface_->armStateChanged(
                telemetry.arm_state == ArmState::ARMED
            );
            
            // Emit flight mode
            QString mode = "UNKNOWN";
            if (telemetry.flight_phase == FlightPhase::IN_AIR) {
                mode = "IN_AIR";
            } else if (telemetry.arm_state == ArmState::ARMED) {
                mode = "ARMED";
            } else {
                mode = "DISARMED";
            }
            emit ui_interface_->modeChanged(mode);
            
            // Emit mission state from MissionController
            QString missionState = QString::fromStdString(mission::to_string(mission.state()));
            emit ui_interface_->missionStateChanged(missionState);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (cmdSender) delete cmdSender;
}
