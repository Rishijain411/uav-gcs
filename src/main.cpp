#include <iostream>
#include "comm/UdpTransport.h"
#include "telemetry/TelemetryParser.h"
#include "telemetry/TelemetryData.h"
#include "core/StateManager.h"

int main() {

    UdpTransport udp;
    TelemetryData telemetry;
    StateManager stateManager;
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
    }

    return 0;
}
