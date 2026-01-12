#pragma once

#include <string>

namespace mission {

enum class MissionState {
    INIT,        // System boot, no mission loaded
    PREFLIGHT,   // BIT, safety config, payload config
    ARMED,       // Explicit operator authorization granted
    TRANSIT,     // Enroute to search area
    SEARCH,      // Autonomous search / loiter
    ENGAGE,      // Interception / kill chain
    RTB,         // Return to base
    COMPLETE,    // Mission finished successfully
    ABORTED      // Emergency termination
};

inline std::string to_string(MissionState state) {
    switch (state) {
        case MissionState::INIT: return "INIT";
        case MissionState::PREFLIGHT: return "PREFLIGHT";
        case MissionState::ARMED: return "ARMED";
        case MissionState::TRANSIT: return "TRANSIT";
        case MissionState::SEARCH: return "SEARCH";
        case MissionState::ENGAGE: return "ENGAGE";
        case MissionState::RTB: return "RTB";
        case MissionState::COMPLETE: return "COMPLETE";
        case MissionState::ABORTED: return "ABORTED";
        default: return "UNKNOWN";
    }
}

} // namespace mission
