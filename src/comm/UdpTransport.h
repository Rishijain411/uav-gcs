#pragma once
#include <cstdint>

class UdpTransport {
public:
    bool start(int port);
    int receive(uint8_t* buffer, int buffer_len);

private:
    int sockfd = -1;
};
