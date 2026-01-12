#pragma once

enum class VehicleCommand {
    NONE,          //  sentinel for audit / non-command events
    ARM,
    DISARM,
    SET_MODE_AUTO,
    TAKEOFF,
    LAND
};
