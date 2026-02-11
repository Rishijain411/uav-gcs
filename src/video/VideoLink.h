#pragma once
#include <cstdint>
#include <cstddef>

class VideoLink {
public:
    virtual ~VideoLink() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    virtual bool sendEncodedFrame(
        const uint8_t* data,
        size_t len,
        uint64_t timestamp_us) = 0;
};
