#pragma once

#include <iostream>
#include <string>

#include "utils/EnumStrings.h"
#include "mission/Mission.h"
#include "mission/MissionEvent.h"
#include "mission/MissionAbortReason.h"
#include "authority/OperatorAuthorization.h"
#include "authority/AuditLogger.h"
#include "command/VehicleCommand.h"

class MissionTransitionAuthority {
public:
    static bool requestTransition(
        mission::Mission& mission,
        mission::MissionEvent event,
        MissionAbortReason reason = MissionAbortReason::NONE,
        const std::string& detail = "")
    {
        auto from_state = mission.state();

        // ----------------------------------
        // CHECK IF HUMAN CONFIRMATION REQUIRED
        // ----------------------------------
        if (requiresOperatorConfirmation(from_state, event)) {

            if (!OperatorAuthorization::hasPending()) {
                std::cout << "[MISSION AUTH] Operator confirmation required\n";

                OperatorAuthorization::request(
                    VehicleCommand::NONE,
                    from_state
                );

                return false; // waiting
            }

            auto decision = OperatorAuthorization::pollDecision();
            if (!decision.has_value()) {
                return false; // still waiting
            }

            if (!decision.value()) {
                OperatorAuthorization::consumeDecision();

                AuditLogger::logMissionTransition(
                    from_state,
                    event,
                    "DENIED",
                    "Operator rejected");

                std::cout << "[MISSION AUTH] Transition denied by operator\n";
                return false;
            }
        }

        // ----------------------------------
        // PRD: Explicit Abort Reason Handling
        // ----------------------------------
        if (event == mission::MissionEvent::SYSTEM_FAILURE) {

            mission.setAbortReason(reason);

            AuditLogger::logMissionAbort(
                mission.state(),
                reason,
                detail
            );

            std::cout << "[MISSION ABORT] Reason="
                      << toString(reason);

            if (!detail.empty())
                std::cout << " (" << detail << ")";

            std::cout << std::endl;
        }

        // ----------------------------------
        // APPLY TRANSITION
        // ----------------------------------
        bool ok = mission.apply_event(event);

        if (ok) {
            OperatorAuthorization::consumeDecision();

            AuditLogger::logMissionTransition(
                from_state,
                event,
                "ACCEPTED");

            std::cout << "[MISSION] "
                      << mission::to_string(from_state)
                      << " → "
                      << mission::to_string(mission.state())
                      << std::endl;
        } else {
            AuditLogger::logMissionTransition(
                from_state,
                event,
                "REJECTED",
                "Policy denied");

            std::cout << "[MISSION AUTH] Transition rejected by policy\n";
        }

        return ok;
    }

private:
    static bool requiresOperatorConfirmation(
        mission::MissionState from,
        mission::MissionEvent event)
    {
        return
            (from == mission::MissionState::PREFLIGHT &&
             event == mission::MissionEvent::PREFLIGHT_OK) ||

            (from == mission::MissionState::SEARCH &&
             event == mission::MissionEvent::TARGET_DETECTED) ||

            (from == mission::MissionState::ENGAGE &&
             event == mission::MissionEvent::ENGAGEMENT_COMPLETE);
    }
};
