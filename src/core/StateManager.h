#pragma once

#include "SystemState.h"

class StateManager {
public:
    void setState(SystemState newState);
    SystemState getState() const;

    // ✅ NEW: explicit recovery helper
    bool isInFailsafe() const;

    // Phase 4.2: allow controlled mutable access for CommandManager
    SystemState& getMutableState() {
        return currentState;
    }

private:
    SystemState currentState = SystemState::DISCONNECTED;
};
