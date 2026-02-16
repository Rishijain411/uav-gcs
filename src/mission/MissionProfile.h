#pragma once

#include <string>
#include <vector>
#include <optional>
#include <array>
#include <cstdint>
#include "mission/search/SearchPattern.h"

namespace mission {

// Payload types (PRD: Kinetic/Explosive)
enum class PayloadType {
    NONE,
    KINETIC,
    EXPLOSIVE
};

// Failsafe behaviors (PRD: Comms Loss, Low Battery, GPS Jamming)
enum class FailsafeBehavior {
    LAND,           // Immediate landing
    RTL,            // Return to launch
    HOLD,           // Hold position
    CONTINUE        // Continue mission
};

// Failsafe configuration
struct FailsafeRules {
    FailsafeBehavior comms_loss = FailsafeBehavior::RTL;
    FailsafeBehavior low_battery = FailsafeBehavior::RTL;
    FailsafeBehavior gps_jamming = FailsafeBehavior::LAND;
};

// Payload policy
struct PayloadPolicy {
    PayloadType type = PayloadType::NONE;
    bool arm_on_engagement = false;  // Arm payload only when engaging
    double arming_delay_seconds = 0.0;
};

// GPS Polygon (for search area)
struct SearchArea {
    std::vector<GeoPoint> vertices;  // Polygon vertices
    double min_altitude_m = 0.0;
    double max_altitude_m = 100.0;
    
    bool isValid() const {
        return vertices.size() >= 3;  // Minimum 3 points for polygon
    }
};
// Phase 5.2 — Mission Crypto
struct MissionCrypto {
    std::array<uint8_t, 32> mission_key{};
    uint32_t key_epoch = 0;
    std::string key_id;

    bool isValid() const {
        return !key_id.empty();
    }
};


// Mission Profile (PRD Phase 1)
struct MissionProfile {
    // Schema version (optional in JSON; defaults to 1)
    int schema_version = 1;

    // Target identification (optional)
    std::optional<std::string> target_id;
    
    // Search area (GPS polygon)
    SearchArea search_area;

    // Added for Phase 2: Adaptive Search Patterns
    double search_radius = 0.0;
    
    // Flight path waypoints
    std::vector<GeoPoint> waypoints;

    // Payload configuration
    PayloadPolicy payload_policy;
    
    // Failsafe rules
    FailsafeRules failsafe_rules;

    // Mission crypto
    MissionCrypto crypto;
    
    // Validation
    bool isValid() const {
        return search_area.isValid() &&
               !waypoints.empty() &&
               crypto.isValid();
    }
};

} 


