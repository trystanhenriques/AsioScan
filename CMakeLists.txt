cmake_minimum_required(VERSION 3.16)

# ------------------------------------------------------------
# Project definition
# ------------------------------------------------------------
project(AsioScan
        VERSION 0.1.0
        DESCRIPTION "Cross-platform TCP port scanner using Boost.Asio"
        LANGUAGES CXX
)

# ------------------------------------------------------------
# C++ standard (portable and strict)
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Compiler warnings
# ------------------------------------------------------------
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ------------------------------------------------------------
# CLI11 (command-line parsing, header-only)
# ------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
)

FetchContent_MakeAvailable(CLI11)

# ------------------------------------------------------------
# Main executable
# ------------------------------------------------------------
add_executable(asioscan
        src/main.cpp

        # Configuration / models
        src/config/scan_config.hpp
        #
        # Scanner engine (Boost.Asio logic)
        src/scanner/scanner.cpp
        src/scanner/scanner.hpp
        #
        # Result models
        src/result/port_result.hpp
        src/result/host_result.hpp
        src/result/scan_summary.hpp
        #
        # Formatting
        src/formatters/formatter.hpp
        src/formatters/OutputOptions.hpp
        src/formatters/text_formatter.cpp
        src/formatters/text_formatter.hpp

)

# ------------------------------------------------------------
# Include directories
# ------------------------------------------------------------
target_include_directories(asioscan
        PRIVATE
        src
)

# ------------------------------------------------------------
# Boost (correct cross-platform handling)
#
# Windows (vcpkg):
#   - Uses FindBoost module
#   - Requires explicit Boost.System linkage
#
# macOS (Homebrew):
#   - Uses BoostConfig.cmake
#   - Does NOT export Boost::system
#   - Asio is header-only; system library is resolved implicitly
# ------------------------------------------------------------
if (WIN32)
    find_package(Boost REQUIRED COMPONENTS system)

    target_link_libraries(asioscan
        PRIVATE
        Boost::system
    )
else()
    find_package(Boost CONFIG REQUIRED)

    target_link_libraries(asioscan
        PRIVATE
        Boost::boost   # headers-only target (this exists on Homebrew)
    )
endif()

# ------------------------------------------------------------
# CLI11 linkage (header-only)
# ------------------------------------------------------------
target_link_libraries(asioscan PRIVATE CLI11::CLI11)

# ------------------------------------------------------------
# Windows-specific networking requirement
# ------------------------------------------------------------
if (WIN32)
    target_link_libraries(asioscan PRIVATE ws2_32)
endif()

# ------------------------------------------------------------
# Ensure console subsystem on Windows
# ------------------------------------------------------------
if (WIN32)
    set_target_properties(asioscan PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()
