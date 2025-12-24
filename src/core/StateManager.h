#pragma once
#include "SystemState.h"

class StateManager {
public:
    void setState(SystemState newState);
    SystemState getState() const;

private:
    SystemState currentState = SystemState::DISCONNECTED;
};
