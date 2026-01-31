#include "PreFlightBit.h"
#include "telemetry/TelemetryData.h"
#include <iostream>
#include <chrono>

namespace mission {

Mission::BitStatus PreFlightBit::runChecks(
    const TelemetryData& telemetry)
{
    Mission::BitStatus status;
    
    status.motors_ok = checkMotors(telemetry);
    status.battery_ok = checkBattery(telemetry);
    status.mavlink_ok = checkMavlink(telemetry);
    status.payload_ok = checkPayload(telemetry);
    
    return status;
}

bool PreFlightBit::checkMotors(const TelemetryData& telemetry) {
    // PRD: Motors/ESCs check
    // For now, simulate check (always pass if vehicle is connected)
    // In real system, would query motor status via MAVLink
    
    if (!telemetry.heartbeat_received) {
        std::cout << "[BIT] Motors: FAIL (no vehicle connection)\n";
        return false;
    }
    
    // Simulated check - in production, query actual motor status
    bool motors_ok = simulateMotorCheck();
    
    if (motors_ok) {
        std::cout << "[BIT] Motors: PASS\n";
    } else {
        std::cout << "[BIT] Motors: FAIL\n";
    }
    
    return motors_ok;
}

bool PreFlightBit::checkBattery(const TelemetryData& telemetry) {
    // PRD: Battery Health check
    if (!telemetry.battery_received) {
        std::cout << "[BIT] Battery: FAIL (no battery data)\n";
        return false;
    }
    
    if (!telemetry.battery_ok) {
        std::cout << "[BIT] Battery: FAIL (low battery)\n";
        return false;
    }
    
    std::cout << "[BIT] Battery: PASS\n";
    return true;
}

bool PreFlightBit::checkMavlink(const TelemetryData& telemetry) {
    // PRD: MAVLink Heartbeat check
    if (!telemetry.heartbeat_received) {
        std::cout << "[BIT] MAVLink: FAIL (no heartbeat)\n";
        return false;
    }
    
    // Check heartbeat freshness (within last 2 seconds)
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - telemetry.last_heartbeat_time).count();
    
    if (elapsed > 2000) {
        std::cout << "[BIT] MAVLink: FAIL (heartbeat timeout)\n";
        return false;
    }
    
    std::cout << "[BIT] MAVLink: PASS\n";
    return true;
}

bool PreFlightBit::checkPayload(const TelemetryData& telemetry) {
    // PRD: Payload continuity (stub)
    // In real system, would check payload circuit continuity
    
    if (!telemetry.heartbeat_received) {
        std::cout << "[BIT] Payload: FAIL (no vehicle connection)\n";
        return false;
    }
    
    // Simulated check - in production, query actual payload status
    bool payload_ok = simulatePayloadCheck();
    
    if (payload_ok) {
        std::cout << "[BIT] Payload: PASS\n";
    } else {
        std::cout << "[BIT] Payload: FAIL\n";
    }
    
    return payload_ok;
}

bool PreFlightBit::simulateMotorCheck() {
    // Simulated motor check - always pass for now
    // In production, would query motor status via MAVLink
    // Could check: motor temperature, ESC status, rotation speed, etc.
    return true;
}

bool PreFlightBit::simulatePayloadCheck() {
    // Simulated payload continuity check - always pass for now
    // In production, would check payload circuit continuity
    // Could check: payload voltage, continuity, arming status, etc.
    return true;
}

} // namespace mission

