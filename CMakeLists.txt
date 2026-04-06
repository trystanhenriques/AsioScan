cmake_minimum_required(VERSION 3.16)

# Project definition
project(AsioScan
        VERSION 0.1.0
        DESCRIPTION "Cross-platform TCP port scanner using Boost.Asio"
        LANGUAGES CXX
)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# CLI11 (header-only)
include(FetchContent)

FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
)

FetchContent_MakeAvailable(CLI11)

# Catch2 (unit test framework)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)

FetchContent_MakeAvailable(Catch2)

# Core library (scanner + parser + formatter)
add_library(asioscan_lib
        src/config/scan_config.hpp
        src/result/port_result.hpp
        src/result/host_result.hpp
        src/result/scan_summary.hpp
        src/scanner/scanner.hpp
        src/scanner/scanner.cpp
        src/formatters/OutputOptions.hpp
        src/formatters/formatter.hpp
        src/formatters/text_formatter.hpp
        src/formatters/text_formatter.cpp
        src/formatters/text_formatter.hpp
        #
        # CLI parsing
        src/cli/cli_parser.cpp
        src/cli/cli_parser.hpp

)

# ------------------------------------------------------------
# Include directories
# ------------------------------------------------------------
target_include_directories(asioscan
        PRIVATE
        src
)

# Boost (cross-platform)
if (WIN32)
    find_package(Boost REQUIRED COMPONENTS system)

    target_link_libraries(asioscan_lib
        PUBLIC
        Boost::system
    )
else()
    find_package(Boost CONFIG REQUIRED)

    target_link_libraries(asioscan_lib
        PUBLIC
        Boost::boost   # headers-only target (this exists on Homebrew)
    )
endif()

# CLI11 is used by the CLI parser implementation
target_link_libraries(asioscan_lib PRIVATE CLI11::CLI11)

# Windows networking requirement
if (WIN32)
    target_link_libraries(asioscan_lib PUBLIC ws2_32)
endif()

# Thin executable entry point
add_executable(asioscan src/main.cpp)
target_link_libraries(asioscan PRIVATE asioscan_lib CLI11::CLI11)

# Keep console subsystem on Windows
if (WIN32)
    set_target_properties(asioscan PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()

# Enable CTest and add test targets
enable_testing()

# Catch2 test discovery helpers (Catch.cmake)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

add_subdirectory(tests)
