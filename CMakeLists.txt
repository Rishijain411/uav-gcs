cmake_minimum_required(VERSION 3.10)

project(my_gcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    # src/command/CommandTable.cpp        # ← ADD THIS WHEN YOU CREATE IT
)

target_include_directories(my_gcs PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/third_party
)
