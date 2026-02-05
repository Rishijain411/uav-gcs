#pragma once

enum class MissionAbortReason {
    NONE,
    PX4_AUTO_DISARM,
    EKF_NOT_READY,
    COMMAND_TIMEOUT,
    OPERATOR_DENIED,
    FAILSAFE_TRIGGERED,
    SEARCH_EXHAUSTED,
    UNKNOWN
};
inline const char* toString(MissionAbortReason reason) {
    switch (reason) {
    case MissionAbortReason::NONE:
        return "NONE";
    case MissionAbortReason::PX4_AUTO_DISARM:
        return "PX4_AUTO_DISARM";
    case MissionAbortReason::EKF_NOT_READY:
        return "EKF_NOT_READY";
    case MissionAbortReason::COMMAND_TIMEOUT:
        return "COMMAND_TIMEOUT";
    case MissionAbortReason::OPERATOR_DENIED:
        return "OPERATOR_DENIED";
    case MissionAbortReason::FAILSAFE_TRIGGERED:
        return "FAILSAFE_TRIGGERED";
    case MissionAbortReason::SEARCH_EXHAUSTED:
        return "SEARCH_EXHAUSTED";
    default:
        return "UNKNOWN_ABORT_REASON";
    }
}
