#pragma once

#include "MissionState.h"
#include "MissionEvent.h"

namespace mission {

class Mission {
public:
    Mission();

    MissionState state() const;

    // Core state transition API
    bool apply_event(MissionEvent event);

private:
    MissionState current_state_;

    MissionState transition(MissionState state, MissionEvent event);
};

} // namespace mission
