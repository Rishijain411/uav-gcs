#include <iostream>

using namespace std;

#include "mission/engagement/EngagementPolicy.h"

int main() {
    EngagementConfig config;
    config.max_reengagement_attempts = 2;
    EngagementPolicy policy(config);

    TargetTrack weak {true, 0.6, 500, 20};
    TargetTrack strong {true, 0.95, 300, 30};

    cout << int(policy.evaluate(weak)) << endl;   // REQUEST_CONFIRM
    cout << int(policy.evaluate(strong)) << endl; // ENGAGE

    policy.registerMiss();
    policy.registerMiss();

    cout << int(policy.evaluate(strong)) << endl; // ABORT

    cout << "[TEST] Engagement policy OK\n";
    return 0;
}
