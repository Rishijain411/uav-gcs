#include "MissionController.h"
#include "command/CommandManager.h"
#include "telemetry/TelemetryData.h"
#include "ui/GCSBackendInterface.h"

#include <chrono>
#include <string>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <array>
#include <optional>
#include <atomic>
#include <type_traits>
// Static helper to map configured failsafe behaviors to MAVLink commands
static VehicleCommand behaviorToCommand(mission::FailsafeBehavior b) {
    switch(b) {
        case mission::FailsafeBehavior::LAND:     return VehicleCommand::LAND;
        case mission::FailsafeBehavior::HOLD:     return VehicleCommand::SET_MODE_LOITER;
        case mission::FailsafeBehavior::CONTINUE: return VehicleCommand::SET_MODE_AUTO;
        case mission::FailsafeBehavior::RTL:      
        default:                                  return VehicleCommand::SET_MODE_RTL;
    }
}

MissionController::MissionController()
    : engagement_policy_([]{
        EngagementConfig cfg;
        cfg.max_reengagement_attempts = 3;
        return cfg;
    }())
{
    // Search pattern will be initialized dynamically
    // when entering SEARCH state from mission profile
}


void MissionController::update(
    mission::Mission& mission,
    const TelemetryData& telemetry)
{
    // ==================================================
    // 1. HEARTBEAT MONITOR (PRD: Comms Loss Fail-Safe)
    // ==================================================
    auto now = std::chrono::steady_clock::now();
    auto hb_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - telemetry.last_heartbeat_time).count();

    // If link is lost for > 10 seconds, trigger failsafe transition
    // Ignore during INIT, PREFLIGHT, and ABORTED states (connection not required yet)
    if (hb_elapsed_ms > 10000 && 
        mission.state() != mission::MissionState::INIT &&
        mission.state() != mission::MissionState::PREFLIGHT &&
        mission.state() != mission::MissionState::ABORTED) {
        if (ui_interface_) {
            ui_interface_->onCommsLoss();
        }
        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::SYSTEM_FAILURE,
            MissionAbortReason::COMMAND_TIMEOUT,
            "Comms Loss: Heartbeat Timeout");
        return;
    }

    // ==================================================
    // ARM_REQUESTED → send ARM command 
    // ==================================================
    if (mission.state() == mission::MissionState::ARM_REQUESTED &&
        last_state_ != mission::MissionState::ARM_REQUESTED)
    {
        if (cmd_manager_) {
            cmd_manager_->requestCommand(
                VehicleCommand::ARM,
                SystemState::ARMED,
                mission.state(),
                telemetry);
        }
    }

    // ==================================================
    // ARM CONFIRMATION (PX4 authoritative)
    // ==================================================
    // Wait for consistent health before confirming ARM
    // This prevents transitioning to AUTO while preflight checks are still settling
    
    if (mission.state() == mission::MissionState::ARM_REQUESTED) {
        bool arm_ack_received = false;
        
        // Check if ARM command ACK was received
        if (cmd_manager_ && cmd_manager_->hasArmAckBeenReceived()) {
            arm_ack_received = true;
            std::cout << "[ARM ACK] Flag set - can proceed to ARMED state\n";
        }

        // ========== SITL MODE: Skip health checks for testing ==========
        #ifdef SITL_MODE
            // In SITL, we bypass health checks to test the logic flow
            // without hardware dependencies (GPS, real battery, etc.)
            bool health_ok = true;
            static auto last_sitl_log = std::chrono::steady_clock::time_point::min();
            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_sitl_log).count() > 1000) {
                std::cout << "[TEST] SITL MODE ACTIVE: Health checks bypassed\n";
                last_sitl_log = now;
            }
        #else
            // Production: Enforce all health checks
            bool health_ok = (telemetry.ekf_ok &&
                            telemetry.battery_ok &&
                            !telemetry.in_failsafe);
        #endif
        
        // Arm state check (always required, both SITL and production)
        // CRITICAL: Require BOTH ARM command ACK AND telemetry confirmation
        if (arm_ack_received && telemetry.arm_state == ArmState::ARMED && health_ok) {
            arm_stable_frames_++;
        } else {
            arm_stable_frames_ = 0;
        }

        // Wait for ARM_STABLE_FRAMES_REQUIRED (~100ms) to ensure stable arming
        if (arm_stable_frames_ >= ARM_STABLE_FRAMES_REQUIRED) {
            if (cmd_manager_) {
                cmd_manager_->clearCommandTimeout();
                cmd_manager_->clearArmAckFlag();  // Clear flag for next ARM attempt
            }
            MissionTransitionAuthority::requestTransition(
                mission,
                mission::MissionEvent::VEHICLE_ARMED);
            arm_stable_frames_ = 0;
        }
    }

    // ==================================================
    // TESTING: External ARM detection (for `commander arm` in PX4)
    // Only trigger if mission is still in PREFLIGHT when ARM occurs
    // ==================================================
    static ArmState last_arm_state = ArmState::DISARMED;
    static bool external_arm_processed = false;

    // Reset flag when entering PREFLIGHT
    if (mission.state() == mission::MissionState::PREFLIGHT && 
        last_state_ != mission::MissionState::PREFLIGHT) {
        external_arm_processed = false;
    }

    // Only detect external ARM if:
    // - Still in PREFLIGHT (UI hasn't started ARM_REQUESTED flow)
    // - Vehicle just transitioned to ARMED
    // - Haven't already processed this external ARM
    if (mission.state() == mission::MissionState::PREFLIGHT &&
        telemetry.arm_state == ArmState::ARMED &&
        last_arm_state == ArmState::DISARMED &&
        !external_arm_processed)
    {
        std::cout << "[TEST MODE] External ARM detected (commander arm)\n";
        external_arm_processed = true;
        
        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::PREFLIGHT_OK);
    }
    
    last_arm_state = telemetry.arm_state;

    // ==================================================
    // ARMED → AUTO MODE 
    // ==================================================
    // When UI is active: TAKEOFF button handles SET_MODE_AUTO (no terminal prompt)
    // When headless: Request OITL authorization via terminal
    // Skip automatic request if UI is active - UI TAKEOFF button will handle it
    if (mission.state() == mission::MissionState::ARMED &&
        !auto_mode_sent_ &&
        !OperatorAuthorization::hasPending())
    {
        // Only request terminal OITL if no UI is active
        // UI mode: User clicks TAKEOFF button → backend sends SET_MODE_AUTO directly
        if (ui_interface_ == nullptr) {
            OperatorAuthorization::request(
                VehicleCommand::SET_MODE_AUTO,
                mission.state());
            return;
        }
        // UI mode: Don't request here, wait for UI TAKEOFF button
        // The automatic transition in main.cpp will handle it
    }

    // ==================================================
    // HANDLE AUTO AUTHORIZATION
    // ==================================================
    if (mission.state() == mission::MissionState::ARMED &&
        OperatorAuthorization::hasPending())
    {
        auto decision = OperatorAuthorization::pollDecision();
        if (!decision.has_value())
            return;

        OperatorAuthorization::consumeDecision();

        if (!decision.value()) {
            MissionTransitionAuthority::requestTransition(
                mission,
                mission::MissionEvent::OPERATOR_ABORT,
                MissionAbortReason::OPERATOR_DENIED,
                "Operator denied AUTO");
            return;
        }

        if (cmd_manager_) {
            cmd_manager_->requestCommand(
                VehicleCommand::SET_MODE_AUTO,
                SystemState::ARMED,
                mission.state(),
                telemetry);
        }

        auto_mode_sent_ = true;
        return;
    }

    // AUTO CONFIRMED → TRANSIT
    if (mission.state() == mission::MissionState::ARMED &&
        auto_mode_sent_ &&
        !cmd_manager_->hasActiveCommand())
    {
        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::OPERATOR_AUTO_CONFIRM);

        auto_mode_sent_ = false;
        return;
    }

    // ==================================================
    // TESTING: External TAKEOFF detection (for `commander takeoff` in PX4)
    // ==================================================
    static NavState last_nav_state = NavState::UNKNOWN;
    
    if (mission.state() == mission::MissionState::ARMED &&
        !auto_mode_sent_ &&
        (telemetry.nav_state == NavState::AUTO_MISSION || 
         telemetry.nav_state == NavState::AUTO_TAKEOFF) &&
        last_nav_state != NavState::AUTO_MISSION &&
        last_nav_state != NavState::AUTO_TAKEOFF)
    {
        std::cout << "[TEST MODE] External TAKEOFF detected - transitioning to TRANSIT\n";
        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::OPERATOR_AUTO_CONFIRM);
    }
    
    last_nav_state = telemetry.nav_state;

    // ==================================================
    // ABORT HANDLING
    // ==================================================
    if (mission.state() != mission::MissionState::ABORTED &&
        mission.state() != mission::MissionState::COMPLETE)
    {
        if (cmd_manager_ && cmd_manager_->hasCommandTimedOut()) {
            // Do not abort during preflight/arm request/armed in SITL; allow late ACKs
            if (mission.state() == mission::MissionState::PREFLIGHT ||
                mission.state() == mission::MissionState::ARM_REQUESTED ||
                mission.state() == mission::MissionState::ARMED) {
                cmd_manager_->clearCommandTimeout();
            } else {
                MissionTransitionAuthority::requestTransition(
                    mission,
                    mission::MissionEvent::SYSTEM_FAILURE,
                    MissionAbortReason::COMMAND_TIMEOUT,
                    "Command retry limit exceeded");
                return;
            }
        }

        if (telemetry.in_failsafe) {
            MissionTransitionAuthority::requestTransition(
                mission,
                mission::MissionEvent::SYSTEM_FAILURE,
                MissionAbortReason::FAILSAFE_TRIGGERED,
                telemetry.last_status_text);
            return;
        }
    }

    // ==================================================
    // STATE-SPECIFIC AUTONOMY
    // ==================================================
    switch (mission.state()) {

    case mission::MissionState::SEARCH:
        handleSearch(mission, telemetry);
        break;

    case mission::MissionState::ENGAGE:
        handleEngage(mission, telemetry);
        break;

    case mission::MissionState::ASSESS:
        handleAssess(mission, telemetry);
        break;

    case mission::MissionState::RTB:
        // Monitor return journey and finalize on landing
        if (mission.isStateNewlyEntered() && ui_interface_) {
            ui_interface_->onRTBInitiated("Battle Damage Assessment Complete");
        }
        if (telemetry.isLanded()) {
            if (ui_interface_) {
                ui_interface_->onLandingDetected();
            }
            MissionTransitionAuthority::requestTransition(
                mission,
                mission::MissionEvent::MISSION_COMPLETE);
        }
        break;

    case mission::MissionState::COMPLETE:
    case mission::MissionState::ABORTED:
        if (mission.state() == mission::MissionState::COMPLETE && ui_interface_) {
            ui_interface_->onMissionCompleted();
        }
        handleRecovery(mission, telemetry);
        break;

    default:
        break;
    }

    last_state_ = mission.state();
}

/* ---------------- SEARCH ---------------- */

void MissionController::handleSearch(
    mission::Mission& mission,
    const TelemetryData& telemetry)
{
    const auto now = std::chrono::steady_clock::now();

    if (last_state_ != mission::MissionState::SEARCH) {
        
        if (cmd_manager_) {
            double speed = mission.getProfile().performance.cruise_speed_m_s;
            std::cout << "[SEARCH] Setting cruise speed to: " << speed << " m/s\n";
            
            // This sends MAV_CMD_DO_CHANGE_SPEED (cmd=178)
            cmd_manager_->requestCommand(
                VehicleCommand::SET_SPEED,
                SystemState::ARMED,
                mission.state(),
                telemetry,
                static_cast<float>(speed) 
            );
        }

        if (!search_pattern_) {

            const auto& profile = mission.getProfile();

            if (!profile.waypoints.empty()) {

                GeoPoint center = profile.waypoints[0];

                // --------------------------------------------------
                // PHASE 2: ADAPTIVE SEARCH PATTERN SELECTION
                // --------------------------------------------------
                // Reasoning: Sector search is optimized for concentrated loitering,
                // while Expanding Square is best for wide-area coverage.
                if (profile.search_radius < 200.0) {
                    search_pattern_ = std::make_unique<SectorSearchPattern>(
                        center,
                        profile.search_radius
                    );
                    std::cout << "[SEARCH] Adaptive Choice: Sector Search Pattern (Radius: " 
                              << profile.search_radius << "m)\n";
                } 
                else {
                    search_pattern_ = std::make_unique<ExpandingSquarePattern>(
                        center,
                        100.0,  // initial side
                        50.0,   // step size
                        8       // max legs
                    );
                    std::cout << "[SEARCH] Adaptive Choice: Expanding Square Pattern\n";
                }
            }
            else {
                std::cout << "[SEARCH] No mission waypoints — aborting\n";

                MissionTransitionAuthority::requestTransition(
                    mission,
                    mission::MissionEvent::SYSTEM_FAILURE,
                    MissionAbortReason::SEARCH_EXHAUSTED,
                    "No waypoints for search");

                return;
            }
        }

        search_pattern_->reset();
        next_search_gen_time_ = now;

        std::cout << "[SEARCH] Initialized from mission profile\n";
    }

    if (telemetry.target_detected) {
        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::TARGET_DETECTED);
        return;
    }

    // --------------------------------------------------
    // SEARCH COMPLETION CHECK
    // --------------------------------------------------
    if (search_pattern_->completed()) {
        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::SYSTEM_FAILURE,
            MissionAbortReason::SEARCH_EXHAUSTED,
            "Search pattern exhausted");
        return;
    }

    // --------------------------------------------------
    // AUTO MODE: Monitor PX4's autonomous execution
    // --------------------------------------------------
    if (telemetry.nav_state == NavState::AUTO_MISSION ||
        telemetry.nav_state == NavState::AUTO_TAKEOFF) {
        if (telemetry.mission_current_received) {
            std::cout << "[SEARCH] PX4 executing mission waypoint " << telemetry.mission_current_seq << "\n";
        }
        return;  // Don't send search waypoints; let PX4 execute mission
    }

    // --------------------------------------------------
    // DYNAMIC WAYPOINT MODE: Generate and send waypoints via GCS
    // (Only when mission is NOT uploaded)
    // --------------------------------------------------
    
    // WAIT FOR POSITION
    if (!telemetry.position_received)
        return;

    // If no active waypoint → generate first one
    if (!current_waypoint_active_) {

        auto points = search_pattern_->next();
        if (points.empty())
            return;

        current_waypoint_ = points.front();
        current_waypoint_active_ = true;

        if (cmd_manager_) {
            cmd_manager_->sendSearchWaypoint(current_waypoint_);
        }

        return;
    }

   // Check if vehicle reached current waypoint
    double dlat = telemetry.latitude_deg  - current_waypoint_.lat;
    double dlon = telemetry.longitude_deg - current_waypoint_.lon;
    double distance_sq = dlat*dlat + dlon*dlon;

    // DYNAMIC FIX: Use the acceptance radius from the mission profile
    // Conversion: 1 meter is approximately 0.0000089 degrees
    double radius_m = mission.getProfile().performance.acceptance_radius_m;
    double radius_deg = radius_m * 0.000009; 
    double arrival_threshold_sq = radius_deg * radius_deg;

    if (distance_sq < arrival_threshold_sq) {
        current_waypoint_active_ = false;
        std::cout << "[SEARCH] Waypoint reached (Radius: " << radius_m << "m)\n";
    }
}
/* ---------------- ENGAGE ---------------- */

void MissionController::handleEngage(
    mission::Mission& mission,
    const TelemetryData& telemetry)
{
    // PRD REQUIREMENT: Define track for "Kill Chain" evaluation
    TargetTrack track {
        telemetry.target_detected,
        telemetry.target_confidence,
        telemetry.target_range_m,
        telemetry.target_closing_speed
    };

    // 1. VALIDATION: Check target presence
    if (!telemetry.target_detected || !engagement_policy_.checkClosingSpeed(track)) {
        engagement_policy_.registerMiss();
        AuditLogger::logDecision(VehicleCommand::NONE, mission.state(), "ENGAGEMENT_MISS", "Target lost");
        MissionTransitionAuthority::requestTransition(mission, mission::MissionEvent::ENGAGEMENT_FAILED);
        return;
    }

    // 2. POLICY EVALUATION
    EngagementDecision decision = engagement_policy_.evaluate(track);

    // 3. STRICT OITL (PRD: Force authorization for initial strike)
    if (decision == EngagementDecision::ENGAGE && engagement_policy_.getAttemptCount() == 0) {
        decision = EngagementDecision::REQUEST_CONFIRM;
    }

    // --- 4. EXPLOSIVE SAFETY GATE (WITH LATCHING TIMER & UI SYNC) ---
    const auto& policy = mission.getProfile().payload_policy;
    if (policy.type == mission::PayloadType::EXPLOSIVE && !payload_armed_confirmed_) {
        
        if (!OperatorAuthorization::hasPending()) {
            std::cout << "[SAFETY] Explosive Payload Detected. Requesting ARMING Authorization.\n";
            OperatorAuthorization::request(VehicleCommand::ARM_PAYLOAD, mission.state());
            return; 
        }

        auto auth = OperatorAuthorization::pollDecision();
        if (auth.has_value() && auth.value()) {
             // LATCHING: Send command once and start the clock
             if (cmd_manager_ && !arming_request_sent_) {
                 cmd_manager_->requestCommand(VehicleCommand::ARM_PAYLOAD, SystemState::ARMED, mission.state(), telemetry);
                 arming_request_sent_ = true;
                 arming_start_time_ = std::chrono::steady_clock::now();
             }
            // Calculate time remaining with the local variable you defined
             auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - arming_start_time_).count();
             double elapsed_secs = elapsed_ms / 1000.0;
             
             // Calculate time remaining (Using your local elapsed_secs variable)
             int remaining = static_cast<int>(policy.arming_delay_seconds - elapsed_secs);

             if (remaining > 0) {
                 if (ui_interface_) ui_interface_->sendPayloadArmingCountdown(remaining); 
                 return; 
             } 
             else {
                 payload_armed_confirmed_ = true; 
                 if (ui_interface_) ui_interface_->sendPayloadArmingCountdown(0); 

                 // Correct Backend Logging
                 AuditLogger::logDecision(VehicleCommand::ARM_PAYLOAD, mission.state(), 
                                        "PAYLOAD_ARMED", "Timer Complete");
                 OperatorAuthorization::consumeDecision();
             }
        } else if (auth.has_value() && !auth.value()) {
            MissionTransitionAuthority::requestTransition(mission, mission::MissionEvent::SYSTEM_FAILURE, MissionAbortReason::OPERATOR_DENIED);
            OperatorAuthorization::consumeDecision();
            return;
        }
        return; 
    }

    // --- 5. STRIKE AUTHORIZATION HANDLER ---
    if (decision == EngagementDecision::REQUEST_CONFIRM) {
        if (!OperatorAuthorization::hasPending()) {
            OperatorAuthorization::request(VehicleCommand::NONE, mission.state());
            return;
        }

        auto auth = OperatorAuthorization::pollDecision();
        if (!auth.has_value()) return;

        OperatorAuthorization::consumeDecision(); 

        if (!auth.value()) {
            MissionTransitionAuthority::requestTransition(mission, mission::MissionEvent::SYSTEM_FAILURE, MissionAbortReason::OPERATOR_DENIED);
            return;
        }

        MissionTransitionAuthority::requestTransition(mission, mission::MissionEvent::OPERATOR_ENGAGE_CONFIRM);
        decision = EngagementDecision::ENGAGE;
    }

    // --- 6. PHASE 3: TERMINAL GUIDANCE (Pro-Nav) ---
    if (decision == EngagementDecision::ENGAGE || decision == EngagementDecision::REENGAGE) {
        Vector3D interceptor_pos(telemetry.latitude_deg, telemetry.longitude_deg, telemetry.relative_alt_m);
        Vector3D target_pos(0, 0, 0); 
        Vector3D interceptor_vel(0, 0, 0); 
        Vector3D target_vel(0, 0, 0);

        Vector3D accel = pro_nav_.calculateAcceleration(interceptor_pos, target_pos, interceptor_vel, target_vel, 0.1);

        if (cmd_manager_) cmd_manager_->sendAccelerationCommand(accel);
        AuditLogger::logDecision(VehicleCommand::NONE, mission.state(), "TERMINAL_GUIDANCE", "Pro-Nav active");
    }
}
/* ---------------- RECOVERY ---------------- */

void MissionController::handleRecovery(
    mission::Mission& mission,
    const TelemetryData& telemetry)
{
    if (!mission.isStateNewlyEntered())
        return;

    mission.markStateHandled();

    VehicleCommand recovery_cmd = VehicleCommand::NONE;
    std::string reason;

    if (mission.state() == mission::MissionState::COMPLETE) {

        if (telemetry.isAirborne()) {
            recovery_cmd = VehicleCommand::SET_MODE_RTL;
            reason = "Mission complete — RTL";
        } 
        else {
            reason = "Mission complete — already on ground";
        }
    }
    else if (mission.state() == mission::MissionState::ABORTED) {

        if (telemetry.in_failsafe) {
            // DYNAMIC FIX: Respect the rules configured via UI or JSON profile
            mission::FailsafeBehavior rule = mission.getProfile().failsafe_rules.comms_loss;
            recovery_cmd = behaviorToCommand(rule);
            reason = "Failsafe Active: Executing configured rule (" + std::to_string(static_cast<int>(rule)) + ")";
        }
        else if (telemetry.isAirborne()) {
            recovery_cmd = VehicleCommand::SET_MODE_RTL;
            reason = "Mission aborted — RTL";
        }
        else {
            reason = "Mission aborted — already on ground";
        }
    }


    AuditLogger::logRecovery(
        mission.state(),
        recovery_cmd,
        reason
    );

    if (cmd_manager_ && recovery_cmd != VehicleCommand::NONE) {
        cmd_manager_->requestCommand(
            recovery_cmd,
            SystemState::ARMED,
            mission.state(),
            telemetry
        );
    }
}
void MissionController::handleAssess(
    mission::Mission& mission,
    const TelemetryData& telemetry)
{
    if (!mission.isStateNewlyEntered() && bda_completed_)
        return;

    if (mission.isStateNewlyEntered()) {
        assess_start_time_ = std::chrono::steady_clock::now();
        stable_frames_ = 0;
        bda_completed_ = false;

        if (ui_interface_) {
            ui_interface_->onBDAStarted();
        }

        AuditLogger::logDecision(
            VehicleCommand::NONE,
            mission.state(),
            "BDA_START",
            "Entering post-engagement assessment");

        mission.markStateHandled();
        return;
    }

    // --- Stability evaluation ---
    bool healthy_sample =
        telemetry.ekf_ok &&
        telemetry.battery_ok &&
        !telemetry.in_failsafe;

    if (healthy_sample)
        stable_frames_++;
    else
        stable_frames_ = 0;

    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - assess_start_time_).count();

    if (elapsed_ms < BDA_MIN_DURATION_MS ||
        stable_frames_ < BDA_STABLE_FRAMES)
        return;

    // --- Final BDA decision ---
    mission::BDAResult result = evaluateBDA(telemetry);

    // 🔹 PRD: BDA result MUST be logged before recovery decision
    AuditLogger::logBDAResult(
        mission.state(),
        result,
        "Post-engagement health evaluation complete");

    // Emit BDA result to UI
    if (ui_interface_) {
        std::string health_status;
        switch(result) {
            case mission::BDAResult::MISSION_WORTHY: health_status = "MISSION_WORTHY"; break;
            case mission::BDAResult::DEGRADED: health_status = "DEGRADED"; break;
            case mission::BDAResult::CRITICAL: health_status = "CRITICAL"; break;
            default: health_status = "UNKNOWN"; break;
        }
        ui_interface_->onBDAComplete(QString::fromStdString(health_status));
    }

    //  — Recovery Planning
    auto recovery_plan = buildRecoveryPlan(
        mission,
        telemetry,
        result);

    AuditLogger::logRecoveryPlan(mission.state(),
    static_cast<int>(recovery_plan.command));


    bda_completed_ = true;
    // --------------------------------------------------
    // Phase 4.3 — Dynamic Home Update (PRD)
    // --------------------------------------------------
    if (recovery_plan.source != mission::RecoverySource::ONBOARD_FAILSAFE &&
        recovery_plan.command == VehicleCommand::SET_MODE_RTL)
    {
        // Update PX4 Home BEFORE RTL
        AuditLogger::logDecision(
            VehicleCommand::SET_HOME,
            mission.state(),
            "HOME_UPDATE",
            "Source=" + std::to_string(int(recovery_plan.source)));

        if (cmd_manager_) {
            cmd_manager_->requestCommand(
                VehicleCommand::SET_HOME,
                SystemState::ARMED,
                mission.state(),
                telemetry
            );
        }
    }


    // --- Critical damage overrides everything ---
    if (result == mission::BDAResult::CRITICAL) {
        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::SYSTEM_FAILURE,
            MissionAbortReason::FAILSAFE_TRIGGERED,
            "Critical BDA failure");
        return;
    }

    // --- Assessment complete → proceed to RTB phase ---
    MissionTransitionAuthority::requestTransition(
        mission,
        mission::MissionEvent::BDA_EVALUATED);
}
mission::BDAResult MissionController::evaluateBDA(
    const TelemetryData& telemetry)
{
    if (telemetry.in_failsafe ||
        !telemetry.ekf_ok ||
        !telemetry.battery_ok)
        return mission::BDAResult::CRITICAL;


    if (!telemetry.isTelemetryReady())
        return mission::BDAResult::DEGRADED;

    return mission::BDAResult::MISSION_WORTHY;
}

mission::RecoveryPlan
MissionController::buildRecoveryPlan(
    const mission::Mission& mission,
    const TelemetryData& telemetry,
    mission::BDAResult bda) const
{
    mission::RecoveryPlan plan{};

    // -------------------------------
    // CRITICAL DAMAGE → FORCE LAND
    // -------------------------------
    if (bda == mission::BDAResult::CRITICAL) {
        plan.mode   = mission::RecoveryMode::LAND;
        plan.source = mission::RecoverySource::GCS_DYNAMIC;
        plan.command = VehicleCommand::LAND;
        return plan;
    }

    // ----------------------------------
    // PRIORITY 1: GCS Dynamic Override
    // ----------------------------------
    if (telemetry.isTelemetryReady()) {
        plan.mode   = mission::RecoveryMode::RTL;
        plan.source = mission::RecoverySource::GCS_DYNAMIC;
        plan.command = VehicleCommand::SET_MODE_RTL;
        return plan;
    }

    // ----------------------------------
    // PRIORITY 2: Mission Default
    // ----------------------------------
    if (mission.hasValidProfile()) {
        plan.mode   = mission::RecoveryMode::RTL;
        plan.source = mission::RecoverySource::MISSION_DEFAULT;
        plan.command = VehicleCommand::SET_MODE_RTL;
        return plan;
    }

    // ----------------------------------
    // PRIORITY 3: Onboard Failsafe
    // ----------------------------------
    plan.mode   = mission::RecoveryMode::RTL;
    plan.source = mission::RecoverySource::ONBOARD_FAILSAFE;
    plan.command = VehicleCommand::SET_MODE_RTL;
    return plan;
}




