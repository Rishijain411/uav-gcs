#pragma once

#include <algorithm>

inline int computeMissionUploadBackoffMs(int retry, int base_ms, int max_shift = 8) {
    const int shift = std::min(retry, max_shift);
    return base_ms * (1 << shift);
}