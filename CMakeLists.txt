

cmake_minimum_required(VERSION 3.10)

project(my_gcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(my_gcs
    src/main.cpp
    src/comm/UdpTransport.cpp
    src/telemetry/TelemetryParser.cpp
    src/core/StateManager.cpp
)

target_include_directories(my_gcs PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/third_party
)

