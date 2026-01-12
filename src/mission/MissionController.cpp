#include "MissionController.h"

// 🔹 REAL definition lives here
#include "telemetry/TelemetryData.h"


MissionController::MissionController()
    : engagement_policy_(3) // PRD: predefined N attempts
{
    // Search centered on launch (example)
    GeoPoint center { 0.0, 0.0, 50.0 };

    search_pattern_ = make_unique<ExpandingSquarePattern>(
        center,
        100.0,   // initial leg (m)
        50.0,    // expansion step (m)
        8        // max legs
    );
}

void MissionController::update(
    mission::Mission& mission,
    const TelemetryData& telemetry)
{
    switch (mission.state()) {

    case mission::MissionState::SEARCH:
        handleSearch(mission, telemetry);
        break;

    case mission::MissionState::ENGAGE:
        handleEngage(mission, telemetry);
        break;

    default:
        break;
    }
}
void MissionController::handleSearch(
    mission::Mission& mission,
    const TelemetryData& telemetry)
{
    // Example hook: AI / radar later
    if (telemetry.target_detected) {

        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::TARGET_DETECTED);

        return;
    }

    if (search_pattern_->completed()) {

        AuditLogger::logDecision(
            VehicleCommand::NONE,
            mission.state(),
            "SEARCH_COMPLETE",
            "Pattern exhausted");

        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::SYSTEM_FAILURE); // or RTB

        return;
    }

    // Generate next intent (not sent yet)
    auto points = search_pattern_->next();

    cout << "[SEARCH] Generated "
         << points.size()
         << " search waypoint(s)\n";
}


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

    auto decision = engagement_policy_.evaluate(track);

    // --------------------------------------------------
    // Handle operator confirmation if required
    // --------------------------------------------------
    if (decision == EngagementDecision::REQUEST_CONFIRM) {

        if (!OperatorAuthorization::hasPending()) {
            OperatorAuthorization::request(
                VehicleCommand::NONE,
                mission.state());
            return;
        }

        auto auth_decision = OperatorAuthorization::pollDecision();
        if (!auth_decision.has_value())
            return;

        if (!auth_decision.value()) {
            cout << "[ENGAGE] Operator denied engagement completion\n";
            return;
        }

        // Authorization granted → proceed as ENGAGE
        decision = EngagementDecision::ENGAGE;
    }

    // --------------------------------------------------
    // Execute engagement decision
    // --------------------------------------------------
    switch (decision) {

    case EngagementDecision::ENGAGE:
        AuditLogger::logDecision(
            VehicleCommand::NONE,
            mission.state(),
            "ENGAGE_APPROVED");

        cout << "[ENGAGE] Engagement authorized\n";
        break;

    case EngagementDecision::REENGAGE:
        engagement_policy_.registerMiss();
        cout << "[ENGAGE] Re-engagement authorized\n";
        break;

    case EngagementDecision::ABORT:
        MissionTransitionAuthority::requestTransition(
            mission,
            mission::MissionEvent::SYSTEM_FAILURE);
        break;

    case EngagementDecision::HOLD:
    default:
        break;
    }

}
