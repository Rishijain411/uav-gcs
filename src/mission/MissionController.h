#pragma once

#include <memory>
#include <iostream>
#include <chrono>

using namespace std;

#include "mission/Mission.h"
#include "mission/MissionEvent.h"

// 🔹 Forward declaration (NO include here)
struct TelemetryData;
class CommandManager;
class GCSBackendInterface;

#include "authority/MissionTransitionAuthority.h"
#include "mission/search/SearchPattern.h"
#include "mission/search/ExpandingSquarePattern.h"
#include "mission/search/SectorSearchPattern.h"
#include "mission/engagement/EngagementPolicy.h"
#include "mission/engagement/ProportionalNavigation.h"
#include "authority/OperatorAuthorization.h"
#include "authority/AuditLogger.h"
#include "mission/BDAResult.h"
#include "mission/RecoveryPlan.h"



class MissionController {
public:
    MissionController();

    void update(
        mission::Mission& mission,
        const TelemetryData& telemetry);

    // Phase C: Set command manager for waypoint publishing
    void setCommandManager(CommandManager* cmd_manager) {
        cmd_manager_ = cmd_manager;
    }
    
    // Phase 4: Set UI interface for signal emission
    void setUIInterface(GCSBackendInterface* ui_interface) {
        ui_interface_ = ui_interface;
    }

private:
    unique_ptr<SearchPattern> search_pattern_;
    EngagementPolicy engagement_policy_;
    CommandManager* cmd_manager_ = nullptr;
    GCSBackendInterface* ui_interface_ = nullptr;  // New: UI signal interface
    ProportionalNavigation pro_nav_;
    // Phase 4.1 — BDA tracking
    std::chrono::steady_clock::time_point assess_start_time_;
    int stable_frames_ = 0;
    bool bda_completed_ = false;
    bool auto_mode_authorized_ = false;
    bool takeoff_requested_ = false;
    bool takeoff_completed_ = false;
    bool auto_mode_sent_ = false;
    
    // Arm stability tracking: wait for consistent health before transitioning to AUTO
    int arm_stable_frames_ = 0;
    static constexpr int ARM_STABLE_FRAMES_REQUIRED = 30;  // ~300ms at 100Hz - allow EKF/GPS to settle
    GeoPoint current_waypoint_;
    bool current_waypoint_active_ = false;



    // Phase C: SEARCH autonomy (generate periodically, reset on entry)
    mission::MissionState last_state_ = mission::MissionState::INIT;
    std::chrono::steady_clock::time_point next_search_gen_time_{};
    std::chrono::milliseconds search_gen_interval_{1000}; // 1 Hz waypoint intent generation
    static constexpr int BDA_MIN_DURATION_MS = 1000;
    static constexpr int BDA_STABLE_FRAMES = 5;
    void handleSearch(
        mission::Mission& mission,
        const TelemetryData& telemetry);

    void handleEngage(
        mission::Mission& mission,
        const TelemetryData& telemetry);

    void handleRecovery(
    mission::Mission& mission,
    const TelemetryData& telemetry);

    mission::BDAResult evaluateBDA(const TelemetryData& telemetry);
    void handleAssess(mission::Mission& mission, const TelemetryData& telemetry);
    
    mission::RecoveryPlan buildRecoveryPlan(
    const mission::Mission& mission,
    const TelemetryData& telemetry,
    mission::BDAResult bda) const;

};
