#include <iostream>

using namespace std;

#include "mission/Mission.h"
#include "mission/MissionEvent.h"
#include "authority/MissionTransitionAuthority.h"

int main() {
    mission::Mission m;

    // Should prompt for operator input
    MissionTransitionAuthority::requestTransition(
        m, mission::MissionEvent::LOAD_MISSION);

    MissionTransitionAuthority::requestTransition(
        m, mission::MissionEvent::PREFLIGHT_OK);

    cout << "[TEST] MissionTransitionAuthority executed\n";
    return 0;
}
