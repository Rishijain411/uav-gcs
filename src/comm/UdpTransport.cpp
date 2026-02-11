#include "comm/UdpTransport.h"
#include "security/SecurityManager.h"
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <iostream>

UdpTransport::UdpTransport(
    SecureChannel* secure,
    LinkType type,
    bool security_enabled)
    : secure_(secure),
      link_type_(type),
      security_enabled_(SecurityManager::enabled()) {}

bool UdpTransport::start(int port)
{
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return false;
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(port);

    if (bind(sockfd,
             reinterpret_cast<sockaddr*>(&local),
             sizeof(local)) < 0) {
        perror("bind");
        close(sockfd);
        return false;
    }

    std::cout << "[UDP] Listening on port " << port << std::endl;
    return true;
}

int UdpTransport::send(
    const uint8_t* data,
    size_t len,
    const sockaddr* dst,
    socklen_t dst_len)
{
    if (security_enabled_ && secure_) {
    return secure_->encryptAndSend(
        SecureChannel::Channel::MAVLINK,  // control/telemetry channel
        sockfd,data,len,dst,dst_len);
}


    return sendto(
        sockfd,
        data,
        len,
        0,
        dst,
        dst_len);
}

int UdpTransport::receive(uint8_t* buffer, size_t max_len)
{
    if (security_enabled_ && secure_) {
        return secure_->receiveAndDecrypt(
            SecureChannel::Channel::MAVLINK,  // control/telemetry channel
            sockfd,
            buffer,
            max_len);

    }

    return recvfrom(sockfd, buffer, max_len, 0, nullptr, nullptr);
}
