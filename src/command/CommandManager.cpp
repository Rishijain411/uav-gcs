#include "command/CommandManager.h"

bool CommandManager::isCommandAllowed(VehicleCommand cmd, SystemState state) const {
    switch (cmd) {
        case VehicleCommand::ARM:
            return state == SystemState::CONNECTED;

        case VehicleCommand::DISARM:
            return true;

        default:
            return false;
    }
}
