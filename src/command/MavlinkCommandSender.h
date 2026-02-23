#pragma once

#include <cstdint>
#include <netinet/in.h>

#include "mission/MissionProfile.h"
#include "mission/engagement/ProportionalNavigation.h"
extern "C" {
#include "mavlink/common/mavlink.h"
}
class LinkManager;

struct GeoPoint;

class MavlinkCommandSender {
public:
    MavlinkCommandSender(LinkManager& link, uint8_t target_sys);


    // ---------- High-level helpers ----------
    void sendArm();
    void sendDisarm();
    void sendTakeoff(float altitude_m);
    void sendLand();
    void sendSetModeAuto();
    void sendSetModeRTL();
    void sendSetModeLoiter();
    void sendSpeed(float speed_m_s);

    // Phase C: Search waypoint
    void sendSearchWaypoint(const GeoPoint& waypoint);

    // Mission upload (Phase B integration)
    void sendMissionUpload(const mission::MissionProfile& profile);
    void sendMissionCount(uint16_t count);
    void sendMissionItemInt(uint16_t seq, const GeoPoint& waypoint, bool isCurrent = false, bool autoContinue = true);
    void sendMissionClearAll();
    void sendMissionSetCurrent(uint16_t seq);
    void sendAccelerationSetpoint(const Vector3D& accel);
    // ---------- Generic command interface (Phase 4 / 5) ----------
    void sendRawCommand(uint16_t command) {
        sendCommand(command);
    }

private:
    // ---------- Single source of truth ----------
    void sendCommand(
        uint16_t command,
        float p1 = 0, float p2 = 0, float p3 = 0,
        float p4 = 0, float p5 = 0, float p6 = 0,
        float p7 = 0
    );

    int sockfd;
    uint8_t target_sysid;          // PX4 SYSID
    sockaddr_in px4_addr;
    sockaddr_in target_addr{};
    LinkManager& link_;

    
};
