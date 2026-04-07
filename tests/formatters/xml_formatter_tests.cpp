#include <catch2/catch_test_macros.hpp>

#include "formatters/xml_formatter.hpp"
#include "formatters/OutputOptions.hpp"
#include "result/scan_summary.hpp"
#include "result/host_result.hpp"
#include "result/port_result.hpp"

#include <chrono>
#include <sstream>
#include <string>

namespace {

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

asioscan::ScanSummary make_empty_summary() {
    asioscan::ScanSummary summary;
    summary.start_time = std::chrono::steady_clock::now();
    summary.end_time = summary.start_time + std::chrono::milliseconds{150};
    return summary;
}

} // namespace

TEST_CASE("XmlFormatter prints XML declaration, root element and version", "[formatter][xml]") {
    const auto summary = make_empty_summary();
    asioscan::OutputOptions options;
    asioscan::XmlFormatter formatter;
    std::ostringstream out;

    formatter.print(out, summary, options);
    const std::string output = out.str();

    REQUIRE(output.find("<?xml version=\"1.0\"?>") != std::string::npos);
    REQUIRE(output.find("<asioscan") != std::string::npos);
    REQUIRE(output.find("version=\"0.1.0\"") != std::string::npos);
    REQUIRE(output.find("</asioscan>") != std::string::npos);
}

TEST_CASE("XmlFormatter emits scan metadata", "[formatter][xml]") {
    auto summary = make_empty_summary();
    asioscan::ScanConfig config;
    config.timeout = std::chrono::milliseconds{1000};
    config.max_concurrency = 50;
    config.ports = {22, 80};
    summary.config = config;

    asioscan::OutputOptions options;
    asioscan::XmlFormatter formatter;
    std::ostringstream out;

    formatter.print(out, summary, options);
    const std::string output = out.str();

    REQUIRE(output.find("<scaninfo") != std::string::npos);
    REQUIRE(output.find("timeout=\"1000\" unit=\"ms\"") != std::string::npos);
    REQUIRE(output.find("concurrency=\"50\"") != std::string::npos);
    REQUIRE(output.find("ports=\"22,80\"") != std::string::npos);
    REQUIRE(output.find("duration=") != std::string::npos);
    REQUIRE(output.find("unit=\"seconds\"") != std::string::npos);
}

TEST_CASE("XmlFormatter handles a single host with no open ports", "[formatter][xml]") {
    auto summary = make_empty_summary();
    
    asioscan::HostResult host;
    host.host = "192.168.1.1";
    summary.hosts.push_back(host);

    asioscan::OutputOptions options;
    asioscan::XmlFormatter formatter;
    std::ostringstream out;

    formatter.print(out, summary, options);
    const std::string output = out.str();

    REQUIRE(output.find("<host>") != std::string::npos);
    REQUIRE(output.find("<address addr=\"192.168.1.1\"/>") != std::string::npos);
    REQUIRE(output.find("</host>") != std::string::npos);
}

TEST_CASE("XmlFormatter escapes special XML characters", "[formatter][xml]") {
    auto summary = make_empty_summary();
    
    asioscan::HostResult host;
    host.host = "evil<host>&name\"'";
    
    asioscan::PortResult port = make_port(80, asioscan::PortState::Closed, std::chrono::milliseconds{5}, "reason <with> &special\"chars'");
    host.ports.push_back(port);
    summary.hosts.push_back(host);

    asioscan::OutputOptions options;
    asioscan::XmlFormatter formatter;
    std::ostringstream out;

    formatter.print(out, summary, options);
    const std::string output = out.str();

    // The single quote might optionally not be escaped standardly if in text but should be just in case, typical for attributes.
    // For now stick to mandatory: &, <, >, " as &amp; &lt; &gt; &quot; (&apos; optional but common).
    REQUIRE(output.find("evil&lt;host&gt;&amp;name&quot;&apos;") != std::string::npos);
    REQUIRE(output.find("reason &lt;with&gt; &amp;special&quot;chars&apos;") != std::string::npos);
}

TEST_CASE("XmlFormatter formats nested port results with attributes", "[formatter][xml]") {
    auto summary = make_empty_summary();
    
    asioscan::HostResult host;
    host.host = "localhost";
    host.ports = {
        make_port(22, asioscan::PortState::Open, std::chrono::milliseconds{10}, "syn-ack"),
        make_port(80, asioscan::PortState::Closed, std::chrono::milliseconds{5}, "rst")
    };
    summary.hosts.push_back(host);

    asioscan::OutputOptions options;
    asioscan::XmlFormatter formatter;
    std::ostringstream out;

    formatter.print(out, summary, options);
    const std::string output = out.str();

    REQUIRE(output.find("<port protocol=\"tcp\" portid=\"22\">") != std::string::npos);
    REQUIRE(output.find("<state state=\"open\" reason=\"syn-ack\"/>") != std::string::npos);
    REQUIRE(output.find("</port>") != std::string::npos);
    REQUIRE(output.find("<port protocol=\"tcp\" portid=\"80\">") != std::string::npos);
    REQUIRE(output.find("<state state=\"closed\" reason=\"rst\"/>") != std::string::npos);
}

TEST_CASE("XmlFormatter correctly prints run summary stats", "[formatter][xml]") {
    auto summary = make_empty_summary();
    
    asioscan::HostResult host1;
    host1.host = "h1";
    host1.ports = { make_port(22, asioscan::PortState::Open, std::chrono::milliseconds{1}, "ok") };
    
    asioscan::HostResult host2;
    host2.host = "h2";
    host2.ports = { make_port(80, asioscan::PortState::Closed, std::chrono::milliseconds{1}, "refused") };

    summary.hosts = {host1, host2};

    asioscan::OutputOptions options;
    asioscan::XmlFormatter formatter;
    std::ostringstream out;

    formatter.print(out, summary, options);
    const std::string output = out.str();

    REQUIRE(output.find("<runstats") != std::string::npos);
    REQUIRE(output.find("up=\"1\"") != std::string::npos);
    REQUIRE(output.find("down=\"1\"") != std::string::npos);
    REQUIRE(output.find("total=\"2\"") != std::string::npos);
    REQUIRE(output.find("ports-scanned=\"2\"") != std::string::npos);
    REQUIRE(output.find("open=\"1\"") != std::string::npos);
    REQUIRE(output.find("closed=\"1\"") != std::string::npos);
}

TEST_CASE("XmlFormatter sequential multiple hosts are serialized correctly", "[formatter][xml]") {
    auto summary = make_empty_summary();
    
    asioscan::HostResult host1;
    host1.host = "h1";
    
    asioscan::HostResult host2;
    host2.host = "h2";

    summary.hosts = {host1, host2};

    asioscan::OutputOptions options;
    asioscan::XmlFormatter formatter;
    std::ostringstream out;

    formatter.print(out, summary, options);
    const std::string output = out.str();

    auto pos1 = output.find("<address addr=\"h1\"/>");
    auto pos2 = output.find("<address addr=\"h2\"/>");
    
    REQUIRE(pos1 != std::string::npos);
    REQUIRE(pos2 != std::string::npos);
    REQUIRE(pos1 < pos2); // h1 must come before h2
}
