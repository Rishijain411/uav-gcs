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
#include "authority/MissionTransitionAuthority.h"

extern "C" {
#include "mavlink/common/mavlink.h"
}

#include <chrono>
#include <thread>
#include <iostream>
#include <QString>
#include <string>

IntegratedBackend::IntegratedBackend(GCSBackendInterface* ui_interface)
    : ui_interface_(ui_interface), running_(false), mission_load_requested_(false),
      arm_requested_(false), disarm_requested_(false), takeoff_requested_(false),
      abort_requested_(false), engage_requested_(false),
      rtl_requested_(false), land_requested_(false) 
{}

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
    {
        std::lock_guard<std::mutex> lock(mission_file_mutex_);
        mission_file_ = mission_file;
    }
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
void IntegratedBackend::updateFailsafeRules(int comms, int battery, int gps) {
    pending_comms_loss_.store(comms);
    pending_low_battery_.store(battery);
    pending_gps_jamming_.store(gps);
    failsafe_update_pending_.store(true);
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
    missionController.setUIInterface(ui_interface_);  // Set UI interface to disable terminal OITL
    
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
    
    // Mission load timeout tracking
    auto mission_load_start = std::chrono::steady_clock::now();
    bool mission_load_in_progress = false;
    const int MISSION_UPLOAD_MAX_RETRIES = 10;
    const int MISSION_UPLOAD_BASE_BACKOFF_MS = 500;
    const int MISSION_UPLOAD_TIMEOUT_MS = 20000;
    const int MISSION_LOAD_TIMEOUT_MS = 10000;  // 10 second timeout for loading
    
    auto last_hb = std::chrono::steady_clock::now();
    std::string last_status_text_emitted;
    auto last_status_text_time = std::chrono::steady_clock::time_point::min();
    
    while (running_) {
        // Check for failsafe rule overrides from the UI
        if (failsafe_update_pending_.load()) {
            mission::FailsafeRules rules;
            
            // UI mapping: 0:RTL, 1:LAND, 2:HOLD, 3:CONTINUE (based on MainWindow setup)
            auto mapUI = [](int index) {
                switch(index) {
                    case 1: return mission::FailsafeBehavior::LAND;
                    case 2: return mission::FailsafeBehavior::HOLD;
                    case 3: return mission::FailsafeBehavior::CONTINUE;
                    default: return mission::FailsafeBehavior::RTL;
                }
            };

            rules.comms_loss = mapUI(pending_comms_loss_.load());
            rules.low_battery = mapUI(pending_low_battery_.load());
            
            // GPS UI mapping: 0:RTL, 1:LOITER(CONTINUE), 2:HOLD, 3:LAND
            int gpsIdx = pending_gps_jamming_.load();
            if (gpsIdx == 1) rules.gps_jamming = mission::FailsafeBehavior::CONTINUE;
            else if (gpsIdx == 2) rules.gps_jamming = mission::FailsafeBehavior::HOLD;
            else if (gpsIdx == 3) rules.gps_jamming = mission::FailsafeBehavior::LAND;
            else rules.gps_jamming = mission::FailsafeBehavior::RTL;

            mission.updateFailsafeRules(rules);
            failsafe_update_pending_.store(false);
            std::cout << "[BACKEND] Applied UI failsafe overrides to mission profile\n";
        }
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
        if (mission_load_requested_ && !mission_loaded && !mission_load_in_progress) {
            mission_load_in_progress = true;
            mission_load_start = now;
            std::string mission_file_copy;
            {
                std::lock_guard<std::mutex> lock(mission_file_mutex_);
                mission_file_copy = mission_file_;
            }
            
            std::string error;
            std::cout << "[BACKEND] Loading mission from: " << mission_file_copy << "\n";
            
            if (mission::MissionProfileParser::parseFromJson(mission_file_copy, profile, error)) {
                std::cout << "[BACKEND] Mission parsed: " << profile.waypoints.size() << " waypoints\n";
                
                if (mission.loadProfile(profile)) {
                    mission_loaded = true;
                    mission_load_requested_ = false;
                    mission_load_in_progress = false;
                    
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
                    mission_load_in_progress = false;
                }
            } else {
                std::cout << "[BACKEND] Failed to parse mission JSON: " << error << "\n";
                mission_load_in_progress = false;
            }
        }
        
        // Mission load timeout check
        if (mission_load_in_progress) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - mission_load_start).count();
            if (elapsed > MISSION_LOAD_TIMEOUT_MS) {
                std::cout << "[BACKEND] ERROR: Mission load timeout after " << elapsed << "ms\n";
                mission_load_in_progress = false;
                mission_load_requested_ = false;
                emit ui_interface_->errorOccurred("Mission load timeout");
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
                    
                    // Start the mission by setting current waypoint to 0
                    if (sender_initialized && cmdSender) {
                        std::cout << "[BACKEND] Starting mission (setting current waypoint to 0)\n";
                        cmdSender->sendMissionSetCurrent(0);
                    }
                    
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
            if (mission.state() == mission::MissionState::PREFLIGHT) {
                std::cout << "[BACKEND] UI ARM button: Applying PREFLIGHT_OK (operator authorized via UI)\n";
                // Direct application - operator already authorized by clicking UI button
                mission.apply_event(mission::MissionEvent::PREFLIGHT_OK);
            } else {
                std::cout << "[BACKEND] ARM requested but not in PREFLIGHT state (current: "
                          << static_cast<int>(mission.state()) << ")\n";
            }
            arm_requested_ = false;
        }
        
        // REAL-TIME COMMAND ACKNOWLEDGMENT BRIDGE
        // This handles feedback for ARM, TAKEOFF, and other mission commands
        if (telemetry.last_command_ack.valid) {
            uint16_t cmd_id = telemetry.last_command_ack.command_id;
            uint8_t result = telemetry.last_command_ack.result;

            if (result == MAV_RESULT_ACCEPTED) {
                if (cmd_id == MAV_CMD_COMPONENT_ARM_DISARM) {
                    emit ui_interface_->armStateChanged(true);
                    std::cout << "[BACKEND] ARM Accepted by vehicle\n";
                } 
                else if (cmd_id == MAV_CMD_NAV_TAKEOFF) {
                    emit ui_interface_->statusUpdated("TAKEOFF Accepted - Vehicle is airborne");
                    std::cout << "[BACKEND] TAKEOFF Accepted\n";
                }
                // Add other mission-critical command IDs here as needed
            } 
            else if (result == MAV_RESULT_DENIED || result == MAV_RESULT_TEMPORARILY_REJECTED) {
                // Bridge rejections back to the UI audit log and error popups
                QString errorMsg = QString("Command %1 Rejected: Check Pre-Flight/GPS").arg(cmd_id);
                emit ui_interface_->errorOccurred(errorMsg);
                std::cout << "[BACKEND] Command " << cmd_id << " REJECTED with code: " << (int)result << "\n";
            }
            
            // Note: We do NOT set valid = false here. 
            // commandManager.update() below needs to see this ACK to clear its internal state.
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
            std::cout << "[BACKEND] Sending TAKEOFF (AUTO mode)\n";
            cmdSender->sendSetModeAuto();
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
        
        // --- RATE-LIMITED TELEMETRY STREAM TO UI (Fixes SIGSEGV) ---
        if (telemetry.heartbeat_received) {
            
            // 1. Mission State: Only emit on actual transition
            static mission::MissionState last_emitted_state = mission::MissionState::INIT;
            if (mission.state() != last_emitted_state) {
                QString missionState = QString::fromStdString(mission::to_string(mission.state()));
                emit ui_interface_->missionStateChanged(missionState);
                last_emitted_state = mission.state();
            }

            // 2. Flight Mode: Only emit on actual change
            static QString last_mode;
            QString currentMode = (telemetry.flight_phase == FlightPhase::IN_AIR) ? "IN_AIR" : 
                                 (telemetry.arm_state == ArmState::ARMED ? "ARMED" : "DISARMED");
            if (currentMode != last_mode) {
                emit ui_interface_->modeChanged(currentMode);
                last_mode = currentMode;
            }

            // 3. Telemetry Bridge: Throttle to 10Hz (Every 100ms) per PRD
            static auto last_telem_time = std::chrono::steady_clock::now();
            if (now - last_telem_time >= std::chrono::milliseconds(100)) {
                
                // Push coordinates and speed metrics
                emit ui_interface_->positionUpdated(
                    telemetry.latitude_deg, 
                    telemetry.longitude_deg, 
                    telemetry.relative_alt_m
                );

                if (ui_interface_) {
                    ui_interface_->onTelemetryUpdated(telemetry);
                }
                
                last_telem_time = now;
            }

            // 4. Status Text: Only emit when updated by PX4
            if (telemetry.status_text_updated) {
                telemetry.status_text_updated = false;
                emit ui_interface_->statusUpdated(QString::fromUtf8(telemetry.last_status_text));
            }
            
            // 5. BIT Health: Update UI checkboxes
            emit ui_interface_->healthStatusUpdated(
                telemetry.ekf_ok,
                telemetry.battery_ok,
                telemetry.heartbeat_received
            );
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (cmdSender) delete cmdSender;
}
