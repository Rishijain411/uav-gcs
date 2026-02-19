# GCS-Vyuha UI ARM Button Fix - Summary

## Overview
Successfully fixed UI ARM button to work end-to-end in SITL mode, enabling full mission execution from the Qt UI without requiring hardware dependencies (GPS, battery).

## Build Modes

### SITL Mode (Testing)
```bash
./build.sh SITL
# or just: ./build.sh
```
**Features:**
- Health checks **DISABLED** (no GPS/battery required)
- ARM button works without preflight checks
- Faster command timeouts (1s) suitable for SITL speed
- Force-arm parameter enabled (param2=21196)
- Full logic testing enabled
- **Recommended for development & testing**

### Production Mode
```bash
./build.sh PRODUCTION
```
**Features:**
- ALL safety checks **ENFORCED**
- ARM requires: GPS lock + battery OK
- Longer command timeouts (5s) for real vehicle latency
- Normal ARM parameter (param2=0)
- **For real vehicle/HIL only**

### Clean Build
```bash
./build.sh clean
```

## Running the GCS

After building:
```bash
# Run Qt UI (with backend integration)
./build/gcs_ui

# Or run headless backend
./build/my_gcs
```

## Key Changes Made

### 1. Force-Arm Parameter (SITL Mode)
**File:** `src/command/MavlinkCommandSender.cpp`
- Added conditional compilation: sends `param2=21196` (force-arm) in SITL mode
- Sends `param2=0` (normal arm) in PRODUCTION mode
- Force-arm overrides preflight checks in PX4 (required for SITL)

### 2. ARM Command Routing Fix
**File:** `src/command/CommandManager.cpp`
- Fixed issue where generic `sendRawCommand()` wasn't sending ARM parameters
- Now routes ARM command through `sendArm()` method which includes parameters
- Other commands still use generic `sendRawCommand()`

### 3. ARM ACK Tracking
**Files:** 
- `src/command/CommandManager.h` - Added `arm_ack_received_` flag
- `src/command/CommandManager.cpp` - Sets flag when ARM ACK received
- Prevents race condition where TAKEOFF was sent before ARM confirmed

### 4. Mission State Machine Fix
**File:** `src/mission/MissionController.cpp`
- Changed ARM confirmation to require **BOTH**:
  1. ARM command ACK received ✓
  2. Telemetry confirms vehicle is ARMED ✓
- Prevents transition to ARMED before ACK arrives
- Prevents TAKEOFF being sent during ARM retry phase
- Clears timeout on ARMED state (prevents false abort)

### 5. Mission Upload Thread Safety
**Files:** 
- `src/ui/IntegratedBackend.h` - Added `std::mutex mission_file_mutex_`
- `src/ui/IntegratedBackend.cpp` - Protected mission file access
- Prevents race condition between UI and backend threads

## Execution Flow

### Normal Mission Execution (UI ARM → Mission)
```
1. User clicks "Load Mission" in UI
   ↓
2. Backend thread loads mission file (thread-safe with mutex)
   ↓
3. User clicks "ARM" in UI
   ↓
4. UI sends PREFLIGHT_OK event
   ↓
5. CommandManager sends ARM command with force-arm parameter (SITL)
   ↓
6. PX4 receives command and arms vehicle
   ↓
7. PX4 sends COMMAND_ACK (ACCEPTED)
   ↓
8. CommandManager sets arm_ack_received_ flag
   ↓
9. MissionController detects both: ACK received + telemetry shows ARMED
   ↓
10. Mission transitions to ARMED state
   ↓
11. CommandManager sends TAKEOFF (AUTO mode)
   ↓
12. Vehicle takes off and executes mission
```

## SITL Testing Workflow

```bash
# Terminal 1: Start PX4 SITL
cd ~/PX4-Autopilot
./Tools/sitl_run.sh px4 sitl

# Terminal 2: Build GCS
cd ~/Projects/python/Internship/GCS-Vyuha
./build.sh SITL  # or just: ./build.sh

# Terminal 3: Run UI
./build/gcs_ui
```

**In UI:**
1. Wait for "Connected to vehicle" message
2. Click "Load Mission" → Select mission file
3. Click "ARM" → Watch vehicle arm
4. Click "TAKEOFF" → Vehicle takes off automatically
5. Mission executes with loaded waypoints

## Command Timeout Settings

**SITL Mode:** 5000ms (5 seconds)
**PRODUCTION Mode:** 5000ms (5 seconds)
**Max Retries:** 5 attempts

## Testing Checklist

- [x] UI ARM button sends MAV_CMD_COMPONENT_ARM_DISARM (cmd=400)
- [x] Force-arm parameter (param2=21196) sent in SITL
- [x] ARM command ACK received and tracked
- [x] Vehicle actually arms (not just accepts command)
- [x] Vehicle stays armed after ARM command
- [x] TAKEOFF sent only after ARM confirmed
- [x] Mission executes with waypoints
- [x] Thread-safe mission file loading
- [x] No mission upload hangs with 10-second timeout
- [x] No unintended disarms from timeout logic

## Files Modified

1. `CMakeLists.txt` - BUILD_MODE configuration
2. `src/command/MavlinkCommandSender.h/cpp` - Force-arm parameter
3. `src/command/CommandManager.h/cpp` - ARM ACK tracking and routing
4. `src/mission/MissionController.cpp` - ARM confirmation logic
5. `src/ui/IntegratedBackend.h/cpp` - Thread-safe mission loading
6. `build.sh` - Build script with SITL/PRODUCTION modes

## Known Limitations

- SITL mode simulates slow network (ACKs take time)
- PX4 mission manager can be busy if restarting without mission clear
- Force-arm is SITL-only for safety; not used in production

## Production Considerations

When deploying to real vehicle:
1. Ensure `./build.sh PRODUCTION` is used
2. Verify GPS lock before attempting ARM
3. Check battery voltage is adequate
4. All preflight checks must pass before ARM
5. UI will enforce these restrictions automatically

## Troubleshooting

**Vehicle not arming in SITL:**
- Check PX4 shows "Ready for takeoff!" in console
- Verify GCS output shows `[DEBUG] Sending ARM with force-arm param2=21196`
- If not shown, SITL_MODE not compiled; rebuild with `./build.sh SITL`

**Mission upload hangs:**
- Timeout now 10 seconds with automatic error handling
- UI will show timeout error and recover automatically

**TAKEOFF doesn't execute after ARM:**
- Check both `[ARM ACK]` and `[MISSION] ARM_REQUESTED → ARMED` lines appear
- If only ARM ACK appears, wait for telemetry confirmation

## Questions?

Refer to `SITL_TESTING_GUIDE.md` for detailed testing guide.
