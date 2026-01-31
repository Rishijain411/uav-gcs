#include "MissionController.h"
#include "command/CommandManager.h"

// 🔹 REAL definition lives here
#include "telemetry/TelemetryData.h"
#include <chrono>


MissionController::MissionController()
    : engagement_policy_([]{
        EngagementConfig cfg;
        cfg.max_reengagement_attempts = 3;
        return cfg;
    }()) // PRD: predefined N attempts
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
    // Track state entry/exit for autonomy behaviors
    const auto current_state = mission.state();

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

    last_state_ = current_state;
}
void MissionController::handleSearch(
    mission::Mission& mission,
    const TelemetryData& telemetry)
{
    const auto now = std::chrono::steady_clock::now();

    // Reset pattern and scheduling when we ENTER SEARCH
    if (last_state_ != mission::MissionState::SEARCH) {
        search_pattern_->reset();
        next_search_gen_time_ = now; // generate immediately on entry
        cout << "[SEARCH] Entered SEARCH: pattern reset\n";

        // Phase C: Request AUTO mode on SEARCH entry
        if (cmd_manager_) {
            cout << "[SEARCH] Requesting SET_MODE_AUTO for autonomous search\n";
            cmd_manager_->requestCommand(
                VehicleCommand::SET_MODE_AUTO,
                SystemState::ARMED,
                mission::MissionState::SEARCH,
                telemetry);
        }
    }

    // Example hook: AI / radar later
    if (telemetry.target_detected) {
        cout << "[SEARCH] Target detected -> requesting transition to ENGAGE\n";

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

    // Generate next intent periodically (not sent yet)
    if (now < next_search_gen_time_)
        return;

    next_search_gen_time_ = now + search_gen_interval_;
    auto points = search_pattern_->next();

    cout << "[SEARCH] Generated "
         << points.size()
         << " search waypoint(s)\n";

    // Print and send the waypoints
    for (const auto& p : points) {
        cout << "  wp: lat=" << p.lat
             << " lon=" << p.lon
             << " alt=" << p.alt << "\n";

        // Phase C: Send to vehicle via CommandManager
        if (cmd_manager_) {
            cmd_manager_->sendSearchWaypoint(p);
        }
    }
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
