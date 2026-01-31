#pragma once

#include <optional>
#include <iostream>
#include "utils/EnumStrings.h"
#include "command/VehicleCommand.h"
#include "mission/MissionState.h"

struct AuthorizationRequest {
    VehicleCommand command;
    mission::MissionState mission_state;
};

class OperatorAuthorization {
public:
    // Request authorization (non-blocking)
    static bool request(
        VehicleCommand cmd,
        mission::MissionState mission_state) {
        
        if (pending_) return false;  // Already pending
        
        pending_ = AuthorizationRequest{cmd, mission_state};
        decision_value_ = std::nullopt;  // Clear old decision
        
        std::cout << "\n[OITL] Authorization requested\n"
                  << "  Command: " << toString(cmd) << "\n"
                  << "  MissionState: " << toString(mission_state) << "\n"
                  << "  Type 'y' (approve) or 'n' (deny) then ENTER\n";
        
        return false;
    }
    
    // Poll for operator decision (non-blocking)
    static std::optional<bool> pollDecision() {
        if (!pending_) return std::nullopt;
        
        // If decision already polled, return cached value
        if (decision_value_.has_value()) {
            return decision_value_;
        }
        
        // Try to read one character non-blocking
        int c = readNonBlocking();
        if (c == -1) {
            return std::nullopt;  // No input yet
        }
        
        // Skip whitespace (newlines, spaces, tabs)
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
            return std::nullopt;  // Keep waiting for actual input
        }
        
        std::cout << "[DEBUG] readNonBlocking returned: " << c << " (char: '" << (char)c << "')\n";
        
        bool approved = (c == 'y' || c == 'Y');
        decision_value_ = approved;  // Cache decision
        
        std::cout << "[OITL] Decision: " 
                  << (approved ? "APPROVED" : "DENIED") << "\n";
        
        return approved;
    }
    
    // NEW: Explicitly consume the decision
    static void consumeDecision() {
        pending_.reset();
        decision_value_ = std::nullopt;
    }
    
    static bool hasPending() {
        return pending_.has_value();
    }
    
private:
    // NEW: Non-blocking single character read (implemented in .cpp)
    static int readNonBlocking();
    
    static inline std::optional<AuthorizationRequest> pending_;
    static inline std::optional<bool> decision_value_;  // NEW: Cache decision
};
