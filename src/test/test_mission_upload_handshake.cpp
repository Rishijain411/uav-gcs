#include "telemetry/TelemetryParser.h"
#include "telemetry/TelemetryData.h"
#include "core/StateManager.h"

#include <cassert>

extern "C" {
#include "mavlink/common/mavlink.h"
}

static void feedMessage(TelemetryParser& parser, mavlink_message_t& msg) {
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    const uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
    for (uint16_t i = 0; i < len; ++i) {
        parser.parse(buffer[i]);
    }
}

int main() {
    TelemetryData telemetry;
    StateManager stateManager;
    TelemetryParser parser(telemetry, stateManager);

    mavlink_message_t msg;

    mavlink_msg_mission_request_int_pack(
        1, 1, &msg,
        1, MAV_COMP_ID_AUTOPILOT1,
        5, MAV_MISSION_TYPE_MISSION
    );
    feedMessage(parser, msg);
    assert(telemetry.mission_request_received);
    assert(telemetry.last_mission_request_seq == 5);

    mavlink_msg_mission_ack_pack(
        1, 1, &msg,
        1, MAV_COMP_ID_AUTOPILOT1,
        MAV_MISSION_ACCEPTED, MAV_MISSION_TYPE_MISSION, 0
    );
    feedMessage(parser, msg);
    assert(telemetry.last_mission_ack.valid);
    assert(telemetry.last_mission_ack.type == MAV_MISSION_ACCEPTED);

    return 0;
}