#pragma once

#include <chrono>
#include <cstdint>
#include <netinet/in.h>

#include "comm/UdpTransport.h"

class LinkManager {
public:
    LinkManager(UdpTransport& primary, UdpTransport& secondary);

    int send(const uint8_t* data,size_t len,const sockaddr* dst,socklen_t dst_len);
    int receive(uint8_t* buffer, size_t max_len);

private:
    void checkFailover();
    UdpTransport& primary_;
    UdpTransport& secondary_;
    UdpTransport* active_;

    std::chrono::steady_clock::time_point last_rx_;
    static constexpr int FAILOVER_TIMEOUT_MS = 1500;

    
};

