#pragma once

#include "command/VehicleCommand.h"
#include "mission/MissionState.h"

// ---------------- VehicleCommand ----------------
inline const char* toString(VehicleCommand cmd) {
    switch (cmd) {
    case VehicleCommand::NONE:          return "NONE";
    case VehicleCommand::ARM:           return "ARM";
    case VehicleCommand::DISARM:        return "DISARM";
    case VehicleCommand::TAKEOFF:       return "TAKEOFF";
    case VehicleCommand::LAND:          return "LAND";
    case VehicleCommand::SET_MODE_AUTO: return "SET_MODE_AUTO";
    default:                            return "UNKNOWN_COMMAND";
    }
}

// ---------------- MissionState ----------------
inline const char* toString(mission::MissionState s) {
    switch (s) {
    case mission::MissionState::INIT:          return "INIT";
    case mission::MissionState::PREFLIGHT:     return "PREFLIGHT";
    case mission::MissionState::ARM_REQUESTED: return "ARM_REQUESTED"; // 🔹 FIX
    case mission::MissionState::ARMED:         return "ARMED";
    case mission::MissionState::TRANSIT:       return "TRANSIT";
    case mission::MissionState::SEARCH:        return "SEARCH";
    case mission::MissionState::ENGAGE:        return "ENGAGE";
    case mission::MissionState::ASSESS:        return "ASSESS";
    case mission::MissionState::RTB:           return "RTB";
    case mission::MissionState::COMPLETE:      return "COMPLETE";
    case mission::MissionState::ABORTED:       return "ABORTED";
    default:                                   return "UNKNOWN_STATE";
    }
}
