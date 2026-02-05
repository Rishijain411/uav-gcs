#include "Mission.h"
#include "MissionPolicy.h"
#include "mission/MissionAbortReason.h"

namespace mission {

Mission::Mission()
    : current_state_(MissionState::INIT),
      bit_status_() {}

MissionState Mission::state() const {
    return current_state_;
}

bool Mission::isStateNewlyEntered() const {
    return state_just_changed_;
}

void Mission::markStateHandled() {
    state_just_changed_ = false;
}
MissionState Mission::transition(MissionState state, MissionEvent event) {
    switch (state) {

        case MissionState::INIT:
            if (event == MissionEvent::LOAD_MISSION)
                return MissionState::PREFLIGHT;
            break;

        case MissionState::PREFLIGHT:
            if (event == MissionEvent::PREFLIGHT_OK)
                return MissionState::ARMED;
            if (event == MissionEvent::PREFLIGHT_FAIL)
                return MissionState::ABORTED;
            break;

        case MissionState::ARMED:
            if (event == MissionEvent::TRANSIT_REACHED)
                return MissionState::TRANSIT;
            break;

        case MissionState::TRANSIT:
            if (event == MissionEvent::TRANSIT_REACHED)
                return MissionState::SEARCH;
            break;

        case MissionState::SEARCH:
            if (event == MissionEvent::OPERATOR_ENGAGE_CONFIRM)
                return MissionState::ENGAGE;
            break;


        case MissionState::ENGAGE:
            if (event == MissionEvent::ENGAGEMENT_COMPLETE)
                return MissionState::RTB;
            if (event == MissionEvent::ENGAGEMENT_FAILED)
                return MissionState::SEARCH;
            break;

        case MissionState::RTB:
            if (event == MissionEvent::RTB_COMPLETE)
                return MissionState::COMPLETE;
            break;

        default:
            break;
    }

    return MissionState::ABORTED;
}

bool Mission::loadProfile(const MissionProfile& profile) {
    if (!profile.isValid()) {
        return false;
    }
    profile_ = profile;
    return true;
}

bool Mission::hasValidProfile() const {
    return profile_.has_value() && profile_.value().isValid();
}

const MissionProfile& Mission::getProfile() const {
    return profile_.value();
}
bool Mission::apply_event(MissionEvent event) {
    auto next = transition(current_state_, event);

    if (next != current_state_) {
        current_state_ = next;
        state_just_changed_ = true;

        // ✅ CLEAR abort reason unless this is an abort
        if (event != MissionEvent::SYSTEM_FAILURE) {
            abort_reason_ = MissionAbortReason::NONE;
        }

        return true;
    }
    return false;
}
// PRD: Abort Reason Latching (Authoritative)
// ---------------------------------------------
void Mission::setAbortReason(MissionAbortReason reason) {
    abort_reason_ = reason;
}

MissionAbortReason Mission::getAbortReason() const {
    return abort_reason_;
}


} // namespace mission
