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

#include "authority/MissionTransitionAuthority.h"
#include "mission/search/SearchPattern.h"
#include "mission/search/ExpandingSquarePattern.h"
#include "mission/engagement/EngagementPolicy.h"
#include "authority/OperatorAuthorization.h"
#include "authority/AuditLogger.h"

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

private:
    unique_ptr<SearchPattern> search_pattern_;
    EngagementPolicy engagement_policy_;
    CommandManager* cmd_manager_ = nullptr;

    // Phase C: SEARCH autonomy (generate periodically, reset on entry)
    mission::MissionState last_state_ = mission::MissionState::INIT;
    std::chrono::steady_clock::time_point next_search_gen_time_{};
    std::chrono::milliseconds search_gen_interval_{1000}; // 1 Hz waypoint intent generation

    void handleSearch(
        mission::Mission& mission,
        const TelemetryData& telemetry);

    void handleEngage(
        mission::Mission& mission,
        const TelemetryData& telemetry);
};
