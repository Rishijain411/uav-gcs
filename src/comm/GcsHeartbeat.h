#pragma once

#include <netinet/in.h>

class LinkManager;

class GcsHeartbeat {
public:
    explicit GcsHeartbeat(LinkManager& link);
    void send();

private:
    LinkManager& link_;
    sockaddr_in target_addr{};
};
