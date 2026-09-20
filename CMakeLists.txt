cmake_minimum_required(VERSION 3.15)
project(crdl VERSION 1.0.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(CRDL_BUILD_TESTS "Build tests" ON)
option(CRDL_BUILD_EXAMPLES "Build examples" ON)
option(CRDL_ENABLE_LOGGING "Enable detailed logging" ON)

# Platform detection
if(WIN32)
    add_definitions(-DCRDL_PLATFORM_WINDOWS)
    add_definitions(-DNOMINMAX)
    add_definitions(-D_WIN32_WINNT=0x0601)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DCRDL_PLATFORM_LINUX)
elseif(APPLE)
    add_definitions(-DCRDL_PLATFORM_MACOS)
endif()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Add widevine-cpp
add_subdirectory(../widevine-cpp ${CMAKE_BINARY_DIR}/widevine-cpp EXCLUDE_FROM_ALL)

# Find dependencies
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)

# Try to find packages from vcpkg first
find_package(nlohmann_json CONFIG QUIET)
find_package(spdlog CONFIG QUIET)

# If not found in vcpkg, fetch from source
include(FetchContent)

if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found in vcpkg, fetching from source")
    FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(json)
endif()

if(NOT spdlog_FOUND)
    message(STATUS "spdlog not found in vcpkg, fetching from source")
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.12.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# Core library
add_subdirectory(src)

# Tests
if(CRDL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(CRDL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
install(TARGETS crdl
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/crdl
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
