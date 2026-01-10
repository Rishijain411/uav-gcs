#pragma once
#include <cstddef>
#include <cstdint>

class UdpTransport {
public:
    bool start(int port);
    int receive(uint8_t* buffer, size_t len);
    int getSocketFd() const;

private:
    int sockfd = -1;
};
