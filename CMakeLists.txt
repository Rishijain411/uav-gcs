cmake_minimum_required(VERSION 3.16)

project(my_gcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt configuration (optional - only if building UI)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# =====================================================
#                    GLOBAL INCLUDES
# =====================================================
include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/third_party
)

# =====================================================
#                    MAIN GCS TARGET
# =====================================================
add_executable(my_gcs
    src/main.cpp

    # ---------------- Comm ----------------
    src/comm/UdpTransport.cpp
    src/comm/GcsHeartbeat.cpp

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
    src/mission/engagement/EngagementPolicy.cpp
    
    # ---------------- Phase B: Mission Planning ----------------
    src/mission/MissionProfileParser.cpp
    src/mission/PreFlightBit.cpp

    # ---------------- Authority ----------------
    src/authority/OperatorAuthorization.cpp
)

# =====================================================
#                    TEST TARGETS
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
)

# ---- Engagement Policy Test ----
add_executable(test_engagement_policy
    src/test/test_engagement_policy.cpp
    src/mission/engagement/EngagementPolicy.cpp
)

# ---- Engagement Policy V2 Test (with ProNav) ----
add_executable(test_engagement_v2
    src/test/test_engagement_policy_v2.cpp
    src/mission/engagement/EngagementPolicy.cpp
)

# ---- ProNav Test ----
add_executable(test_pronav
    src/test/test_pronav.cpp
    src/mission/engagement/ProportionalNavigation.cpp
)

# =====================================================
#                    REGISTER TESTS
# =====================================================
enable_testing()

add_test(NAME test_mission_state COMMAND test_mission_state)
add_test(NAME test_mission_transition COMMAND test_mission_transition)
add_test(NAME test_search_pattern COMMAND test_search_pattern)
add_test(NAME test_engagement_policy COMMAND test_engagement_policy)
add_test(NAME test_engagement_v2 COMMAND test_engagement_v2)
add_test(NAME test_pronav COMMAND test_pronav)

# =====================================================
#                    Qt UI (OPTIONAL)
# =====================================================
# Find Qt6 for UI application
find_package(Qt6 COMPONENTS Widgets QUIET)

if(Qt6_FOUND)
    message(STATUS "Qt6 found - building GCS UI")
    
    add_executable(gcs_ui
        src/ui/main_ui.cpp
        src/ui/MainWindow_QGC.cpp
        src/ui/MainWindow_QGC.h
    )
    
    target_link_libraries(gcs_ui PRIVATE Qt6::Widgets)
    
    message(STATUS "gcs_ui target created - run with: ./gcs_ui")
else()
    message(STATUS "Qt6 not found - skipping UI build (install qt6-base-dev to enable)")
endif()
