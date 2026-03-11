cmake_minimum_required(VERSION 3.16)

project(my_gcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt configuration (optional)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# =====================================================
#                     BUILD MODES
# =====================================================
# Default to SITL mode for testing
if(NOT DEFINED BUILD_MODE)
    set(BUILD_MODE "SITL" CACHE STRING "Build mode: SITL or PRODUCTION")
endif()

message(STATUS "Build Mode: ${BUILD_MODE}")

# Set compiler flags based on build mode
if(BUILD_MODE STREQUAL "SITL")
    message(STATUS ">>> SITL MODE: Health checks disabled for testing")
    add_compile_definitions(SITL_MODE=1)
    set(CMAKE_BUILD_TYPE Debug)
elseif(BUILD_MODE STREQUAL "PRODUCTION")
    message(STATUS ">>> PRODUCTION MODE: All safety checks enforced")
    add_compile_definitions(SITL_MODE=0)
    set(CMAKE_BUILD_TYPE Release)
else()
    message(FATAL_ERROR "Invalid BUILD_MODE. Use 'SITL' or 'PRODUCTION'")
endif()

# =====================================================
#                     GLOBAL INCLUDES
# =====================================================
include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/third_party
)

# =====================================================
#                     DEPENDENCIES
# =====================================================
find_package(OpenSSL REQUIRED)
find_package(Qt6 COMPONENTS Widgets QUIET)

if(Qt6_FOUND)
    set(HAVE_QT 1)
else()
    set(HAVE_QT 0)
endif()

# =====================================================
#                     MAIN GCS TARGET
# =====================================================
add_executable(my_gcs
    src/main.cpp

    # ---------------- Comm ----------------
    src/comm/UdpTransport.cpp
    src/comm/GcsHeartbeat.cpp
    src/comm/LinkManager.cpp

    # ---------------- Security ----------------
    src/security/SecureChannel.cpp

    # ---------------- Telemetry ----------------
    src/telemetry/TelemetryParser.cpp

    # ---------------- Core ----------------
    src/core/StateManager.cpp

    # ---------------- Command ----------------
    src/command/CommandManager.cpp
    src/command/MavlinkCommandSender.cpp
    src/command/CommandTable.cpp

    # ---------------- Mission ----------------
    src/mission/Mission.cpp
    src/mission/MissionController.cpp

    # ---------------- Mission Logic ----------------
    src/mission/search/ExpandingSquarePattern.cpp
    src/mission/search/SectorSearchPattern.cpp        # NEW: Added to support Phase 2 Adaptive Search
    src/mission/engagement/EngagementPolicy.cpp
    src/mission/engagement/ProportionalNavigation.cpp # NEW: Added to support Phase 3 Terminal Guidance

    # ---------------- Phase B: Mission Planning ----------------
    src/mission/MissionProfileParser.cpp
    src/mission/PreFlightBit.cpp

    # ---------------- Authority ----------------
    src/authority/OperatorAuthorization.cpp
    
    # ----------- UI Backend Bridge -------- 
    src/ui/GCSBackendInterface.cpp
    
    # ---------------- Video Streaming----------------
    src/video/NullVideoLink.cpp
    src/video/JetsonVideoLink.cpp
    src/video/NullSrtpSession.cpp

)

# OpenSSL (Security)
target_link_libraries(my_gcs PRIVATE OpenSSL::Crypto)

# Qt (if available)
if(Qt6_FOUND)
    target_link_libraries(my_gcs PRIVATE Qt6::Widgets)
endif()
# =====================================================
#                     TEST TARGETS
# =====================================================

# ---- Mission State Machine Test ----
add_executable(test_mission_state
    src/test/test_mission_state.cpp
    src/mission/Mission.cpp
)

# ---- Mission Transition Authority Test ----
add_executable(test_mission_transition
    src/test/test_mission_transition.cpp
    src/mission/Mission.cpp
    src/authority/OperatorAuthorization.cpp
)

# ---- Search Pattern Test ----
add_executable(test_search_pattern
    src/test/test_search_pattern.cpp
    src/mission/search/ExpandingSquarePattern.cpp
    src/mission/search/SectorSearchPattern.cpp
)

# ---- Engagement Policy Test ----
add_executable(test_engagement_policy
    src/test/test_engagement_policy.cpp
    src/mission/engagement/EngagementPolicy.cpp
    src/mission/engagement/ProportionalNavigation.cpp
)

# ---- Engagement Policy V2 Test (with ProNav) ----
add_executable(test_engagement_v2
    src/test/test_engagement_policy_v2.cpp
    src/mission/engagement/EngagementPolicy.cpp
    src/mission/engagement/ProportionalNavigation.cpp # NEW: Added to fix linker error in V2 test suite
)

# ---- ProNav Test ----
add_executable(test_pronav
    src/test/test_pronav.cpp
    src/mission/engagement/ProportionalNavigation.cpp
)

# ---- Mission Upload Tests ----
add_executable(test_mission_upload_handshake
    src/test/test_mission_upload_handshake.cpp
    src/command/MavlinkCommandSender.cpp
    src/telemetry/TelemetryParser.cpp
    src/comm/LinkManager.cpp
    src/comm/UdpTransport.cpp
    src/core/StateManager.cpp
    src/security/SecureChannel.cpp
)
target_link_libraries(test_mission_upload_handshake PRIVATE OpenSSL::Crypto)

add_executable(test_mission_upload_backoff
    src/test/test_mission_upload_backoff.cpp
)

# =====================================================
#                     REGISTER TESTS
# =====================================================
enable_testing()

add_test(NAME test_mission_state COMMAND test_mission_state)
add_test(NAME test_mission_transition COMMAND test_mission_transition)
add_test(NAME test_search_pattern COMMAND test_search_pattern)
add_test(NAME test_engagement_policy COMMAND test_engagement_policy)
add_test(NAME test_engagement_v2 COMMAND test_engagement_v2)
add_test(NAME test_pronav COMMAND test_pronav)
add_test(NAME test_mission_upload_handshake COMMAND test_mission_upload_handshake)
add_test(NAME test_mission_upload_backoff COMMAND test_mission_upload_backoff)

# =====================================================
#                     Qt UI (OPTIONAL)
# =====================================================
# Find Qt6 for UI application
find_package(Qt6 COMPONENTS Widgets QUIET)

if(Qt6_FOUND)
    message(STATUS "Qt6 found - building GCS UI")
    
    add_executable(gcs_ui
        src/ui/main_ui.cpp
        src/ui/MainWindow_QGC_v2.cpp
        src/ui/MainWindow_QGC.h
        src/ui/GCSBackendInterface.cpp
        src/ui/GCSBackendInterface.h
        src/ui/IntegratedBackend.cpp
        src/ui/IntegratedBackend.h
        src/mission/Mission.cpp
        src/mission/MissionController.cpp
        src/mission/MissionProfileParser.cpp
        src/mission/search/ExpandingSquarePattern.cpp
        src/mission/search/SectorSearchPattern.cpp
        src/mission/engagement/EngagementPolicy.cpp
        src/mission/engagement/ProportionalNavigation.cpp
        src/telemetry/TelemetryParser.cpp
        src/command/MavlinkCommandSender.cpp
        src/command/CommandManager.cpp
        src/comm/LinkManager.cpp
        src/comm/UdpTransport.cpp
        src/comm/GcsHeartbeat.cpp
        src/core/StateManager.cpp
        src/security/SecureChannel.cpp
        src/authority/OperatorAuthorization.cpp
    )
    
    target_link_libraries(gcs_ui PRIVATE Qt6::Widgets OpenSSL::Crypto)
    
    message(STATUS "gcs_ui target created - run with: ./gcs_ui")
else()
    message(STATUS "Qt6 not found - skipping UI build (install qt6-base-dev to enable)")
endif()