# GCS UI Integration Guide

## Overview

This document describes the complete integration of the GCS backend mission control logic with the Qt6-based user interface.

## Architecture

### Backend (my_gcs)
- **Main Process**: `src/main.cpp` - GCS backend that connects to MAVLink vehicle
- **Mission State Machine**: Manages 11 mission states (INIT → PREFLIGHT → ARM_REQUESTED → ARMED → TRANSIT → SEARCH → ENGAGE → ASSESS → RTB → COMPLETE/ABORTED)
- **Mission Upload Protocol**: Implements MAVLink MISSION_COUNT/MISSION_ITEM_INT/MISSION_ACK handshake
- **Dual-Mode Search**:
  - If mission uploaded: Monitors `MISSION_CURRENT` messages from vehicle (PX4 executes autonomously)
  - If mission not uploaded: Generates dynamic waypoints via `ExpandingSquarePattern`

### Frontend (gcs_ui)
- **Qt6 UI**: `src/ui/MainWindow_QGC_v2.cpp` - Interactive mission control interface
- **Signal Bridge**: `src/ui/GCSBackendInterface.h/.cpp` - Qt signals for backend events
- **Mock Backend**: Integrated in `main_ui.cpp` for UI testing without live vehicle

## Build Instructions

### Prerequisites
```bash
sudo apt-get install qt6-base-dev libqt6core6 libqt6gui6
```

### Build Targets

**Backend (GCS core logic)**:
```bash
cd /home/khushhal/Projects/python/Internship/GCS-Vyuha
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target my_gcs
./my_gcs
```

**UI (Qt6 frontend with mock backend)**:
```bash
cmake --build . --target gcs_ui
./gcs_ui
```

## Key Components

### 1. GCSBackendInterface (Signal Bridge)

**File**: `src/ui/GCSBackendInterface.h/.cpp`

Acts as a bridge between backend events and Qt UI signals:

```cpp
class GCSBackendInterface : public QObject {
    Q_OBJECT
    
public slots:
    void onMissionStateChanged(mission::MissionState old_state, mission::MissionState new_state);
    void onTelemetryUpdated(const TelemetryData& telemetry);
    void onMissionUploadProgress(int waypoints_sent, int total_waypoints);
    void onMissionUploadComplete();
    void onCommandExecuted(const QString& command_name, bool success);
    void onError(const QString& error_message);
    
signals:
    void missionStateChanged(const QString& state);
    void positionUpdated(double lat, double lon, double alt);
    void armStateChanged(bool armed);
    void batteryUpdated(double voltage, double current, int percent);
    void ekfStatusChanged(bool ok);
    void missionUploadProgress(int sent, int total);
    void missionUploadComplete();
    void missionCurrentWaypoint(uint16_t seq);
    void commandAcknowledged(const QString& command);
    void commandFailed(const QString& command, const QString& error);
    void errorOccurred(const QString& error);
};
```

### 2. MainWindow Integration

**File**: `src/ui/MainWindow_QGC_v2.cpp`

The main UI window now receives backend updates via slot connections:

```cpp
// Connected slots in MainWindow class:
void onMissionStateChanged(const QString& state);
void updateTelemetryDisplay(double lat, double lon, double alt);
void updateArmState(bool armed);
void updateBatteryDisplay(int percent);
void updateMissionUploadProgress(int sent, int total);
void updateCurrentWaypoint(uint16_t seq);
void displayError(const QString& error);
```

### 3. UI Updates

The UI displays:

- **Mission State**: Color-coded state label (INIT→PREFLIGHT→ARM_REQUESTED→ARMED→TRANSIT→SEARCH→ENGAGE→ASSESS→RTB)
- **Position**: Latitude, longitude, altitude in real-time
- **Arm Status**: Dynamically updates ARM/DISARM button availability
- **Battery Status**: Percentage with color coding (green >50%, yellow 20-50%, red <20%)
- **Mission Upload Progress**: Shows waypoint count and percentage
- **Current Waypoint**: Displays PX4 execution progress during mission
- **Audit Log**: Timestamped events with color coding

## Mission Upload Protocol

The integration includes full MAVLink mission upload handshake:

1. **MISSION_COUNT**: GCS sends waypoint count
2. **MISSION_REQUEST_INT**: Vehicle requests waypoint by sequence
3. **MISSION_ITEM_INT**: GCS sends individual waypoint
4. **MISSION_ACK**: Vehicle acknowledges mission complete

**Status**: ✅ Tested with PX4 SITL (3 waypoints uploaded successfully)

## Dual-Mode Search Implementation

The system intelligently switches search modes based on mission upload status:

```cpp
// From MissionController::handleSearch()
if (telemetry.mission_upload_complete) {
    // Mode 1: Mission Uploaded
    // Monitor MISSION_CURRENT to track PX4 execution
    // Output: "[SEARCH] PX4 executing mission waypoint X"
} else {
    // Mode 2: Dynamic Waypoint Generation
    // Generate waypoints via ExpandingSquarePattern
    // Send to vehicle via cmd_manager_->sendSearchWaypoint()
    // Output: "[SEARCH] Generated waypoint X, sent to vehicle"
}
```

**Status**: ✅ Working with PX4 SITL

## UI Testing with Mock Backend

The integrated `main_ui.cpp` includes a mock GCS backend for UI testing without a real vehicle:

```cpp
class MockGCSBackend {
    // Simulates:
    // - Mission state progression (INIT → PREFLIGHT → ARM_REQUESTED → ARMED → TRANSIT → SEARCH)
    // - Telemetry updates (position, battery, arm state)
    // - Mission upload progress (3 waypoints)
    // - Current waypoint execution tracking
    
    // Runs in background thread, emits events every 100ms
};
```

**To test UI**:
```bash
./gcs_ui  # Displays mock mission execution with updates
```

## Integration Flow

```
┌─────────────────────────────────────────────────────────────┐
│                    Qt6 Application (gcs_ui)                 │
│                                                             │
│  MainWindow_QGC_v2                                          │
│  ├─ Tactical Map (resizable)                               │
│  ├─ Left Sidebar (mission/BIT status)                      │
│  ├─ Center Panel (mission commands)                        │
│  ├─ Right Panel (asset status)                             │
│  └─ Bottom Panel (video, telemetry, audit log)             │
│                                                             │
│  Slots (update handlers)                                   │
│  ├─ onMissionStateChanged()                                │
│  ├─ updateTelemetryDisplay()                               │
│  ├─ updateArmState()                                       │
│  ├─ updateBatteryDisplay()                                 │
│  └─ updateCurrentWaypoint()                                │
│                                                             │
└────────────────────────┬────────────────────────────────────┘
                         │ Qt Signals
                         ▼
┌─────────────────────────────────────────────────────────────┐
│             GCSBackendInterface (Signal Bridge)              │
│                                                             │
│  Slots (from backend):                                      │
│  ├─ onMissionStateChanged()                                │
│  ├─ onTelemetryUpdated()                                   │
│  ├─ onMissionUploadProgress()                              │
│  └─ onError()                                              │
│                                                             │
│  Signals (to UI):                                          │
│  ├─ missionStateChanged(QString state)                    │
│  ├─ positionUpdated(lat, lon, alt)                        │
│  ├─ armStateChanged(bool armed)                           │
│  ├─ batteryUpdated(...)                                   │
│  └─ missionCurrentWaypoint(uint16_t seq)                  │
│                                                             │
└────────────────────────┬────────────────────────────────────┘
                         │ Events from backend
                         ▼
┌─────────────────────────────────────────────────────────────┐
│              Backend (my_gcs or MockGCSBackend)             │
│                                                             │
│  Mission State Machine                                      │
│  ├─ INIT → PREFLIGHT → ARM_REQUESTED → ARMED               │
│  ├─ TRANSIT → SEARCH → ENGAGE → ASSESS → RTB               │
│  └─ COMPLETE / ABORTED                                     │
│                                                             │
│  Mission Upload Protocol (MAVLink)                          │
│  ├─ MISSION_COUNT handshake                                │
│  ├─ MISSION_ITEM_INT transmission                          │
│  └─ MISSION_ACK acknowledgment                             │
│                                                             │
│  Dual-Mode Search                                          │
│  ├─ If uploaded: Monitor MISSION_CURRENT                  │
│  └─ If not: Generate dynamic waypoints                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
         │
         ▼
    MAVLink Vehicle (PX4 SITL)
```

## Testing Checklist

- [x] Backend (my_gcs) compiles successfully
- [x] Frontend (gcs_ui) compiles successfully with Qt6
- [x] GCSBackendInterface signal bridge implemented
- [x] Mock backend integrated in UI for testing
- [x] Mission state transitions display correctly
- [x] Telemetry updates (position, battery, arm state) propagate to UI
- [x] Mission upload progress shown in UI
- [x] Current waypoint sequence displayed during mission execution
- [x] Audit log entries with color coding
- [x] Operator authorization hold buttons functional

## Files Modified/Created

### Created:
- `src/ui/GCSBackendInterface.h` - Signal bridge header
- `src/ui/GCSBackendInterface.cpp` - Signal bridge implementation
- `src/ui/MainWindow_QGC.h` - Updated with backend slots
- `UI_INTEGRATION_GUIDE.md` - This file

### Modified:
- `src/ui/main_ui.cpp` - Mock backend + signal connections
- `src/ui/MainWindow_QGC_v2.cpp` - Backend slot implementations
- `src/telemetry/TelemetryData.h` - Mission tracking fields
- `src/telemetry/TelemetryParser.cpp` - MISSION_CURRENT parsing
- `src/mission/MissionController.cpp` - Dual-mode search logic
- `CMakeLists.txt` - Added gcs_ui target with dependencies

## Known Limitations

1. **Mock Backend**: Uses hardcoded mission state progression (not real vehicle)
2. **Real Integration**: Requires connecting actual `my_gcs` backend to `GCSBackendInterface`
3. **Battery Data**: Simplified due to limited fields in TelemetryData (only battery_ok flag)
4. **Command Feedback**: UI commands not yet wired to backend command execution

## Next Steps for Full Integration

1. **Connect Real Backend**:
   - Instantiate GCSBackendInterface in my_gcs
   - Call `onMissionStateChanged()` from state machine
   - Call `onTelemetryUpdated()` from telemetry parser
   - Call `onMissionUploadProgress()` from mission upload logic

2. **UI Command Execution**:
   - Wire ARM/DISARM buttons to backend commands
   - Connect mission upload button to backend upload handler
   - Add mode selection UI controls

3. **Advanced Features**:
   - Real-time tactical map updates with vehicle position
   - Search pattern visualization
   - Target detection overlay
   - NFZ (No-Fly Zone) display

4. **Performance Optimization**:
   - Implement telemetry rate limiting
   - Add UI refresh throttling
   - Optimize signal/slot connections

## References

- Qt6 Documentation: https://doc.qt.io/qt-6/
- MAVLink Protocol: https://mavlink.io/
- PX4 Autopilot: https://px4.io/
