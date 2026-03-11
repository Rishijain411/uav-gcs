#include "EngagementPolicy.h"

EngagementPolicy::EngagementPolicy(const EngagementConfig& config)
    : config_(config) {}

EngagementDecision EngagementPolicy::evaluate(
    const TargetTrack& track)
{
    // 1. Check basic validity
    if (!track.valid) {
        return EngagementDecision::HOLD;
    }
    
    // 2. Check if max attempts exhausted
    if (hasExhaustedAttempts()) {
        return EngagementDecision::ABORT;
    }
    
    // 3. Check lock confidence
    if (!checkLockConfidence(track)) {
        return EngagementDecision::REQUEST_CONFIRM;
    }
    
    // 4. Check range constraints
    if (!checkRange(track)) {
        return EngagementDecision::ABORT;
    }
    
    // 5. Check closing speed
    if (!checkClosingSpeed(track)) {
        return EngagementDecision::ABORT;
    }
    
    // 6. All checks passed - determine if re-engagement or first attempt
    if (attempts_ > 0) {
        return EngagementDecision::REENGAGE;
    }
    
    return EngagementDecision::ENGAGE;
}

void EngagementPolicy::registerMiss() {
    attempts_++;
}

void EngagementPolicy::registerHit() {
    // Successful engagement - reset for next target
    attempts_ = 0;
}

void EngagementPolicy::reset() {
    attempts_ = 0;
}

bool EngagementPolicy::checkLockConfidence(const TargetTrack& track) {
    return track.confidence >= config_.min_lock_confidence;
}

bool EngagementPolicy::checkRange(const TargetTrack& track) {
    // Must be within max range and above minimum safe distance
    const double min_safe_range = 10.0; // meters
    return track.range_m >= min_safe_range && 
           track.range_m <= config_.max_intercept_range_m;
}

bool EngagementPolicy::checkClosingSpeed(const TargetTrack& track) {
    return track.closing_speed >= config_.min_closing_speed;
}

