#include "comm/GcsHeartbeat.h"
#include "comm/LinkManager.h"

#include <cstring>

extern "C" {
#include "mavlink/common/mavlink.h"
}

GcsHeartbeat::GcsHeartbeat(LinkManager& link)
    : link_(link)
{
    std::memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port   = htons(18570);
    target_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
}

void GcsHeartbeat::send()
{
    mavlink_message_t msg;
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];

    mavlink_msg_heartbeat_pack(
        250,
        MAV_COMP_ID_MISSIONPLANNER,
        &msg,
        MAV_TYPE_GCS,
        MAV_AUTOPILOT_GENERIC,
        0, 0,
        MAV_STATE_ACTIVE
    );

    uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);

    link_.send(
        buffer,
        len,
        reinterpret_cast<sockaddr*>(&target_addr),
        sizeof(target_addr)
    );
}
