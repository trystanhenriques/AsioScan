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
# Compiler warnings (safe across platforms)
# ------------------------------------------------------------
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ------------------------------------------------------------
# Find Boost (only what we need)
# Boost.Asio requires Boost.System
# ------------------------------------------------------------
find_package(Boost CONFIG REQUIRED)

# ------------------------------------------------------------
# Main executable
# ------------------------------------------------------------
add_executable(asioscan
        # Entry point
        src/main.cpp

        # --------------------------------------------------------
        # CLI parsing (add files here later)
        # src/cli/cli_parser.cpp
        # src/cli/cli_parser.hpp
        #
        # Scan configuration
        # src/config/scan_config.cpp
        src/config/scan_config.hpp
        #
        # Scanner engine (Boost.Asio logic)
        # src/scanner/scanner.cpp
        # src/scanner/scanner.hpp
        #
        # Result models
        src/result/port_result.hpp
        src/result/host_result.hpp
        # src/result/scan_summary.hpp
        #
        # Output formatters
        # src/format/text_formatter.cpp
        # src/format/xml_formatter.cpp
        #
        # Utilities (color, helpers, etc.)
        # src/util/color.hpp
        # src/util/net_utils.hpp
)

# ------------------------------------------------------------
# Include directories
# ------------------------------------------------------------
target_include_directories(asioscan
        PRIVATE
        src            # Allows includes like "scanner/scanner.hpp"
)

# ------------------------------------------------------------
# Link Boost libraries
# ------------------------------------------------------------
target_link_libraries(asioscan
        PRIVATE
        Boost::headers
)

# ------------------------------------------------------------
# Windows-specific networking requirement
# ------------------------------------------------------------
if (WIN32)
    target_link_libraries(asioscan PRIVATE ws2_32)
endif()

# ------------------------------------------------------------
# Ensure this is a console application on Windows
# ------------------------------------------------------------
if (WIN32)
    set_target_properties(asioscan PROPERTIES
            WIN32_EXECUTABLE FALSE
    )
endif()

target_compile_definitions(asioscan
        PRIVATE
        BOOST_ERROR_CODE_HEADER_ONLY
)
