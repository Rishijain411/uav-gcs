#include "CommandDefinition.h"
#include "CommandRules.h"

static const CommandDefinition COMMAND_TABLE[] = {
    { VehicleCommand::ARM,        MAV_CMD_COMPONENT_ARM_DISARM, canArm },
    { VehicleCommand::DISARM,     MAV_CMD_COMPONENT_ARM_DISARM, canDisarm },
    { VehicleCommand::SET_MODE_AUTO, MAV_CMD_DO_SET_MODE,       canSetAuto },
    { VehicleCommand::TAKEOFF,    MAV_CMD_NAV_TAKEOFF,          canTakeoff },
    { VehicleCommand::LAND,       MAV_CMD_NAV_LAND,             canLand }
};

inline const CommandDefinition* findCommand(VehicleCommand cmd) {
    for (const auto& c : COMMAND_TABLE)
        if (c.logical == cmd)
            return &c;
    return nullptr;
}
