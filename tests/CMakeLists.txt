# Test executable for AsioScan core library
add_executable(asioscan_tests
    test_smoke.cpp
    cli/cli_parser_tests.cpp
    result/test_port_result.cpp
    result/test_host_result.cpp
    result/test_scan_summary.cpp
    formatters/text_formatter_tests.cpp
    formatters/output_options_tests.cpp
    scanner/test_scanner_unit.cpp
)

# Link production library and Catch2 test main
target_link_libraries(asioscan_tests
    PRIVATE
    asioscan_lib
    Catch2::Catch2WithMain
)

# Enable Catch2 test discovery with CTest
include(Catch)
catch_discover_tests(asioscan_tests)
