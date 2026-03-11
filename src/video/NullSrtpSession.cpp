#include "video/SrtpSession.h"

class NullSrtpSession : public SrtpSession {
public:
    bool initialize(
        const std::vector<uint8_t>&,
        const std::vector<uint8_t>&) override {
        return true;
    }

    bool protect(uint8_t*, size_t&, size_t) override {
        return true; // plaintext passthrough
    }

    bool unprotect(uint8_t*, size_t&) override {
        return true;
    }
};
