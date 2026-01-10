#include "comm/GcsHeartbeat.h"

#include <cstring>
#include <unistd.h>

extern "C" {
#include "mavlink/common/mavlink.h"
}

static constexpr uint8_t GCS_SYS_ID  = 50;
static constexpr uint8_t GCS_COMP_ID = MAV_COMP_ID_MISSIONPLANNER;

GcsHeartbeat::GcsHeartbeat(int socket_fd)
    : sockfd(socket_fd) {

    std::memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;

    // ✅ PX4 MAVLink *listening* port
    target_addr.sin_port = htons(18570);

    target_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
}

void GcsHeartbeat::send() {

    mavlink_message_t msg;
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];

    mavlink_msg_heartbeat_pack(
        GCS_SYS_ID,
        GCS_COMP_ID,
        &msg,
        MAV_TYPE_GCS,
        MAV_AUTOPILOT_INVALID,
        0,
        0,
        MAV_STATE_ACTIVE
    );

    uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);

    sendto(
        sockfd,
        buffer,
        len,
        0,
        reinterpret_cast<sockaddr*>(&target_addr),
        sizeof(target_addr)
    );
}
