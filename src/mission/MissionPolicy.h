#pragma once

#include "MissionState.h"
#include "MissionEvent.h"

namespace mission {

class MissionPolicy {
public:
    // Can this event be applied in the current state?
    static bool is_event_allowed(MissionState state, MissionEvent event) {
        switch (state) {

            case MissionState::INIT:
                return event == MissionEvent::LOAD_MISSION;

            case MissionState::PREFLIGHT:
                return event == MissionEvent::PREFLIGHT_OK ||
                       event == MissionEvent::PREFLIGHT_FAIL ||
                       event == MissionEvent::OPERATOR_ABORT;

            case MissionState::ARMED:
                return event == MissionEvent::TRANSIT_REACHED ||
                       event == MissionEvent::OPERATOR_ABORT;

            case MissionState::TRANSIT:
                return event == MissionEvent::TRANSIT_REACHED ||
                       event == MissionEvent::SYSTEM_FAILURE;

            case MissionState::SEARCH:
                return event == MissionEvent::TARGET_DETECTED ||
                       event == MissionEvent::SYSTEM_FAILURE;

            case MissionState::ENGAGE:
                return event == MissionEvent::ENGAGEMENT_COMPLETE ||
                       event == MissionEvent::ENGAGEMENT_FAILED ||
                       event == MissionEvent::SYSTEM_FAILURE;

            case MissionState::RTB:
                return event == MissionEvent::RTB_COMPLETE ||
                       event == MissionEvent::SYSTEM_FAILURE;

            case MissionState::COMPLETE:
            case MissionState::ABORTED:
                return false;
        }
        return false;
    }

    // Does this event require explicit operator confirmation?
    static bool requires_operator_confirmation(MissionEvent event) {
        return event == MissionEvent::OPERATOR_ARM_CONFIRM ||
               event == MissionEvent::OPERATOR_ENGAGE_CONFIRM;
    }
};

} // namespace mission
