#pragma once

#include <cstdint>
#include <netinet/in.h>

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


    // Phase C: Search waypoint
    void sendSearchWaypoint(const GeoPoint& waypoint);

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
