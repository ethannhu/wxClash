cmake_minimum_required(VERSION 3.8)

project(WxClash LANGUAGES CXX)

# Link the MSVC runtime statically to reduce Win7 deployment dependencies.
set(
    CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# Build wxWidgets statically.
set(wxBUILD_SHARED OFF CACHE BOOL "" FORCE)
set(wxBUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(wxBUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(3rdparty/wxWidgets)

add_executable(WxClash WIN32
    src/main.cpp
    src/ui/main_frame.cpp
)

target_compile_features(WxClash PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(WxClash PRIVATE /utf-8)
endif()

# Declare Windows 7 as the minimum target system.
target_compile_definitions(WxClash PRIVATE
    WINVER=0x0601
    _WIN32_WINNT=0x0601
)

target_link_libraries(WxClash PRIVATE
    wx::core
    wx::base
)
