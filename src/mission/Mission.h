#pragma once

#include "MissionState.h"
#include "MissionEvent.h"
#include "MissionProfile.h"
#include <optional>
#include "mission/MissionAbortReason.h"

namespace mission {

class Mission {
public:
    Mission();

    MissionState state() const;

    // Core state transition API
    bool apply_event(MissionEvent event);

    // NEW: State latch tracking
    bool isStateNewlyEntered() const;
    void markStateHandled();
    
    // Phase B: Mission Profile Management
    bool loadProfile(const MissionProfile& profile);
    bool hasValidProfile() const;
    const MissionProfile& getProfile() const;
    
    // Phase B: Pre-Flight BIT status
    struct BitStatus {
        bool motors_ok = false;
        bool battery_ok = false;
        bool mavlink_ok = false;
        bool payload_ok = false;
        
        bool allPassed() const {
            return motors_ok && battery_ok && mavlink_ok && payload_ok;
        }

    };
    void setAbortReason(MissionAbortReason reason);
    MissionAbortReason getAbortReason() const;

    
    BitStatus getBitStatus() const { return bit_status_; }
    void setBitStatus(const BitStatus& status) { bit_status_ = status; }
    void updateFailsafeRules(const FailsafeRules& rules);
private:
    MissionState current_state_;
    bool state_just_changed_ = false;  // NEW: Track state transitions
    
    // Phase B: Mission Profile
    std::optional<MissionProfile> profile_;
    
    // Phase B: Pre-Flight BIT
    BitStatus bit_status_;
    MissionAbortReason abort_reason_ = MissionAbortReason::NONE;


    MissionState transition(MissionState state, MissionEvent event);
};

} // namespace mission
