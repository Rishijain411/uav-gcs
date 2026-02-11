#include "comm/LinkManager.h"
#include "authority/AuditLogger.h"

#include <iostream>

using steady_clock_t = std::chrono::steady_clock;

LinkManager::LinkManager(
    UdpTransport& primary,
    UdpTransport& secondary)
    : primary_(primary),
      secondary_(secondary),
      active_(&primary),
      last_rx_(steady_clock_t::time_point::min()) {}

int LinkManager::send(
    const uint8_t* data,
    size_t len,
    const sockaddr* dst,
    socklen_t dst_len)
{
    return active_->send(data, len, dst, dst_len);
}

int LinkManager::receive(uint8_t* buffer, size_t max_len)
{
    int ret = active_->receive(buffer, max_len);
    if (ret > 0) {
        last_rx_ = steady_clock_t::now();
        return ret;
    }

    checkFailover();
    return -1;
}

void LinkManager::checkFailover()
{
    // Do not failover before first RX
    if (last_rx_ == steady_clock_t::time_point::min())
        return;

    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            steady_clock_t::now() - last_rx_).count();

    if (elapsed > FAILOVER_TIMEOUT_MS && active_ == &primary_) {
        active_ = &secondary_;

        AuditLogger::logDecision(
            VehicleCommand::NONE,
            mission::MissionState::INIT,
            "LINK_FAILOVER",
            "Primary RF link lost, switched to LTE");

        std::cerr << "[LINK] Failover: RF → LTE\n";
    }
}
