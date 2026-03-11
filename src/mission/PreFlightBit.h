#pragma once

#include "Mission.h"
#include "telemetry/TelemetryData.h"
#include <chrono>

namespace mission {

class PreFlightBit {
public:
    // Run all BIT checks
    static Mission::BitStatus runChecks(
        const TelemetryData& telemetry);
    
    // Individual checks
    static bool checkMotors(const TelemetryData& telemetry);
    static bool checkBattery(const TelemetryData& telemetry);
    static bool checkMavlink(const TelemetryData& telemetry);
    static bool checkPayload(const TelemetryData& telemetry);  // Stub for now

private:
    // Motor check simulation (PRD: Motors/ESCs)
    // In real system, this would query motor status
    static bool simulateMotorCheck();
    
    // Payload continuity check (PRD: Payload continuity)
    // In real system, this would check payload circuit
    static bool simulatePayloadCheck();
};

} // namespace mission

