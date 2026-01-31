#pragma once

#include "MissionProfile.h"
#include <string>

namespace mission {

class MissionProfileParser {
public:
    // Parse JSON file into MissionProfile
    static bool parseFromJson(
        const std::string& filepath,
        MissionProfile& profile,
        std::string& error_message);
    
    // Parse YAML file into MissionProfile (future)
    // static bool parseFromYaml(...);
    
    // Validate mission profile
    static bool validate(
        const MissionProfile& profile,
        std::string& error_message);

private:
    // Helper: Parse GeoPoint from JSON array [lat, lon, alt]
    static bool parseGeoPoint(
        const std::string& json_array,
        GeoPoint& point);
    
    // Helper: Parse failsafe behavior from string
    static FailsafeBehavior parseFailsafeBehavior(const std::string& str);
    
    // Helper: Parse payload type from string
    static PayloadType parsePayloadType(const std::string& str);
};

} // namespace mission

