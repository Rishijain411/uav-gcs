#pragma once
#include "TelemetryData.h"
#include "core/StateManager.h"

class TelemetryParser {
public:
    TelemetryParser(TelemetryData& data, StateManager& stateMgr);
    void parse(uint8_t byte);

private:
    TelemetryData& telemetry;
    StateManager& stateManager;
};
