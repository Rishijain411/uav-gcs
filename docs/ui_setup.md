# Qt UI Setup Guide

## Prerequisites

### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install qt6-base-dev qt6-tools-dev cmake build-essential
# OR for Qt5:
# sudo apt-get install qtbase5-dev qt5-qmake cmake build-essential
```

### macOS
```bash
brew install qt@6 cmake
# Add Qt to PATH
echo 'export PATH="/opt/homebrew/opt/qt@6/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

### Windows
1. Download Qt installer from https://www.qt.io/download-open-source
2. Install Qt 6.x with MinGW or MSVC compiler
3. Add Qt bin directory to PATH:
   - Example: `C:\Qt\6.5.0\mingw_64\bin`

## Building the UI

```bash
cd GCS-Vyuha
mkdir build
cd build

# Configure with Qt CMake file
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64  # Adjust path for your system
# OR let CMake auto-detect Qt:
cmake ..

# Build
cmake --build .

# Run
./gcs_ui
```

## UI Layout

### Main Window (1200x800)

#### Left Panel: Telemetry
- Connection status (CONNECTED/DISCONNECTED)
- Arm state (ARMED/DISARMED)
- Flight phase (ON_GROUND/IN_AIR/etc)
- GPS status
- Battery level
- EKF status
- Failsafe indicator
- Position (lat/lon)
- Altitude

#### Middle Panel: Commands
- ARM button (green)
- DISARM button (red)
- TAKEOFF button (blue)
- LAND button (orange)
- ABORT MISSION button (purple)
- Active command indicator

#### Authorization Panel
- Shows pending authorization requests
- APPROVE button (replaces terminal 'y')
- DENY button (replaces terminal 'n')
- Request details (command, mission state)

#### Right Panel: Logs
- Real-time console output
- Color-coded messages
- Auto-scroll

## Integration with Existing GCS

To integrate with the current terminal-based GCS:

1. **Option A: Separate process** (current setup)
   - Run `./gcs_ui` alongside `./my_gcs`
   - UI reads shared memory or UDP updates
   - Keep terminal version for headless operation

2. **Option B: Merged application**
   - Modify `src/main.cpp` to initialize QApplication
   - Replace terminal authorization with UI signals
   - Run UI and GCS loop in same process

## Next Steps

1. **Install Qt** on your system
2. **Test build** with `CMakeLists_Qt.txt`
3. **Wire up signals** to connect UI buttons to CommandManager
4. **Replace terminal authorization** with Qt dialog
5. **Add map widget** for visualizing search patterns (optional, using QtLocation)

## Cross-Platform Notes

- **Linux**: Works out of the box with X11/Wayland
- **Windows**: Requires Qt DLLs deployed with executable
- **macOS**: Creates proper .app bundle automatically
- All platforms use native look-and-feel with Qt

## Troubleshooting

**Qt not found:**
```bash
# Linux: Install qt6-base-dev
# macOS: brew install qt6
# Windows: Add Qt bin to PATH
```

**MOC errors:**
Make sure `CMAKE_AUTOMOC ON` is set in CMakeLists.txt

**Missing widgets:**
Ensure you have Qt Widgets module installed, not just Qt Core
