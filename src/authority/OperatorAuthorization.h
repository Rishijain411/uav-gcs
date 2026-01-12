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
        mission::MissionState mission_state)
    {
        if (pending_) return false;

        pending_ = AuthorizationRequest{cmd, mission_state};

        cout << "\n[OITL] Authorization requested\n"
             << "  Command: " << toString(cmd) << "\n"
             << "  MissionState: " << toString(mission_state) << "\n"
             << "Type 'y' or 'n' then ENTER\n";

        return false;
    }

    // Poll operator decision (non-blocking)
    static std::optional<bool> pollDecision() {
        if (!pending_) return std::nullopt;

        if (!std::cin.rdbuf()->in_avail())
            return std::nullopt;

        char c;
        std::cin >> c;

        bool approved = (c == 'y' || c == 'Y');
        pending_.reset();
        return approved;
    }

    static bool hasPending() {
        return pending_.has_value();
    }

private:
    static inline std::optional<AuthorizationRequest> pending_;
};
