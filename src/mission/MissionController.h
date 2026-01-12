#pragma once

#include <memory>
#include <iostream>

using namespace std;

#include "mission/Mission.h"
#include "mission/MissionEvent.h"

// 🔹 Forward declaration (NO include here)
struct TelemetryData;

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

private:
    unique_ptr<SearchPattern> search_pattern_;
    EngagementPolicy engagement_policy_;

    void handleSearch(
        mission::Mission& mission,
        const TelemetryData& telemetry);

    void handleEngage(
        mission::Mission& mission,
        const TelemetryData& telemetry);
};
