#pragma once
#include <cstdint>
#include <vector>

class SrtpSession {
public:
    virtual ~SrtpSession() = default;

    // Called once per boot
    virtual bool initialize(
        const std::vector<uint8_t>& master_key,
        const std::vector<uint8_t>& master_salt) = 0;

    // Encrypt + authenticate one RTP packet
    virtual bool protect(
        uint8_t* packet,
        size_t& len,
        size_t max_len) = 0;

    // Decrypt + verify one RTP packet
    virtual bool unprotect(
        uint8_t* packet,
        size_t& len) = 0;
};
