#pragma once

#include "EngagementDecision.h"

struct TargetTrack {
    bool valid;
    double confidence;          // Lock confidence 0.0-1.0
    double range_m;
    double closing_speed;       // m/s (positive = closing)
};

struct EngagementConfig {
    double min_lock_confidence = 0.85;     // Minimum confidence to engage
    double max_intercept_range_m = 500.0;  // Max range for engagement
    int max_reengagement_attempts = 3;     // Max re-engagements after miss
    double min_closing_speed = 5.0;        // Min closing speed m/s
};

class EngagementPolicy {
public:
    explicit EngagementPolicy(const EngagementConfig& config);

    // Evaluate whether to engage based on current track
    EngagementDecision evaluate(const TargetTrack& track);

    // Register a miss (for re-engagement logic)
    void registerMiss();
    
    // Register successful engagement
    void registerHit();
    
    // Reset engagement state (new mission)
    void reset();
    
    // Get current attempt count
    int getAttemptCount() const { return attempts_; }
    
    // Check if max attempts reached
    bool hasExhaustedAttempts() const { return attempts_ >= config_.max_reengagement_attempts; }

private:
    EngagementConfig config_;
    int attempts_ = 0;
    
    bool checkLockConfidence(const TargetTrack& track);
    bool checkRange(const TargetTrack& track);
    bool checkClosingSpeed(const TargetTrack& track);
};

