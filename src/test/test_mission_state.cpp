#include <cassert>
#include <iostream>

using namespace std;

#include "mission/Mission.h"
#include "mission/MissionEvent.h"

int main() {
    mission::Mission m;

    assert(m.state() == mission::MissionState::INIT);

    assert(m.apply_event(mission::MissionEvent::LOAD_MISSION));
    assert(m.state() == mission::MissionState::PREFLIGHT);

    assert(m.apply_event(mission::MissionEvent::PREFLIGHT_OK));
    assert(m.state() == mission::MissionState::ARMED);

    cout << "[TEST] Mission state transitions OK\n";
    return 0;
}
