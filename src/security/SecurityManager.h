#pragma once
#include "security/SecurityConfig.h"
#include "security/SecurityMode.h"

class SecurityManager {
public:
    static SecurityMode mode() {
#if SECURE_CHANNEL_ENABLED
        return SecurityMode::ENCRYPTED;
#else
        return SecurityMode::PLAINTEXT;
#endif
    }

    static bool enabled() {
        return mode() == SecurityMode::ENCRYPTED;
    }
};
