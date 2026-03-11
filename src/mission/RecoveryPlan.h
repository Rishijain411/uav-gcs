#pragma once

#include "command/VehicleCommand.h"


namespace mission {

enum class RecoveryMode {
    RTL,
    LAND,
    HOLD
};

enum class RecoverySource {
    GCS_DYNAMIC,
    MISSION_DEFAULT,
    ONBOARD_FAILSAFE
};

struct RecoveryPlan {
    RecoveryMode mode;
    RecoverySource source;
    VehicleCommand command;
};

} // namespace mission
