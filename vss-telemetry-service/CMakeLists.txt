cmake_minimum_required(VERSION 3.20)
project(vss-telemetry-service)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(OBX_BUILD_SYNC ON CACHE BOOL "Enable sync" FORCE)
FetchContent_Declare(
    objectbox
    GIT_REPOSITORY https://github.com/objectbox/objectbox-c.git
    GIT_TAG v5.1.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(objectbox)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

add_executable(vss_service
    vss_service.cpp
    schema_vss.obx.cpp
)

target_include_directories(vss_service PRIVATE
    ${objectbox_SOURCE_DIR}/include
    ${httplib_SOURCE_DIR}
    ${json_SOURCE_DIR}/single_include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(vss_service objectbox)
