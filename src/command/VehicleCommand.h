#pragma once

enum class VehicleCommand {
    NONE,          //  sentinel for audit / non-command events
    ARM,
    DISARM,
    SET_MODE_AUTO,
    SET_MODE_RTL,      //phase 4 req
    SET_MODE_LOITER,   //phase 4 req
    TAKEOFF,
    LAND,
    SET_HOME   //prd phase 4 req
};
