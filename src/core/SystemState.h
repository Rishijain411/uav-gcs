#pragma once

enum class SystemState {
    DISCONNECTED = 0,
    CONNECTED    = 1,
    ARMED        = 2,
    IN_AIR       = 3,
    FAILSAFE     = 4
};
