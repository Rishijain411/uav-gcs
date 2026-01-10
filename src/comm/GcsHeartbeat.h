#pragma once

#include <cstdint>
#include <netinet/in.h>

class GcsHeartbeat {
public:
    explicit GcsHeartbeat(int socket_fd);

    void send();

private:
    int sockfd;
    sockaddr_in target_addr;
};
