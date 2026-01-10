#pragma once
#include "CommandDefinition.h"

inline bool canArm(const TelemetryData& t) {
    return t.heartbeat_received &&
           t.ekf_ok &&
           t.battery_ok &&
           t.isLanded() &&
           t.arm_state == ArmState::DISARMED &&
           !t.in_failsafe;
}

inline bool canDisarm(const TelemetryData& t) {
    return t.arm_state == ArmState::ARMED &&
           t.isLanded();
}

inline bool canSetAuto(const TelemetryData& t) {
    return t.arm_state == ArmState::ARMED &&
           !t.in_failsafe;
}

inline bool canTakeoff(const TelemetryData& t) {
    return t.arm_state == ArmState::ARMED &&
           t.isLanded() &&
           t.ekf_ok &&
           !t.in_failsafe;
}

inline bool canLand(const TelemetryData& t) {
    return t.isAirborne();
}
