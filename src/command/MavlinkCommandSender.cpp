#include "command/MavlinkCommandSender.h"
#include "comm/LinkManager.h"
#include "mission/search/SearchPattern.h"

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
    LinkManager& link,
    uint8_t target_sys)
    : link_(link),
      target_sysid(target_sys) {

    memset(&px4_addr, 0, sizeof(px4_addr));
    px4_addr.sin_family = AF_INET;
    px4_addr.sin_port   = htons(18570);
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

    ssize_t sent = link_.send(
    buffer,
    len,
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
    // param1=1 to arm
    // param2=21196 to force-arm (override preflight checks) needed for SITL
#ifdef SITL_MODE
    std::cout << "[DEBUG] Sending ARM with force-arm param2=21196 (SITL_MODE)\n";
    sendCommand(MAV_CMD_COMPONENT_ARM_DISARM, 1.0f, 21196.0f);
#else
    std::cout << "[DEBUG] Sending ARM with normal param2=0 (PRODUCTION_MODE)\n";
    sendCommand(MAV_CMD_COMPONENT_ARM_DISARM, 1.0f, 0.0f);  // normal preflight checks
#endif
}

void MavlinkCommandSender::sendDisarm() {
    sendCommand(MAV_CMD_COMPONENT_ARM_DISARM, 0.0f);
}

void MavlinkCommandSender::sendTakeoff(float altitude_m) {
    if (altitude_m <= 0.0f)
        altitude_m = 10.0f;   // ✅ SITL-safe default

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
void MavlinkCommandSender::sendSetModeRTL() {
    sendCommand(MAV_CMD_NAV_RETURN_TO_LAUNCH);
}

void MavlinkCommandSender::sendSetModeLoiter() {
    sendCommand(
        MAV_CMD_DO_SET_MODE,
        MAV_MODE_AUTO_ARMED,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED
    );
}

// --------------------------------------------------
// Phase C: Send search waypoint via SET_POSITION_TARGET_GLOBAL_INT
// --------------------------------------------------
void MavlinkCommandSender::sendSearchWaypoint(const GeoPoint& waypoint) {
    mavlink_message_t msg;
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];

    // Convert lat/lon to int32_t (1e7 scale)
    int32_t lat_int = (int32_t)(waypoint.lat * 1e7);
    int32_t lon_int = (int32_t)(waypoint.lon * 1e7);
    float alt_m = waypoint.alt;

    // SET_POSITION_TARGET_GLOBAL_INT: immediate waypoint setpoint
    mavlink_msg_set_position_target_global_int_pack(
        GCS_SYS_ID,
        GCS_COMP_ID,
        &msg,
        0,                              // time_boot_ms
        target_sysid,                   // target_system
        MAV_COMP_ID_AUTOPILOT1,         // target_component
        MAV_FRAME_GLOBAL_RELATIVE_ALT,  // coordinate frame (relative to home alt)
        0b0000111111000111,             // type_mask: only lat/lon/alt, ignore velocity/acceleration
        lat_int,                        // lat (1e7)
        lon_int,                        // lon (1e7)
        alt_m,                          // alt (meters)
        0, 0, 0,                        // vx, vy, vz (unused)
        0, 0, 0,                        // afx, afy, afz (unused)
        0, 0                            // yaw, yaw_rate (unused)
    );

    uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
    ssize_t sent = link_.send(
        buffer,
        len,
        reinterpret_cast<sockaddr*>(&px4_addr),
        sizeof(px4_addr)
    );


    if (sent < 0) {
        perror("[GCS] sendto waypoint failed");
    } else {
        cout << "[WAYPOINT SENT] lat=" << waypoint.lat
             << " lon=" << waypoint.lon
             << " alt=" << waypoint.alt
             << " bytes=" << sent << "\n";
    }
}

// --------------------------------------------------
// Mission Upload (Phase B integration)
// --------------------------------------------------
void MavlinkCommandSender::sendMissionUpload(const mission::MissionProfile& profile) {
    if (profile.waypoints.empty()) {
        std::cerr << "[MISSION UPLOAD] No waypoints to upload\n";
        return;
    }

    const uint16_t count = static_cast<uint16_t>(profile.waypoints.size());
    sendMissionCount(count);

    for (uint16_t i = 0; i < count; ++i) {
        sendMissionItemInt(i, profile.waypoints[i], false, true);
    }

    std::cout << "[MISSION UPLOAD] Sent " << count << " waypoints (fire-and-forget)\n";
}

void MavlinkCommandSender::sendMissionCount(uint16_t count) {
    mavlink_message_t msg;
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];

    mavlink_msg_mission_count_pack(
        GCS_SYS_ID,
        GCS_COMP_ID,
        &msg,
        target_sysid,
        MAV_COMP_ID_AUTOPILOT1,
        count,
        MAV_MISSION_TYPE_MISSION,
        0
    );

    uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
    ssize_t sent = link_.send(
        buffer,
        len,
        reinterpret_cast<sockaddr*>(&px4_addr),
        sizeof(px4_addr)
    );

    if (sent < 0) {
        perror("[GCS] sendMissionCount failed");
    }
}

void MavlinkCommandSender::sendMissionItemInt(uint16_t seq, const GeoPoint& waypoint, bool isCurrent, bool autoContinue) {
    mavlink_message_t msg;
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];

    const int32_t lat_int = static_cast<int32_t>(waypoint.lat * 1e7);
    const int32_t lon_int = static_cast<int32_t>(waypoint.lon * 1e7);
    const float alt_m = static_cast<float>(waypoint.alt);

    mavlink_msg_mission_item_int_pack(
        GCS_SYS_ID,
        GCS_COMP_ID,
        &msg,
        target_sysid,
        MAV_COMP_ID_AUTOPILOT1,
        seq,
        MAV_FRAME_GLOBAL_RELATIVE_ALT,
        MAV_CMD_NAV_WAYPOINT,
        isCurrent ? 1 : 0,
        autoContinue ? 1 : 0,
        0, 0, 0, 0,
        lat_int,
        lon_int,
        alt_m,
        MAV_MISSION_TYPE_MISSION
    );

    uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
    ssize_t sent = link_.send(
        buffer,
        len,
        reinterpret_cast<sockaddr*>(&px4_addr),
        sizeof(px4_addr)
    );

    if (sent < 0) {
        perror("[GCS] sendMissionItemInt failed");
    }
}

void MavlinkCommandSender::sendMissionClearAll() {
    mavlink_message_t msg;
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];

    mavlink_msg_mission_clear_all_pack(
        GCS_SYS_ID,
        GCS_COMP_ID,
        &msg,
        target_sysid,
        MAV_COMP_ID_AUTOPILOT1,
        MAV_MISSION_TYPE_MISSION
    );

    uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
    ssize_t sent = link_.send(
        buffer,
        len,
        reinterpret_cast<sockaddr*>(&px4_addr),
        sizeof(px4_addr)
    );

    if (sent < 0) {
        perror("[GCS] sendMissionClearAll failed");
    }
}

void MavlinkCommandSender::sendMissionSetCurrent(uint16_t seq) {
    mavlink_message_t msg;
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];

    mavlink_msg_mission_set_current_pack(
        GCS_SYS_ID,
        GCS_COMP_ID,
        &msg,
        target_sysid,
        MAV_COMP_ID_AUTOPILOT1,
        seq
    );

    uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
    ssize_t sent = link_.send(
        buffer,
        len,
        reinterpret_cast<sockaddr*>(&px4_addr),
        sizeof(px4_addr)
    );

    if (sent < 0) {
        perror("[GCS] sendMissionSetCurrent failed");
    }
}
void MavlinkCommandSender::sendAccelerationSetpoint(const Vector3D& accel) {
    mavlink_message_t msg;
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];

    // MAVLink type_mask: 0b0000110001111111 
    // (Ignores Pos/Vel/Yaw, enables Accel AFX/AFY/AFZ)
    uint16_t type_mask = 0b0000110001111111;

    mavlink_msg_set_position_target_local_ned_pack(
        GCS_SYS_ID, GCS_COMP_ID, &msg,
        0, target_sysid, MAV_COMP_ID_AUTOPILOT1,
        MAV_FRAME_LOCAL_NED,
        type_mask,
        0, 0, 0,    // x, y, z (ignored)
        0, 0, 0,    // vx, vy, vz (ignored)
        static_cast<float>(accel.x), 
        static_cast<float>(accel.y), 
        static_cast<float>(accel.z),
        0, 0        // yaw, yaw_rate (ignored)
    );

    uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
    link_.send(buffer, len, reinterpret_cast<sockaddr*>(&px4_addr), sizeof(px4_addr));
}