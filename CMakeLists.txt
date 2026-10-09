cmake_minimum_required(VERSION 3.20)

project(WxClash VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_GENERATOR STREQUAL "Ninja" AND
   NOT CMAKE_GENERATOR STREQUAL "Ninja Multi-Config")
    message(FATAL_ERROR
        "WxClash requires the Ninja generator. "
        "Configure with -G Ninja or use a CMake preset.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep the application and all MSVC CMake dependencies on the dynamic
# release CRT (/MD). This has no effect on non-MSVC toolchains.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING
        "MSVC runtime library used by all targets")
endif()

include(FetchContent)

# The sources are pinned so Windows and Linux use the same dependency versions.
# wxWidgets needs its Git submodules (PCRE2 and other bundled libraries).
FetchContent_Declare(
    wxWidgets
    GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
    GIT_TAG        v3.2.8
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES_RECURSE TRUE
)

set(wxBUILD_SHARED OFF CACHE BOOL "Build wxWidgets shared libraries" FORCE)
set(wxBUILD_MONOLITHIC OFF CACHE BOOL "Build wxWidgets as one library" FORCE)
set(wxBUILD_SAMPLES OFF CACHE BOOL "Build wxWidgets samples" FORCE)
set(wxBUILD_TESTS OFF CACHE BOOL "Build wxWidgets tests" FORCE)
set(wxBUILD_DEMOS OFF CACHE BOOL "Build wxWidgets demos" FORCE)
set(wxBUILD_USE_STATIC_RUNTIME OFF CACHE BOOL
    "Use the static MSVC runtime for wxWidgets" FORCE)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
    GIT_SHALLOW    TRUE
)

set(YAML_CPP_BUILD_SHARED_LIBS OFF CACHE BOOL
    "Build yaml-cpp shared libraries" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL
    "Build yaml-cpp contrib targets" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL
    "Build yaml-cpp tools" FORCE)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL
    "Build yaml-cpp tests" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL
    "Install yaml-cpp" FORCE)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(wxWidgets yaml-cpp nlohmann_json)

set(WXCLASH_SOURCES
    src/main.cpp
    src/config/app_config.cpp
    src/api/http_transport.cpp
    src/api/mihomo_api_client.cpp
    src/api/mihomo_api_service.cpp
    src/core/mihomo_sidecar.cpp
    src/ui/main_frame.cpp
    src/ui/pages.cpp
)

add_executable(WxClash WIN32 ${WXCLASH_SOURCES})

target_compile_features(WxClash PRIVATE cxx_std_20)

if(WIN32)
    target_compile_definitions(WxClash PRIVATE
        WINVER=0x0601
        _WIN32_WINNT=0x0601
    )
endif()

if(MSVC)
    target_compile_options(WxClash PRIVATE /utf-8)
endif()

target_link_libraries(WxClash PRIVATE
    wx::core
    wx::base
    wx::net
    yaml-cpp
    nlohmann_json::nlohmann_json
)

