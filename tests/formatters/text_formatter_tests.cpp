#include <catch2/catch_test_macros.hpp>

#include "formatters/text_formatter.hpp"
#include "formatters/OutputOptions.hpp"
#include "result/scan_summary.hpp"
#include "result/host_result.hpp"
#include "result/port_result.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

namespace {

class StdoutCapture {
public:
    StdoutCapture() : old_buf_(std::cout.rdbuf(stream_.rdbuf())) {}

    ~StdoutCapture() {
        std::cout.rdbuf(old_buf_);
    }

    std::string str() const {
        return stream_.str();
    }

private:
    std::ostringstream stream_;
    std::streambuf* old_buf_;
};

asioscan::PortResult make_port(
    std::uint16_t port,
    asioscan::PortState state,
    std::chrono::milliseconds latency,
    std::string reason
) {
    asioscan::PortResult result;
    result.port = port;
    result.state = state;
    result.latency = latency;
    result.reason = std::move(reason);
    return result;
}

asioscan::ScanSummary make_sample_summary() {
    asioscan::ScanSummary summary;

    const auto start = std::chrono::steady_clock::now();
    summary.start_time = start;

    asioscan::HostResult host;
    host.host = "scanme.nmap.org";
    host.start_time = start;
    host.ports = {
        make_port(22, asioscan::PortState::Open, std::chrono::milliseconds{10}, "Connection established"),
        make_port(80, asioscan::PortState::Closed, std::chrono::milliseconds{5}, "Connection refused"),
        make_port(443, asioscan::PortState::Open, std::chrono::milliseconds{12}, "Connection established")
    };
    host.end_time = start + std::chrono::milliseconds{20};

    summary.hosts.push_back(host);
    summary.end_time = start + std::chrono::milliseconds{25};

    return summary;
}

} // namespace

TEST_CASE("TextFormatter normal mode prints report structure and key fields", "[formatter][text][normal]") {
    const auto summary = make_sample_summary();

    asioscan::OutputOptions options;
    options.text_mode = asioscan::TextMode::Normal;
    options.show_reason = false;

    asioscan::TextFormatter formatter;

    StdoutCapture capture;
    formatter.print(summary, options);
    const std::string output = capture.str();

    REQUIRE(output.find("AsioScan Scan Report") != std::string::npos);
    REQUIRE(output.find("Host: scanme.nmap.org") != std::string::npos);
    REQUIRE(output.find("PORT") != std::string::npos);
    REQUIRE(output.find("STATE") != std::string::npos);

    REQUIRE(output.find("22") != std::string::npos);
    REQUIRE(output.find("80") != std::string::npos);
    REQUIRE(output.find("443") != std::string::npos);
    REQUIRE(output.find("Open") != std::string::npos);
    REQUIRE(output.find("Closed") != std::string::npos);

    REQUIRE(output.find("Scan Summary") != std::string::npos);
    REQUIRE(output.find("Hosts scanned: 1") != std::string::npos);
    REQUIRE(output.find("Open ports:    2") != std::string::npos);

    REQUIRE(output.find("Connection established") == std::string::npos);
}

TEST_CASE("TextFormatter quiet mode prints only open ports in script-friendly format", "[formatter][text][quiet]") {
    const auto summary = make_sample_summary();

    asioscan::OutputOptions options;
    options.text_mode = asioscan::TextMode::Quiet;

    asioscan::TextFormatter formatter;

    StdoutCapture capture;
    formatter.print(summary, options);
    const std::string output = capture.str();

    REQUIRE(output.find("scanme.nmap.org:22 open") != std::string::npos);
    REQUIRE(output.find("scanme.nmap.org:443 open") != std::string::npos);

    REQUIRE(output.find("scanme.nmap.org:80 open") == std::string::npos);
    REQUIRE(output.find("AsioScan Scan Report") == std::string::npos);
    REQUIRE(output.find("Scan Summary") == std::string::npos);
    REQUIRE(output.find("Host:") == std::string::npos);
}
