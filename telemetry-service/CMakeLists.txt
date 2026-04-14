cmake_minimum_required(VERSION 3.20)
project(objectbox-telemetry-service)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FetchContent for dependencies
include(FetchContent)

# 1. ObjectBox from source WITH sync enabled
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(OBX_BUILD_SYNC ON CACHE BOOL "Enable sync" FORCE)
FetchContent_Declare(
    objectbox
    GIT_REPOSITORY https://github.com/objectbox/objectbox-c.git
    GIT_TAG v5.1.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(objectbox)

# 2. cpp-httplib (header-only HTTP server)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(httplib)

# 3. nlohmann/json (header-only JSON library)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Executable
add_executable(telemetry_service 
    telemetry_service.cpp
    schema.obx.cpp
)

# Include directories
target_include_directories(telemetry_service PRIVATE
    ${objectbox_SOURCE_DIR}/include
    ${httplib_SOURCE_DIR}
    ${json_SOURCE_DIR}/single_include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(telemetry_service 
    objectbox
)

