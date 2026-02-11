#include "MissionProfileParser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

// Simple JSON parser (for Phase B - can be replaced with library later)
// We'll use a basic string-based parser for now

namespace mission {

    bool MissionProfileParser::parseFromJson(
        const std::string& filepath,
        MissionProfile& profile,
        std::string& error_message)
    {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            error_message = "Failed to open file: " + filepath;
            return false;
        }
        
        // Read entire file
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json_content = buffer.str();
        file.close();
        
        // Simple JSON parsing (basic implementation)
        // For production, use a proper JSON library like nlohmann/json
        
        // Reset profile
        profile = MissionProfile();
        
        // Parse target_id (optional)
        size_t target_pos = json_content.find("\"target_id\"");
        if (target_pos != std::string::npos) {
            size_t colon_pos = json_content.find(':', target_pos);
            size_t quote_start = json_content.find('"', colon_pos);
            if (quote_start != std::string::npos) {
                size_t quote_end = json_content.find('"', quote_start + 1);
                if (quote_end != std::string::npos) {
                    profile.target_id = json_content.substr(
                        quote_start + 1, quote_end - quote_start - 1);
                }
            }
        }
        
        // Parse search_area vertices
        size_t search_area_pos = json_content.find("\"search_area\"");
        if (search_area_pos != std::string::npos) {
            size_t vertices_pos = json_content.find("\"vertices\"", search_area_pos);
            if (vertices_pos != std::string::npos) {
                size_t array_start = json_content.find('[', vertices_pos);
                if (array_start != std::string::npos) {
                    // Find matching closing bracket for nested array
                    int bracket_count = 0;
                    size_t array_end = array_start;
                    for (size_t i = array_start; i < json_content.length(); i++) {
                        if (json_content[i] == '[') bracket_count++;
                        if (json_content[i] == ']') bracket_count--;
                        if (bracket_count == 0) {
                            array_end = i;
                            break;
                        }
                    }
                    
                    if (array_end > array_start) {
                        std::string vertices_str = json_content.substr(
                            array_start + 1, array_end - array_start - 1);
                        
                        // Parse each vertex [lat, lon, alt]
                        size_t pos = 0;
                        while (pos < vertices_str.length()) {
                            size_t bracket_start = vertices_str.find('[', pos);
                            if (bracket_start == std::string::npos) break;
                            
                            // Find matching closing bracket
                            int count = 0;
                            size_t bracket_end = bracket_start;
                            for (size_t i = bracket_start; i < vertices_str.length(); i++) {
                                if (vertices_str[i] == '[') count++;
                                if (vertices_str[i] == ']') count--;
                                if (count == 0) {
                                    bracket_end = i;
                                    break;
                                }
                            }
                            
                            if (bracket_end == bracket_start) break;
                            
                            std::string vertex_str = vertices_str.substr(
                                bracket_start + 1, bracket_end - bracket_start - 1);
                            
                            GeoPoint point;
                            if (parseGeoPoint("[" + vertex_str + "]", point)) {
                                profile.search_area.vertices.push_back(point);
                            }
                            
                            pos = bracket_end + 1;
                        }
                    }
                }
            }
            
            // Parse min/max altitude
            size_t min_alt_pos = json_content.find("\"min_altitude_m\"", search_area_pos);
            if (min_alt_pos != std::string::npos) {
                size_t colon = json_content.find(':', min_alt_pos);
                if (colon != std::string::npos) {
                    size_t end = json_content.find_first_of(",}", colon + 1);
                    profile.search_area.min_altitude_m =
                        std::stod(json_content.substr(colon + 1, end - colon - 1));
                }
            }

            
            size_t max_alt_pos = json_content.find("\"max_altitude_m\"", search_area_pos);
            if (max_alt_pos != std::string::npos) {
                size_t colon = json_content.find(':', max_alt_pos);
                if (colon != std::string::npos) {
                    size_t end = json_content.find_first_of(",}", colon + 1);
                    profile.search_area.max_altitude_m =
                        std::stod(json_content.substr(colon + 1, end - colon - 1));
                }
            }

        }
        
        // Parse waypoints
        size_t waypoints_pos = json_content.find("\"waypoints\"");
        if (waypoints_pos != std::string::npos) {
            size_t array_start = json_content.find('[', waypoints_pos);
            if (array_start != std::string::npos) {
                // Find matching closing bracket for nested array
                int bracket_count = 0;
                size_t array_end = array_start;
                for (size_t i = array_start; i < json_content.length(); i++) {
                    if (json_content[i] == '[') bracket_count++;
                    if (json_content[i] == ']') bracket_count--;
                    if (bracket_count == 0) {
                        array_end = i;
                        break;
                    }
                }
                
                if (array_end > array_start) {
                    std::string waypoints_str = json_content.substr(
                        array_start + 1, array_end - array_start - 1);
                    
                    size_t pos = 0;
                    while (pos < waypoints_str.length()) {
                        size_t bracket_start = waypoints_str.find('[', pos);
                        if (bracket_start == std::string::npos) break;
                        
                        // Find matching closing bracket
                        int count = 0;
                        size_t bracket_end = bracket_start;
                        for (size_t i = bracket_start; i < waypoints_str.length(); i++) {
                            if (waypoints_str[i] == '[') count++;
                            if (waypoints_str[i] == ']') count--;
                            if (count == 0) {
                                bracket_end = i;
                                break;
                            }
                        }
                        
                        if (bracket_end == bracket_start) break;
                        
                        std::string waypoint_str = waypoints_str.substr(
                            bracket_start + 1, bracket_end - bracket_start - 1);
                        
                        GeoPoint point;
                        if (parseGeoPoint("[" + waypoint_str + "]", point)) {
                            profile.waypoints.push_back(point);
                        }
                        
                        pos = bracket_end + 1;
                    }
                }
            }
        }
        
        // Parse payload_policy
        size_t payload_pos = json_content.find("\"payload_policy\"");
        if (payload_pos != std::string::npos) {
            size_t type_pos = json_content.find("\"type\"", payload_pos);
            if (type_pos != std::string::npos) {
                size_t colon = json_content.find(':', type_pos);
                size_t quote_start = json_content.find('"', colon);
                if (quote_start != std::string::npos) {
                    size_t quote_end = json_content.find('"', quote_start + 1);
                    if (quote_end != std::string::npos) {
                        std::string type_str = json_content.substr(
                            quote_start + 1, quote_end - quote_start - 1);
                        profile.payload_policy.type = parsePayloadType(type_str);
                    }
                }
            }
            
            size_t arm_pos = json_content.find("\"arm_on_engagement\"", payload_pos);
            if (arm_pos != std::string::npos) {
                size_t colon = json_content.find(':', arm_pos);
                std::string value = json_content.substr(colon + 1);
                // Remove whitespace
                value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                profile.payload_policy.arm_on_engagement = 
                    (value.find("true") != std::string::npos);
            }
        }
        
        // Parse failsafe_rules
        size_t failsafe_pos = json_content.find("\"failsafe_rules\"");
        if (failsafe_pos != std::string::npos) {
            size_t comms_pos = json_content.find("\"comms_loss\"", failsafe_pos);
            if (comms_pos != std::string::npos) {
                size_t colon = json_content.find(':', comms_pos);
                size_t quote_start = json_content.find('"', colon);
                if (quote_start != std::string::npos) {
                    size_t quote_end = json_content.find('"', quote_start + 1);
                    if (quote_end != std::string::npos) {
                        std::string behavior_str = json_content.substr(
                            quote_start + 1, quote_end - quote_start - 1);
                        profile.failsafe_rules.comms_loss = parseFailsafeBehavior(behavior_str);
                    }
                }
            }
            
            size_t battery_pos = json_content.find("\"low_battery\"", failsafe_pos);
            if (battery_pos != std::string::npos) {
                size_t colon = json_content.find(':', battery_pos);
                size_t quote_start = json_content.find('"', colon);
                if (quote_start != std::string::npos) {
                    size_t quote_end = json_content.find('"', quote_start + 1);
                    if (quote_end != std::string::npos) {
                        std::string behavior_str = json_content.substr(
                            quote_start + 1, quote_end - quote_start - 1);
                        profile.failsafe_rules.low_battery = parseFailsafeBehavior(behavior_str);
                    }
                }
            }
            
            size_t gps_pos = json_content.find("\"gps_jamming\"", failsafe_pos);
            if (gps_pos != std::string::npos) {
                size_t colon = json_content.find(':', gps_pos);
                size_t quote_start = json_content.find('"', colon);
                if (quote_start != std::string::npos) {
                    size_t quote_end = json_content.find('"', quote_start + 1);
                    if (quote_end != std::string::npos) {
                        std::string behavior_str = json_content.substr(
                            quote_start + 1, quote_end - quote_start - 1);
                        profile.failsafe_rules.gps_jamming = parseFailsafeBehavior(behavior_str);
                    }
                }
            }
        }
            // -----------------------------
            // Phase 5.2 — Mission Crypto
            // -----------------------------
            size_t crypto_pos = json_content.find("\"crypto\"");
            if (crypto_pos == std::string::npos) {
                error_message = "Missing required 'crypto' section";
                return false;
            }

            // key_id
            size_t key_id_pos = json_content.find("\"key_id\"", crypto_pos);
            if (key_id_pos != std::string::npos) {
                size_t colon = json_content.find(':', key_id_pos);
                size_t q1 = json_content.find('"', colon);
                size_t q2 = json_content.find('"', q1 + 1);
                profile.crypto.key_id =
                    json_content.substr(q1 + 1, q2 - q1 - 1);
            }

            // key_epoch (optional)
            size_t epoch_pos = json_content.find("\"key_epoch\"", crypto_pos);
            if (epoch_pos != std::string::npos) {
                size_t colon = json_content.find(':', epoch_pos);
                size_t end = json_content.find_first_of(",}", colon + 1);
                profile.crypto.key_epoch =
                    static_cast<uint32_t>(
                        std::stoul(json_content.substr(colon + 1, end - colon - 1)));
            }


            // mission_key_hex
            size_t key_hex_pos = json_content.find("\"mission_key_hex\"", crypto_pos);
            if (key_hex_pos == std::string::npos) {
                error_message = "Missing 'mission_key_hex' in crypto section";
                return false;
            }

            size_t colon = json_content.find(':', key_hex_pos);
            size_t q1 = json_content.find('"', colon);
            size_t q2 = json_content.find('"', q1 + 1);
            std::string hex_key =
                json_content.substr(q1 + 1, q2 - q1 - 1);

            if (!parseHexKey(hex_key, profile.crypto.mission_key)) {
                error_message = "Invalid AES-256 mission key format";
                return false;
            }
            if (profile.crypto.key_id.empty()) {
                error_message = "crypto.key_id missing";
                return false;
            }

            
            // Validate parsed profile
            if (!validate(profile, error_message)) {
                return false;
            }
            
            return true;
        }

    bool MissionProfileParser::parseGeoPoint(
        const std::string& json_array,
        GeoPoint& point)
    {
        // Parse [lat, lon, alt]
        size_t bracket_start = json_array.find('[');
        size_t bracket_end = json_array.find(']');
        
        if (bracket_start == std::string::npos || bracket_end == std::string::npos) {
            return false;
        }
        
        std::string content = json_array.substr(bracket_start + 1, 
                                            bracket_end - bracket_start - 1);
        
        // Split by comma
        size_t comma1 = content.find(',');
        size_t comma2 = content.find(',', comma1 + 1);
        
        if (comma1 == std::string::npos || comma2 == std::string::npos) {
            return false;
        }
        
        try {
            point.lat = std::stod(content.substr(0, comma1));
            point.lon = std::stod(content.substr(comma1 + 1, comma2 - comma1 - 1));
            point.alt = std::stod(content.substr(comma2 + 1));
            return true;
        } catch (...) {
            return false;
        }
    }

    FailsafeBehavior MissionProfileParser::parseFailsafeBehavior(const std::string& str) {
        if (str == "LAND" || str == "land") return FailsafeBehavior::LAND;
        if (str == "RTL" || str == "rtl") return FailsafeBehavior::RTL;
        if (str == "HOLD" || str == "hold") return FailsafeBehavior::HOLD;
        if (str == "CONTINUE" || str == "continue") return FailsafeBehavior::CONTINUE;
        return FailsafeBehavior::RTL;  // Default
    }

    PayloadType MissionProfileParser::parsePayloadType(const std::string& str) {
        if (str == "KINETIC" || str == "kinetic") return PayloadType::KINETIC;
        if (str == "EXPLOSIVE" || str == "explosive") return PayloadType::EXPLOSIVE;
        return PayloadType::NONE;
    }
        


    bool MissionProfileParser::validate(
        const MissionProfile& profile,
        std::string& error_message)
    {
        // Validate search area
        if (!profile.search_area.isValid()) {
            error_message = "Search area must have at least 3 vertices";
            return false;
        }
        
        // Validate waypoints
        if (profile.waypoints.empty()) {
            error_message = "Mission must have at least one waypoint";
            return false;
        }
        
        // Validate altitude range
        if (profile.search_area.min_altitude_m < 0 ||
            profile.search_area.max_altitude_m < profile.search_area.min_altitude_m) {
            error_message = "Invalid altitude range in search area";
            return false;
        }
        
        // Validate waypoint altitudes are within search area range
        for (const auto& wp : profile.waypoints) {
            if (wp.alt < profile.search_area.min_altitude_m ||
                wp.alt > profile.search_area.max_altitude_m) {
                error_message = "Waypoint altitude out of search area range";
                return false;
            }
        }
        // Crypto validation
        if (!profile.crypto.isValid()) {
            error_message = "Mission crypto is missing or invalid";
            return false;
        }

        
        return true;
    }
    bool MissionProfileParser::parseHexKey(
        const std::string& hex,
        std::array<uint8_t, 32>& out)
    {
        if (hex.length() != 64)
            return false;

        for (size_t i = 0; i < 32; ++i) {
            std::string byte_str = hex.substr(i * 2, 2);
            try {
                out[i] = static_cast<uint8_t>(
                    std::stoul(byte_str, nullptr, 16));
            } catch (...) {
                return false;
            }
        }
        return true;
    }


} // namespace mission

