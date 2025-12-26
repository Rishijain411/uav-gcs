#include <iostream>
#include "comm/UdpTransport.h"
#include "telemetry/TelemetryParser.h"
#include "telemetry/TelemetryData.h"
#include "core/StateManager.h"
#include <chrono>
#include "command/CommandManager.h"

constexpr int HEARTBEAT_TIMEOUT_MS = 2000;


int main() {

    UdpTransport udp;
    TelemetryData telemetry;
    StateManager stateManager;
    CommandManager commandManager;
    TelemetryParser parser(telemetry, stateManager);

    if (!udp.start(14550)) {
        return -1;
    }

    uint8_t buffer[2048];

    while (true) {
    int len = udp.receive(buffer, sizeof(buffer));
    if (len > 0) {
        for (int i = 0; i < len; i++) {
            parser.parse(buffer[i]);
        }
    }

        if (telemetry.heartbeat_received &&
        stateManager.getState() == SystemState::CONNECTED) 
        {

            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - telemetry.last_heartbeat_time
            ).count();

            if (elapsed_ms > HEARTBEAT_TIMEOUT_MS) {
                stateManager.setState(SystemState::FAILSAFE);
                std::cout << "[FAILSAFE] Heartbeat timeout detected" << std::endl;
                }
        }
        //phase 2 added lines below
        static bool arm_attempted = false;

        if (!arm_attempted && telemetry.heartbeat_received) {
            arm_attempted = true;

            if (commandManager.isCommandAllowed(
                    VehicleCommand::ARM,
                    stateManager.getState())) {

                std::cout << "[COMMAND] ARM allowed" << std::endl;
                // Later: send MAVLink ARM command
            } else {
                std::cout << "[COMMAND BLOCKED] ARM rejected due to state"
                        << std::endl;
            }
        }
    }
    


    return 0;
}
