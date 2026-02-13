#include "utils/MissionUploadBackoff.h"

#include <cassert>

int main() {
    const int base = 500;
    assert(computeMissionUploadBackoffMs(0, base) == 500);
    assert(computeMissionUploadBackoffMs(1, base) == 1000);
    assert(computeMissionUploadBackoffMs(2, base) == 2000);
    assert(computeMissionUploadBackoffMs(3, base) == 4000);
    assert(computeMissionUploadBackoffMs(9, base) == 500 * (1 << 8));
    return 0;
}