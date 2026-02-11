#pragma once

namespace mission {

enum class BDAResult {
    MISSION_WORTHY = 0,
    DEGRADED       = 1,
    CRITICAL       = 2
};

inline const char* toString(BDAResult r) {
    switch (r) {
        case BDAResult::MISSION_WORTHY: return "MISSION_WORTHY";
        case BDAResult::DEGRADED:       return "DEGRADED";
        case BDAResult::CRITICAL:       return "CRITICAL";
        default:                        return "UNKNOWN";
    }
}

} // namespace mission
