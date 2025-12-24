#include "UdpTransport.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

bool UdpTransport::start(int port) {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return false;
    }

    std::cout << "[UDP] Listening on port " << port << std::endl;
    return true;
}

int UdpTransport::receive(uint8_t* buffer, int buffer_len) {
    return recv(sockfd, buffer, buffer_len, 0);
}
