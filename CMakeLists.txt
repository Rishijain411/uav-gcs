cmake_minimum_required(VERSION 3.10)

project(my_gcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
