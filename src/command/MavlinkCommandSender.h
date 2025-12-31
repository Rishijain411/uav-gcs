#pragma once
#include "mavlink/common/mavlink.h"


#include <cstdint>
#include <netinet/in.h>

class MavlinkCommandSender {
public:
    MavlinkCommandSender(int socket_fd,
                         uint8_t target_sys,
                         uint8_t target_comp);

    void sendArm();
    void sendDisarm();
    void sendTakeoff(float altitude_m);
    void sendLand();
    void setMode(uint32_t custom_mode);
    // Phase 4.2: generic command interface (ACK + retry support)
    void sendRawCommand(uint16_t command) {
        sendCommandLong(command);
    }


private:
    void sendCommandLong(uint16_t command,
                         float p1 = 0, float p2 = 0, float p3 = 0,
                         float p4 = 0, float p5 = 0, float p6 = 0,
                         float p7 = 0);

    int sockfd;
    uint8_t target_sysid;
    uint8_t target_compid;
    sockaddr_in px4_addr;
};
