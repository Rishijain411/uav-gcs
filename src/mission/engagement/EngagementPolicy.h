#pragma once

#include "EngagementDecision.h"

struct TargetTrack {
    bool valid;
    double confidence;
    double range_m;
    double closing_speed;
};

class EngagementPolicy {
public:
    explicit EngagementPolicy(int max_attempts);   // ✅ declaration ONLY

    EngagementDecision evaluate(const TargetTrack& track);

    void registerMiss();
    void reset();

private:
    int max_attempts_;
    int attempts_ = 0;
};
