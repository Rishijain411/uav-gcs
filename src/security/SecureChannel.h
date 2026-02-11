#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <netinet/in.h>

class SecureChannel {
public:
    static constexpr size_t KEY_SIZE = 32;   // AES-256
    static constexpr size_t IV_SIZE  = 12;   // GCM standard
    static constexpr size_t TAG_SIZE = 16;

    // Distinct logical channels (nonce separation)
    enum class Channel {
        MAVLINK = 0,
        AI_METADATA = 1
        // VIDEO intentionally excluded (SRTP, separate plane)
    };

    explicit SecureChannel(const std::array<uint8_t, KEY_SIZE>& mission_key);

    // ---- NEW (per-boot session key support) ----
    void setSessionKey(
        Channel channel,
        const std::array<uint8_t, KEY_SIZE>& key);

    // ---- Encrypt / Decrypt ----
    int encryptAndSend(
        Channel channel,
        int sockfd,
        const uint8_t* plaintext,
        size_t len,
        const sockaddr* dst,
        socklen_t dst_len);

    int receiveAndDecrypt(
        Channel channel,
        int sockfd,
        uint8_t* out,
        size_t max_len);

private:
    // ---- Root / Session Keys ----
    std::array<uint8_t, KEY_SIZE> root_key_;
    std::array<uint8_t, KEY_SIZE> session_keys_[2];

    // ---- Replay Protection (per-channel) ----
    uint64_t tx_counter_[2] = {0, 0};
    uint64_t rx_counter_[2] = {0, 0};

    // ---- Helpers ----
    void deriveIV(uint64_t counter, uint8_t iv[IV_SIZE]) const;
};
