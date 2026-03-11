#pragma once

#include "video/VideoLink.h"

class NullVideoLink : public VideoLink {
public:
    bool start() override;
    void stop() override;
    bool sendEncodedFrame(
        const uint8_t* data,
        size_t len,
        uint64_t timestamp_us) override;
};
