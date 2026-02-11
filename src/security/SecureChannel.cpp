#include "security/SecureChannel.h"

#include <openssl/evp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

// --------------------------------------------------
// Constructor
// --------------------------------------------------
SecureChannel::SecureChannel(
    const std::array<uint8_t, KEY_SIZE>& mission_key)
    : root_key_(mission_key)
{
    // Default: session keys = root key (dev / no Jetson)
    session_keys_[0] = root_key_;
    session_keys_[1] = root_key_;
}

// --------------------------------------------------
// IV derivation (NIST-approved deterministic IV)
// IV = counter (64-bit BE) || zero padding
// --------------------------------------------------
void SecureChannel::deriveIV(uint64_t counter, uint8_t iv[IV_SIZE]) const {
    std::memset(iv, 0, IV_SIZE);

    uint64_t be_counter = htobe64(counter);
    std::memcpy(iv, &be_counter, sizeof(be_counter));
}

// --------------------------------------------------
// Encrypt + Send (Drop-on-failure)
// Packet layout: [IV | CIPHERTEXT | TAG]
// --------------------------------------------------
int SecureChannel::encryptAndSend(
    Channel channel,
    int sockfd,
    const uint8_t* plaintext,
    size_t len,
    const sockaddr* dst,
    socklen_t dst_len)
{
    int ch = static_cast<int>(channel);
    if (ch < 0 || ch >= 2)
        return -1;

    uint8_t iv[IV_SIZE];
    deriveIV(++tx_counter_[ch], iv);

    uint8_t outbuf[2048];
    if (len + IV_SIZE + TAG_SIZE > sizeof(outbuf))
        return -1;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return -1;

    int outlen = 0;
    int tmplen = 0;

    uint8_t aad = static_cast<uint8_t>(channel);

    bool ok =
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, nullptr) &&
        EVP_EncryptInit_ex(
            ctx, nullptr, nullptr,
            session_keys_[ch].data(), iv) &&

        // ---- AAD: bind packet to channel ----
        EVP_EncryptUpdate(ctx, nullptr, &outlen, &aad, sizeof(aad)) &&

        EVP_EncryptUpdate(
            ctx,
            outbuf + IV_SIZE,
            &outlen,
            plaintext,
            static_cast<int>(len)) &&

        EVP_EncryptFinal_ex(
            ctx,
            outbuf + IV_SIZE + outlen,
            &tmplen) &&

        EVP_CIPHER_CTX_ctrl(
            ctx,
            EVP_CTRL_GCM_GET_TAG,
            TAG_SIZE,
            outbuf + IV_SIZE + outlen);

    EVP_CIPHER_CTX_free(ctx);

    if (!ok)
        return -1;

    std::memcpy(outbuf, iv, IV_SIZE);
    size_t total_len = IV_SIZE + outlen + TAG_SIZE;

    ssize_t sent = sendto(
        sockfd,
        outbuf,
        total_len,
        0,
        dst,
        dst_len);

    return (sent == static_cast<ssize_t>(total_len)) ? sent : -1;
}

// --------------------------------------------------
// Receive + Decrypt (Replay protected, silent drop)
// --------------------------------------------------
int SecureChannel::receiveAndDecrypt(
    Channel channel,
    int sockfd,
    uint8_t* out,
    size_t max_len)
{
    int ch = static_cast<int>(channel);
    if (ch < 0 || ch >= 2)
        return -1;

    uint8_t inbuf[2048];
    ssize_t rxlen = recvfrom(sockfd, inbuf, sizeof(inbuf), 0, nullptr, nullptr);
    if (rxlen <= 0)
        return -1;

    if (rxlen < static_cast<ssize_t>(IV_SIZE + TAG_SIZE))
        return -1;

    uint8_t* iv  = inbuf;
    uint8_t* tag = inbuf + rxlen - TAG_SIZE;
    uint8_t* ct  = inbuf + IV_SIZE;
    int ct_len   = rxlen - IV_SIZE - TAG_SIZE;

    if (ct_len <= 0 || static_cast<size_t>(ct_len) > max_len)
        return -1;

    // ---- Replay protection ----
    uint64_t rx_counter;
    std::memcpy(&rx_counter, iv, sizeof(rx_counter));
    rx_counter = be64toh(rx_counter);

    if (rx_counter <= rx_counter_[ch])
        return -1;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return -1;

    int outlen = 0;
    int tmplen = 0;

    uint8_t aad = static_cast<uint8_t>(channel);

    bool ok =
        EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, nullptr) &&
        EVP_DecryptInit_ex(
            ctx, nullptr, nullptr,
            session_keys_[ch].data(), iv) &&

        // ---- AAD: bind packet to channel ----
        EVP_DecryptUpdate(ctx, nullptr, &outlen, &aad, sizeof(aad)) &&

        EVP_DecryptUpdate(ctx, out, &outlen, ct, ct_len) &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE, tag) &&
        EVP_DecryptFinal_ex(ctx, out + outlen, &tmplen);

    EVP_CIPHER_CTX_free(ctx);

    if (!ok)
        return -1;

    rx_counter_[ch] = rx_counter;
    return outlen;
}

// --------------------------------------------------
// Set per-channel session key (future X25519 / SE)
// --------------------------------------------------
void SecureChannel::setSessionKey(
    Channel channel,
    const std::array<uint8_t, KEY_SIZE>& key)
{
    int ch = static_cast<int>(channel);
    if (ch < 0 || ch >= 2)
        return;

    session_keys_[ch] = key;
}
