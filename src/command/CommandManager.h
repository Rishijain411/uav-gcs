#pragma once

#include "core/SystemState.h"

enum class VehicleCommand {
    ARM,
    DISARM
};

class CommandManager {
public:
    bool isCommandAllowed(VehicleCommand cmd, SystemState state) const;
};
