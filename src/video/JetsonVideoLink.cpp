#include "video/JetsonVideoLink.h"

#include <iostream>
#include <atomic>
#include <thread>

// NOTE:
// This implementation is INTENTIONALLY minimal.
// It represents a REAL Jetson-ready video downlink,
// but runs as a stub until Jetson + camera are present.
//
// When Jetson is available, ONLY THIS FILE will change.

namespace {
    std::atomic<bool> running{false};
}

// --------------------------------------------------
// Start video subsystem
// --------------------------------------------------
bool JetsonVideoLink::start()
{
    if (running.load()) {
        std::cerr << "[VIDEO][JETSON] Already running\n";
        return false;
    }

    running.store(true);

    std::cout << "[VIDEO][JETSON] Video link initialized\n";
    std::cout << "[VIDEO][JETSON] Mode: STUB (no Jetson hardware)\n";
    std::cout << "[VIDEO][JETSON] Expected pipeline:\n";
    std::cout << "  Camera → NVENC (H.264/H.265) → SRTP → GCS\n";
    std::cout << "  Port: 5600 / UDP\n";

    return true;
}

// --------------------------------------------------
// Stop video subsystem
// --------------------------------------------------
void JetsonVideoLink::stop()
{
    if (!running.load())
        return;

    running.store(false);
    std::cout << "[VIDEO][JETSON] Video link stopped\n";
}

// --------------------------------------------------
// Send encoded frame (Jetson path)
// --------------------------------------------------
//
// Contract:
// - data = already encoded H.264 / H.265 frame
// - len  = encoded frame size
// - timestamp_us = capture timestamp
//
// CURRENT BEHAVIOR (NO JETSON):
// - Frame is intentionally dropped
// - No failure, no retry
// - This preserves timing semantics
//
bool JetsonVideoLink::sendEncodedFrame(
    const uint8_t* data,
    size_t len,
    uint64_t timestamp_us)
{
    if (!running.load())
        return false;

    // Stub behavior: drop frame intentionally
    // This preserves backpressure semantics without Jetson
    (void)data;
    (void)len;
    (void)timestamp_us;

    return true;
}
