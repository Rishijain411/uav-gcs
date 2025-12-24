#include "TelemetryParser.h"
#include <iostream>

extern "C" {
#include "mavlink/common/mavlink.h"
}

static mavlink_message_t msg;
static mavlink_status_t status;

TelemetryParser::TelemetryParser(TelemetryData& data, StateManager& stateMgr)
    : telemetry(data), stateManager(stateMgr) {}

void TelemetryParser::parse(uint8_t byte) {
    if (mavlink_parse_char(MAVLINK_COMM_0, byte, &msg, &status)) {

        if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
            telemetry.system_id = msg.sysid;
            telemetry.component_id = msg.compid;
            telemetry.heartbeat_received = true;

            if (stateManager.getState() == SystemState::DISCONNECTED) {
                stateManager.setState(SystemState::CONNECTED);
            }

            std::cout << "[HEARTBEAT] Vehicle detected (SysID "
                      << static_cast<int>(msg.sysid) << ")" << std::endl;
        }
    }
}
