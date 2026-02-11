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

                search_pattern_ = std::make_unique<ExpandingSquarePattern>(
                    center,
                    100.0,
                    50.0,
                    8
                );
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
    // WAIT FOR POSITION
    // --------------------------------------------------
    if (!telemetry.position_received)
        return;

    // --------------------------------------------------
    // If no active waypoint → generate first one
    // --------------------------------------------------
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

    // --------------------------------------------------
    // Check if vehicle reached current waypoint
    // --------------------------------------------------
    double dlat = telemetry.latitude_deg  - current_waypoint_.lat;
    double dlon = telemetry.longitude_deg - current_waypoint_.lon;

    double distance_sq = dlat*dlat + dlon*dlon;

    // ~2m threshold in degrees (~1e-5 deg ≈ 1m)
    constexpr double ARRIVAL_THRESHOLD = 1e-10;

    if (distance_sq < ARRIVAL_THRESHOLD) {

        current_waypoint_active_ = false;

        std::cout << "[SEARCH] Waypoint reached\n";
    }

}
/* ---------------- ENGAGE ---------------- */

void MissionController::handleEngage(
    mission::Mission& mission,
    const TelemetryData& telemetry)
{
    TargetTrack track {
        telemetry.target_detected,
        telemetry.target_confidence,
        telemetry.target_range_m,
        telemetry.target_closing_speed
    };

    if (!telemetry.target_detected ||
        telemetry.target_closing_speed <= 0.0) {

        engagement_policy_.registerMiss();

        AuditLogger::logDecision(
            VehicleCommand::NONE,
            mission.state(),
            "ENGAGEMENT_MISS",
            "Target lost or not closing");

        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::ENGAGEMENT_FAILED);

        return;
    }

    auto decision = engagement_policy_.evaluate(track);

    if (decision == EngagementDecision::REQUEST_CONFIRM) {

        if (!OperatorAuthorization::hasPending()) {
            OperatorAuthorization::request(
                VehicleCommand::NONE,
                mission.state());
            return;
        }

        auto auth = OperatorAuthorization::pollDecision();
        if (!auth.has_value())
            return;

        if (!auth.value()) {
            MissionTransitionAuthority::requestTransition(
                mission,
                mission::MissionEvent::SYSTEM_FAILURE,
                MissionAbortReason::OPERATOR_DENIED);
            return;
        }

        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::OPERATOR_ENGAGE_CONFIRM);

        decision = EngagementDecision::ENGAGE;
    }

    if (decision == EngagementDecision::ABORT) {
        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::SYSTEM_FAILURE,
            MissionAbortReason::SEARCH_EXHAUSTED);
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




