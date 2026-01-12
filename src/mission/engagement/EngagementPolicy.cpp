#include "EngagementPolicy.h"

EngagementPolicy::EngagementPolicy(int max_attempts)
    : max_attempts_(max_attempts) {}

EngagementDecision EngagementPolicy::evaluate(
    const TargetTrack& track)
{
    if (!track.valid)
        return EngagementDecision::HOLD;

    if (track.confidence < 0.85)
        return EngagementDecision::REQUEST_CONFIRM;

    if (attempts_ >= max_attempts_)
        return EngagementDecision::ABORT;

    return EngagementDecision::ENGAGE;
}

void EngagementPolicy::registerMiss() {
    attempts_++;
}

void EngagementPolicy::reset() {
    attempts_ = 0;
}
