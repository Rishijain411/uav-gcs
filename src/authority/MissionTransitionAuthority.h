#pragma once

#include <iostream>
using namespace std;
#include "utils/EnumStrings.h"
#include "mission/Mission.h"
#include "mission/MissionEvent.h"
#include "authority/OperatorAuthorization.h"
#include "authority/AuditLogger.h"
#include "command/VehicleCommand.h"

class MissionTransitionAuthority {
public:
    static bool requestTransition(
        mission::Mission& mission,
        mission::MissionEvent event)
    {
        auto from_state = mission.state();

        // ----------------------------------
        // CHECK IF HUMAN CONFIRMATION REQUIRED
        // ----------------------------------
        if (requiresOperatorConfirmation(from_state, event)) {

            // If no pending request, issue one
            if (!OperatorAuthorization::hasPending()) {
                cout << "[MISSION AUTH] Operator confirmation required\n";

                OperatorAuthorization::request(
                    VehicleCommand::NONE,   // mission-level decision
                    from_state
                );

                return false; // wait for operator
            }

            // Poll operator decision (non-blocking)
            auto decision = OperatorAuthorization::pollDecision();
            if (!decision.has_value()) {
                return false; // still waiting
            }

            if (!decision.value()) {
                OperatorAuthorization::consumeDecision();  // NEW: Mark decision as consumed
                
                AuditLogger::logMissionTransition(
                    from_state,
                    event,
                    "DENIED",
                    "Operator rejected");

                cout << "[MISSION AUTH] Transition denied by operator\n";
                return false;
            }
        }

        // ----------------------------------
        // APPLY TRANSITION
        // ----------------------------------
        bool ok = mission.apply_event(event);

        if (ok) {
            OperatorAuthorization::consumeDecision();  // NEW: Mark decision as consumed
            
            AuditLogger::logMissionTransition(
                from_state,
                event,
                "ACCEPTED");

            cout << "[MISSION] "
                 << mission::to_string(from_state)
                 << " → "
                 << mission::to_string(mission.state())
                 << endl;
        } else {
            AuditLogger::logMissionTransition(
                from_state,
                event,
                "REJECTED",
                "Policy denied");

            cout << "[MISSION AUTH] Transition rejected by policy\n";
        }

        return ok;
    }

private:
    static bool requiresOperatorConfirmation(
        mission::MissionState from,
        mission::MissionEvent event)
    {
        // PRD: Explicit human confirmation required for escalation
        return
            (from == mission::MissionState::PREFLIGHT &&
             event == mission::MissionEvent::PREFLIGHT_OK) ||

            (from == mission::MissionState::SEARCH &&
             event == mission::MissionEvent::TARGET_DETECTED) ||

            (from == mission::MissionState::ENGAGE &&
             event == mission::MissionEvent::ENGAGEMENT_COMPLETE);
    }
};
