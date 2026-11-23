cmake_minimum_required(VERSION 3.24)

project(NAPP
    VERSION 0.1.0
    DESCRIPTION "NAPP — ILLIERTION cross-platform web application platform"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(NAPP_BUILD_TESTS "Build NAPP unit tests" ON)
option(NAPP_ENABLE_CEF "Build the CEF based runtime host (requires third_party/cef)" OFF)

# Base URL of the repository the launcher registers on first start. Override to
# publish from a different GitHub account/org:
#   cmake -DNAPP_OFFICIAL_REPOSITORY_URL=https://raw.githubusercontent.com/<owner>/<repo>/main/
set(NAPP_OFFICIAL_REPOSITORY_URL
    "https://raw.githubusercontent.com/ILLIERTION/NAPP-Repository/main/"
    CACHE STRING "Default NAPP repository base URL")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent Quick QuickWidgets)
qt_standard_project_setup(REQUIRES 6.5)

if(NAPP_BUILD_TESTS)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
endif()

include(cmake/NappCommon.cmake)

add_subdirectory(libs/core)
add_subdirectory(libs/ipkg)
add_subdirectory(launcher)
add_subdirectory(runtime)

if(NAPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "NAPP ${PROJECT_VERSION} — Qt ${Qt6_VERSION}, CEF host: ${NAPP_ENABLE_CEF}")
