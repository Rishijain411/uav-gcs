#!/bin/bash
# Build script for GCS-Vyuha with BUILD_MODE support
# Usage:
#   ./build.sh SITL        # Build for SITL testing (default)
#   ./build.sh PRODUCTION  # Build for production with all safety checks
#   ./build.sh clean       # Clean build directory

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
BUILD_MODE="${1:-SITL}"

echo "======================================"
echo "GCS-Vyuha Build Script"
echo "======================================"
echo "Build Mode: $BUILD_MODE"
echo "Project Root: $PROJECT_ROOT"
echo "Build Dir: $BUILD_DIR"
echo ""

# Handle clean target
if [ "$BUILD_MODE" = "clean" ]; then
    echo "[*] Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    echo "[✓] Clean complete"
    exit 0
fi

# Validate build mode
if [ "$BUILD_MODE" != "SITL" ] && [ "$BUILD_MODE" != "PRODUCTION" ]; then
    echo "[✗] Invalid BUILD_MODE: $BUILD_MODE"
    echo "    Use: SITL or PRODUCTION"
    exit 1
fi

# Create build directory if needed
mkdir -p "$BUILD_DIR"

# Configure
echo "[*] Configuring CMake with BUILD_MODE=$BUILD_MODE..."
cd "$BUILD_DIR"
cmake -DBUILD_MODE="$BUILD_MODE" "$PROJECT_ROOT"

# Build
echo ""
echo "[*] Building..."
cmake --build . -- -j$(nproc)

echo ""
echo "======================================"
echo "[✓] Build Complete!"
echo "======================================"
echo ""

if [ "$BUILD_MODE" = "SITL" ]; then
    echo "📋 SITL MODE (Testing):"
    echo "   ✓ Health checks DISABLED"
    echo "   ✓ UI ARM button will work without GPS/battery"
    echo "   ✓ Full logic flow testing enabled"
    echo ""
    echo "🚀 Run GCS:"
    echo "   ./build/my_gcs       # Headless backend"
    echo "   ./build/gcs_ui       # Qt UI"
else
    echo "📋 PRODUCTION MODE:"
    echo "   ✓ ALL safety checks ENFORCED"
    echo "   ✓ ARM requires: GPS lock + battery OK"
    echo "   ⚠️  Use only with real vehicle/HIL"
    echo ""
    echo "🚀 Run GCS:"
    echo "   ./build/my_gcs       # Headless backend"
    echo "   ./build/gcs_ui       # Qt UI"
fi

echo ""
echo "======================================"
