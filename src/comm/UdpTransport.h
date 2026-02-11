#pragma once

#include <cstddef>
#include <cstdint>
#include <netinet/in.h>

#include "security/SecureChannel.h"

enum class LinkType { 
    RF, 
    LTE };

class UdpTransport {
public:
    UdpTransport(
        SecureChannel* secure,
        LinkType type,
        bool security_enabled);

    bool start(int port);

    int send(
        const uint8_t* data,
        size_t len,
        const sockaddr* dst,
        socklen_t dst_len);

    int receive(uint8_t* buffer, size_t max_len);

private:
    int sockfd = -1;
    SecureChannel* secure_;
    LinkType link_type_;
    bool security_enabled_;
};
