#include <catch2/catch_test_macros.hpp>

#include "formatters/output_writer.hpp"
#include "formatters/OutputOptions.hpp"
#include "result/scan_summary.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::filesystem::path get_temp_file_path(const std::string& prefix) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    std::string filename = prefix + "_" + std::to_string(ms) + ".tmp";
    return std::filesystem::temp_directory_path() / filename;
}

} // namespace

TEST_CASE("write_output writes XML to requested file safely without leaking to default stream", "[formatter][integration][xml][io]") {
    auto temp_path = get_temp_file_path("asioscan_xml");

    asioscan::ScanSummary summary;
    summary.start_time = std::chrono::steady_clock::now();
    summary.end_time = summary.start_time;

    asioscan::OutputOptions options;
    options.format = asioscan::OutputFormat::Xml;
    options.output_file = temp_path.string();

    std::ostringstream default_stream;

    bool success = asioscan::write_output(summary, options, default_stream);
    
    REQUIRE(success);
    REQUIRE(std::filesystem::exists(temp_path));
    REQUIRE(default_stream.str().empty()); // Default stream undisturbed

    std::ifstream infile(temp_path);
    REQUIRE(infile.is_open());
    std::stringstream buffer;
    buffer << infile.rdbuf();
    const std::string content = buffer.str();
    infile.close();

    // Verify fundamental XML tokens exist
    REQUIRE(content.find("<?xml version=\"1.0\"?>") != std::string::npos);
    REQUIRE(content.find("<asioscan") != std::string::npos);

    std::filesystem::remove(temp_path);
}

TEST_CASE("write_output writes Text to requested file safely without leaking to default stream", "[formatter][integration][text][io]") {
    auto temp_path = get_temp_file_path("asioscan_txt");

    asioscan::ScanSummary summary;
    summary.start_time = std::chrono::steady_clock::now();
    summary.end_time = summary.start_time;

    asioscan::OutputOptions options;
    options.format = asioscan::OutputFormat::Text;
    options.output_file = temp_path.string();

    std::ostringstream default_stream;

    bool success = asioscan::write_output(summary, options, default_stream);
    
    REQUIRE(success);
    REQUIRE(std::filesystem::exists(temp_path));
    REQUIRE(default_stream.str().empty());

    std::ifstream infile(temp_path);
    std::stringstream buffer;
    buffer << infile.rdbuf();
    const std::string content = buffer.str();
    infile.close();

    REQUIRE(content.find("AsioScan Scan Report") != std::string::npos);

    std::filesystem::remove(temp_path);
}

#ifndef _WIN32
TEST_CASE("write_output gracefully handles missing/invalid directory context", "[formatter][integration][io]") {
    asioscan::ScanSummary summary;
    asioscan::OutputOptions options;
    options.format = asioscan::OutputFormat::Text;
    
    // Attempting to write to an impossible system path should fail cleanly instead of throwing unhandled
    options.output_file = "/invalid_impossible_directory_999/out.txt";
    
    std::ostringstream default_stream;
    bool success = asioscan::write_output(summary, options, default_stream);
    
    REQUIRE_FALSE(success);
    REQUIRE(default_stream.str().empty());
}
#endif