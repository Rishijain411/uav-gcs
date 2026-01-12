#pragma once

namespace mission {

enum class MissionEvent {
    LOAD_MISSION,
    PREFLIGHT_OK,
    PREFLIGHT_FAIL,

    OPERATOR_ARM_CONFIRM,
    OPERATOR_ABORT,

    TRANSIT_REACHED,
    TARGET_DETECTED,

    OPERATOR_ENGAGE_CONFIRM,
    ENGAGEMENT_COMPLETE,
    ENGAGEMENT_FAILED,

    RTB_COMPLETE,
    SYSTEM_FAILURE
};

} // namespace mission
