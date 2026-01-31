#include "../mission/engagement/EngagementPolicy.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Testing EngagementPolicy...\n";
    
    EngagementConfig config;
    config.min_lock_confidence = 0.85;
    config.max_intercept_range_m = 500.0;
    config.max_reengagement_attempts = 3;
    config.min_closing_speed = 5.0;
    
    EngagementPolicy policy(config);
    
    // Test 1: Invalid track → HOLD
    TargetTrack invalid_track;
    invalid_track.valid = false;
    invalid_track.confidence = 0.9;
    invalid_track.range_m = 100.0;
    invalid_track.closing_speed = 10.0;
    
    EngagementDecision decision = policy.evaluate(invalid_track);
    assert(decision == EngagementDecision::HOLD);
    std::cout << "✓ Invalid track → HOLD\n";
    
    // Test 2: Low confidence → REQUEST_CONFIRM
    TargetTrack low_conf_track;
    low_conf_track.valid = true;
    low_conf_track.confidence = 0.70;  // Below 0.85
    low_conf_track.range_m = 100.0;
    low_conf_track.closing_speed = 10.0;
    
    decision = policy.evaluate(low_conf_track);
    assert(decision == EngagementDecision::REQUEST_CONFIRM);
    std::cout << "✓ Low confidence → REQUEST_CONFIRM\n";
    
    // Test 3: Valid track → ENGAGE
    TargetTrack valid_track;
    valid_track.valid = true;
    valid_track.confidence = 0.95;
    valid_track.range_m = 100.0;
    valid_track.closing_speed = 15.0;
    
    decision = policy.evaluate(valid_track);
    assert(decision == EngagementDecision::ENGAGE);
    std::cout << "✓ Valid track → ENGAGE\n";
    
    // Test 4: After miss → REENGAGE
    policy.registerMiss();
    decision = policy.evaluate(valid_track);
    assert(decision == EngagementDecision::REENGAGE);
    std::cout << "✓ After miss → REENGAGE\n";
    
    // Test 5: Multiple misses → eventually ABORT
    policy.registerMiss();
    policy.registerMiss();
    assert(policy.getAttemptCount() == 3);
    assert(policy.hasExhaustedAttempts());
    
    decision = policy.evaluate(valid_track);
    assert(decision == EngagementDecision::ABORT);
    std::cout << "✓ Max attempts (3) → ABORT\n";
    
    // Test 6: Reset clears attempts
    policy.reset();
    assert(policy.getAttemptCount() == 0);
    decision = policy.evaluate(valid_track);
    assert(decision == EngagementDecision::ENGAGE);
    std::cout << "✓ Reset → ENGAGE again\n";
    
    // Test 7: Too far → ABORT
    TargetTrack far_track;
    far_track.valid = true;
    far_track.confidence = 0.95;
    far_track.range_m = 600.0;  // > 500m max
    far_track.closing_speed = 10.0;
    
    decision = policy.evaluate(far_track);
    assert(decision == EngagementDecision::ABORT);
    std::cout << "✓ Range > 500m → ABORT\n";
    
    // Test 8: Not closing → ABORT
    TargetTrack receding_track;
    receding_track.valid = true;
    receding_track.confidence = 0.95;
    receding_track.range_m = 100.0;
    receding_track.closing_speed = 2.0;  // < 5 m/s min
    
    decision = policy.evaluate(receding_track);
    assert(decision == EngagementDecision::ABORT);
    std::cout << "✓ Closing speed < 5 m/s → ABORT\n";
    
    std::cout << "\n✅ All EngagementPolicy tests passed!\n";
    return 0;
}
