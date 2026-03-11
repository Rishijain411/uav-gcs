#include "video/NullVideoLink.h"
#include <iostream>

bool NullVideoLink::start() {
    std::cout << "[VIDEO] Disabled (no Jetson)\n";
    return true;
}

void NullVideoLink::stop() {}

bool NullVideoLink::sendEncodedFrame(
    const uint8_t*, size_t, uint64_t) {
    return true;
}
