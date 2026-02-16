#include "MissionController.h"
#include "command/CommandManager.h"
#include "telemetry/TelemetryData.h"

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

    // If link is lost for > 2 seconds, trigger failsafe transition
    if (hb_elapsed_ms > 2000 && mission.state() != mission::MissionState::INIT) {
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
    static int armed_stable_frames = 0;

    if (mission.state() == mission::MissionState::ARM_REQUESTED) {

        if (telemetry.arm_state == ArmState::ARMED &&
            telemetry.ekf_ok &&
            telemetry.battery_ok &&
            !telemetry.in_failsafe)
        {
            armed_stable_frames++;
        } else {
            armed_stable_frames = 0;
        }

        if (armed_stable_frames >= 5) {
            MissionTransitionAuthority::requestTransition(
                mission,
                mission::MissionEvent::VEHICLE_ARMED);
            armed_stable_frames = 0;
        }
    }

    // ==================================================
    // ARMED → OITL AUTO MODE 
    // ==================================================
    if (mission.state() == mission::MissionState::ARMED &&
        !auto_mode_sent_ &&
        !OperatorAuthorization::hasPending())
    {
        OperatorAuthorization::request(
            VehicleCommand::SET_MODE_AUTO,
            mission.state());
        return;
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

        auto_mode_sent_ = false; // prevent repeat
        return;
    }

    // ==================================================
    // ABORT HANDLING
    // ==================================================
    if (mission.state() != mission::MissionState::ABORTED &&
        mission.state() != mission::MissionState::COMPLETE)
    {
        if (cmd_manager_ && cmd_manager_->hasCommandTimedOut()) {
            MissionTransitionAuthority::requestTransition(
                mission,
                mission::MissionEvent::SYSTEM_FAILURE,
                MissionAbortReason::COMMAND_TIMEOUT,
                "Command retry limit exceeded");
            return;
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
        if (telemetry.isLanded()) {
            MissionTransitionAuthority::requestTransition(
                mission,
                mission::MissionEvent::MISSION_COMPLETE);
        }
        break;

    case mission::MissionState::COMPLETE:
    case mission::MissionState::ABORTED:
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
    // MISSION UPLOADED MODE: Monitor PX4's autonomous execution
    // --------------------------------------------------
    if (telemetry.mission_upload_complete) {
        // Mission is uploaded to PX4, monitor via MISSION_CURRENT
        if (telemetry.mission_current_received) {
            std::cout << "[SEARCH] PX4 executing mission waypoint " << telemetry.mission_current_seq << "\n";
        }
        return;  // Don't send search waypoints; let PX4 execute uploaded mission
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

    // 5m threshold in degrees (~5e-5 deg ≈ 5m)
    constexpr double ARRIVAL_THRESHOLD_SQ = 25e-10;  // (5e-5)^2

    if (distance_sq < ARRIVAL_THRESHOLD_SQ) {

        current_waypoint_active_ = false;

        std::cout << "[SEARCH] Waypoint reached\n";
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

    // 1. VALIDATION: Check target presence and minimum closing speed threshold (5.0 m/s)
    if (!telemetry.target_detected || !engagement_policy_.checkClosingSpeed(track)) {
        engagement_policy_.registerMiss();

        AuditLogger::logDecision(
            VehicleCommand::NONE,
            mission.state(),
            "ENGAGEMENT_MISS",
            "Target lost or insufficient closing speed");

        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::ENGAGEMENT_FAILED);
        return;
    }

    // 2. POLICY EVALUATION: Check lock confidence and attempt counts
    EngagementDecision decision = engagement_policy_.evaluate(track);

    // 3. STRICT OITL (PRD: No "Ghost" Commands): Force authorization for initial strike
    // If the policy suggests ENGAGE but we haven't attempted yet, treat as REQUEST_CONFIRM.
    if (decision == EngagementDecision::ENGAGE && engagement_policy_.getAttemptCount() == 0) {
        decision = EngagementDecision::REQUEST_CONFIRM;
    }

    // 4. AUTHORIZATION HANDLER
    if (decision == EngagementDecision::REQUEST_CONFIRM) {
        if (!OperatorAuthorization::hasPending()) {
            OperatorAuthorization::request(VehicleCommand::NONE, mission.state());
            return;
        }

        auto auth = OperatorAuthorization::pollDecision();
        if (!auth.has_value()) return;

        OperatorAuthorization::consumeDecision(); // Clear request after decision

        if (!auth.value()) {
            MissionTransitionAuthority::requestTransition(
                mission,
                mission::MissionEvent::SYSTEM_FAILURE,
                MissionAbortReason::OPERATOR_DENIED);
            return;
        }

        // Operator approved: move state to authorized engagement
        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::OPERATOR_ENGAGE_CONFIRM);
        
        decision = EngagementDecision::ENGAGE;
    }

    // 5. PHASE 3: TERMINAL GUIDANCE (Proportional Navigation)
    if (decision == EngagementDecision::ENGAGE || decision == EngagementDecision::REENGAGE) {
        
        // PHASE 3: Terminal Guidance Execution
        // Map interceptor position and velocity from telemetry
        Vector3D interceptor_pos(telemetry.latitude_deg, telemetry.longitude_deg, telemetry.relative_alt_m);
        
        // NOTE: Placeholder target_pos until Radar/AI data is added to TelemetryData.h
        Vector3D target_pos(0, 0, 0); 
        Vector3D interceptor_vel(0, 0, 0); // Requires NED velocities from telemetry
        Vector3D target_vel(0, 0, 0);

        Vector3D accel = pro_nav_.calculateAcceleration(
            interceptor_pos,
            target_pos,
            interceptor_vel,
            target_vel,
            0.1 // 10Hz DT based on search_gen_interval_
        );

        if (cmd_manager_) {
            cmd_manager_->sendAccelerationCommand(accel);
        }

        AuditLogger::logDecision(
            VehicleCommand::NONE,
            mission.state(),
            "TERMINAL_GUIDANCE",
            "Pro-Nav active: Accel Magnitude = " + std::to_string(accel.magnitude()));
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
            recovery_cmd = VehicleCommand::LAND;
            reason = "Mission aborted — CRITICAL, LAND";
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




