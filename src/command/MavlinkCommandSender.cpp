#include "command/MavlinkCommandSender.h"

#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <iostream>

using namespace std;

// --------------------------------------------------
// GCS identity (MUST be stable & non-255)
// --------------------------------------------------
static constexpr uint8_t GCS_SYS_ID  = 250;  // QGC-style valid GCS ID
static constexpr uint8_t GCS_COMP_ID = MAV_COMP_ID_MISSIONPLANNER;

// --------------------------------------------------
// Constructor
// --------------------------------------------------
MavlinkCommandSender::MavlinkCommandSender(
    int socket_fd,
    uint8_t target_sys)
    : sockfd(socket_fd),
      target_sysid(target_sys) {

    memset(&px4_addr, 0, sizeof(px4_addr));
    px4_addr.sin_family = AF_INET;

    // ✅ PX4 COMMAND INPUT PORT (SITL)
    px4_addr.sin_port = htons(18570);

    inet_pton(AF_INET, "127.0.0.1", &px4_addr.sin_addr);
}

// --------------------------------------------------
// CORE: Generic COMMAND_LONG sender (PX4-correct)
// --------------------------------------------------
void MavlinkCommandSender::sendCommand(
    uint16_t command,
    float p1, float p2, float p3,
    float p4, float p5, float p6,
    float p7) {

    mavlink_message_t msg;
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];

    mavlink_msg_command_long_pack(
        GCS_SYS_ID,                 // ✅ VALID GCS SYSID (NOT 255)
        GCS_COMP_ID,                // ✅ GCS component
        &msg,
        target_sysid,               // vehicle sysid
        MAV_COMP_ID_AUTOPILOT1,     // ✅ FORCE autopilot
        command,
        1,                           // ✅ confirmation REQUIRED
        p1, p2, p3, p4, p5, p6, p7
    );

    uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);

    ssize_t sent = sendto(
        sockfd,
        buffer,
        len,
        0,
        reinterpret_cast<sockaddr*>(&px4_addr),
        sizeof(px4_addr)
    );

    if (sent < 0) {
        perror("[GCS] send to failed");
    }
}

// --------------------------------------------------
// High-level helpers (NO CHANGE)
// --------------------------------------------------
void MavlinkCommandSender::sendArm() {
    sendCommand(MAV_CMD_COMPONENT_ARM_DISARM, 1.0f);
}

void MavlinkCommandSender::sendDisarm() {
    sendCommand(MAV_CMD_COMPONENT_ARM_DISARM, 0.0f);
}

void MavlinkCommandSender::sendTakeoff(float altitude_m) {
    sendCommand(
        MAV_CMD_NAV_TAKEOFF,
        0, 0, 0, 0, 0, 0,
        altitude_m
    );
}

void MavlinkCommandSender::sendLand() {
    sendCommand(MAV_CMD_NAV_LAND);
}

void MavlinkCommandSender::sendSetModeAuto() {
    sendCommand(
        MAV_CMD_DO_SET_MODE,
        MAV_MODE_AUTO_ARMED,
        0
    );
}
