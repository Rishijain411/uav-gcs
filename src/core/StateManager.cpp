#include "StateManager.h"
#include <iostream>

void StateManager::setState(SystemState newState) {
    if (currentState != newState) {
        currentState = newState;
        std::cout << "[STATE] Changed to "
                  << static_cast<int>(newState) << std::endl;
    }
}

SystemState StateManager::getState() const {
    return currentState;
}

// ✅ NEW: FAILSAFE query helper
bool StateManager::isInFailsafe() const {
    return currentState == SystemState::FAILSAFE;
}
