#include "comm/UdpTransport.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

bool UdpTransport::start(int port) {

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return false;
    }

    sockaddr_in local_addr{};
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(port);

    if (bind(sockfd,
             reinterpret_cast<sockaddr*>(&local_addr),
             sizeof(local_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return false;
    }

    std::cout << "[UDP] Listening on port " << port << std::endl;
    return true;
}

int UdpTransport::receive(uint8_t* buffer, size_t len) {
    return recvfrom(sockfd, buffer, len, 0, nullptr, nullptr);
}

int UdpTransport::getSocketFd() const {
    return sockfd;
}
