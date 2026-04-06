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

asioscan::ScanSummary make_hosts_only_summary() {
    asioscan::ScanSummary summary;

    const auto start = std::chrono::steady_clock::now();
    summary.start_time = start;

    asioscan::HostResult up_host;
    up_host.host = "up.example";
    up_host.start_time = start;
    up_host.ports = {
        make_port(22, asioscan::PortState::Open, std::chrono::milliseconds{10}, "Connection established")
    };
    up_host.end_time = start + std::chrono::milliseconds{10};

    asioscan::HostResult down_host;
    down_host.host = "down.example";
    down_host.start_time = start;
    down_host.end_time = start + std::chrono::milliseconds{10};

    summary.hosts = {up_host, down_host};
    summary.end_time = start + std::chrono::milliseconds{15};

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

TEST_CASE("TextFormatter normal mode shows reasons when requested", "[formatter][text][normal]") {
    const auto summary = make_sample_summary();

    asioscan::OutputOptions options;
    options.text_mode = asioscan::TextMode::Normal;
    options.show_reason = true;

    asioscan::TextFormatter formatter;

    StdoutCapture capture;
    formatter.print(summary, options);
    const std::string output = capture.str();

    REQUIRE(output.find("REASON") != std::string::npos);
    REQUIRE(output.find("Connection established") != std::string::npos);
    REQUIRE(output.find("Connection refused") != std::string::npos);
}

TEST_CASE("TextFormatter summary mode emits high-level counters only", "[formatter][text][summary]") {
    const auto summary = make_sample_summary();

    asioscan::OutputOptions options;
    options.text_mode = asioscan::TextMode::Summary;

    asioscan::TextFormatter formatter;

    StdoutCapture capture;
    formatter.print(summary, options);
    const std::string output = capture.str();

    REQUIRE(output.find("Hosts scanned: 1") != std::string::npos);
    REQUIRE(output.find("Ports scanned: 3") != std::string::npos);
    REQUIRE(output.find("Open ports: 2") != std::string::npos);

    REQUIRE(output.find("AsioScan Scan Report") == std::string::npos);
    REQUIRE(output.find("Host: scanme.nmap.org") == std::string::npos);
    REQUIRE(output.find("PORT") == std::string::npos);
}

TEST_CASE("TextFormatter ports-only mode prints per-host port lines", "[formatter][text][ports-only]") {
    const auto summary = make_sample_summary();

    asioscan::OutputOptions options;
    options.text_mode = asioscan::TextMode::PortsOnly;

    asioscan::TextFormatter formatter;

    StdoutCapture capture;
    formatter.print(summary, options);
    const std::string output = capture.str();

    REQUIRE(output.find("Host: scanme.nmap.org") != std::string::npos);
    REQUIRE(output.find("22/tcp open") != std::string::npos);
    REQUIRE(output.find("80/tcp closed") != std::string::npos);
    REQUIRE(output.find("443/tcp open") != std::string::npos);

    REQUIRE(output.find("AsioScan Scan Report") == std::string::npos);
    REQUIRE(output.find("Scan Summary") == std::string::npos);
}

TEST_CASE("TextFormatter hosts-only mode prints host status lines", "[formatter][text][hosts-only]") {
    const auto summary = make_hosts_only_summary();

    asioscan::OutputOptions options;
    options.text_mode = asioscan::TextMode::HostsOnly;

    asioscan::TextFormatter formatter;

    StdoutCapture capture;
    formatter.print(summary, options);
    const std::string output = capture.str();

    REQUIRE(output.find("up.example: up (1 open port)") != std::string::npos);
    REQUIRE(output.find("down.example: down (0 open ports)") != std::string::npos);

    REQUIRE(output.find("PORT") == std::string::npos);
    REQUIRE(output.find("AsioScan Scan Report") == std::string::npos);
}

TEST_CASE("TextFormatter verbose mode includes latency, reason, and attempts", "[formatter][text][verbose]") {
    const auto summary = make_sample_summary();

    asioscan::OutputOptions options;
    options.text_mode = asioscan::TextMode::Verbose;
    options.show_reason = false;

    asioscan::TextFormatter formatter;

    StdoutCapture capture;
    formatter.print(summary, options);
    const std::string output = capture.str();

    REQUIRE(output.find("LATENCY(ms)") != std::string::npos);
    REQUIRE(output.find("REASON") != std::string::npos);
    REQUIRE(output.find("Attempts: 1") != std::string::npos);
    REQUIRE(output.find("Connection established") != std::string::npos);
    REQUIRE(output.find("Connection refused") != std::string::npos);
}
