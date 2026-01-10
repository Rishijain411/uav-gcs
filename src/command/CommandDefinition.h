#pragma once
#include "VehicleCommand.h"
#include "telemetry/TelemetryData.h"

struct CommandDefinition {
    VehicleCommand logical;
    uint16_t mavlink_id;

    bool (*allowed)(const TelemetryData&);
};
