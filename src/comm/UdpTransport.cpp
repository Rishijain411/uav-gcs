#include "comm/UdpTransport.h"
#include "security/SecurityManager.h"
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <sys/socket.h>
#include <errno.h>

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

    // Set socket to non-blocking mode to prevent hanging
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl");
        close(sockfd);
        return false;
    }

    // Set receive timeout (500ms) to bail out if stuck
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;  // 500ms
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt SO_RCVTIMEO");
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

    std::cout << "[UDP] Listening on port " << port << " (non-blocking)\n";
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
        int ret = secure_->receiveAndDecrypt(
            SecureChannel::Channel::MAVLINK,  // control/telemetry channel
            sockfd,
            buffer,
            max_len);
        return ret;  // May return -1 on timeout or no data
    }

    int ret = recvfrom(sockfd, buffer, max_len, 0, nullptr, nullptr);
    // EAGAIN/EWOULDBLOCK/EINTR are normal for non-blocking/timeout sockets
    if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        // Real error - uncomment for debugging: perror("recvfrom");
    }
    return ret;
}
