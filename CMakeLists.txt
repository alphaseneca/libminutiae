cmake_minimum_required(VERSION 3.15)
project(libminutiae VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library Sources
set(MINUTIAE_SOURCES
    src/core/image_ops.cpp
    src/core/standards.cpp
    src/core/quality.cpp
    src/core/matcher.cpp
    src/drivers/r558s/r558s.cpp
)

if(WIN32)
    list(APPEND MINUTIAE_SOURCES src/transport/transport_win32.cpp)
endif()

# Core static library
add_library(minutiae STATIC ${MINUTIAE_SOURCES})

target_include_directories(minutiae PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(minutiae PRIVATE /W4)
else()
    target_compile_options(minutiae PRIVATE -Wall -Wextra -Werror -O2)
endif()

# Example 1: Basic Self-Test (100% On-Device / Synthetic Image)
add_executable(basic_self_test examples/01.BasicSelfTest/basic_self_test.cpp)
target_link_libraries(basic_self_test PRIVATE minutiae)

# Example 2: R558-S Live Hardware Capture (Windows / UART)
if(WIN32)
    add_executable(r558s_live_capture examples/02.R558SLiveCapture/r558s_live_capture.cpp)
    target_link_libraries(r558s_live_capture PRIVATE minutiae)
endif()
