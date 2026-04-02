#include <catch2/catch_test_macros.hpp>

#include "result/scan_summary.hpp"

#include <chrono>

namespace {

asioscan::HostResult make_host(
    const char* name,
    std::initializer_list<asioscan::PortState> states
) {
    asioscan::HostResult host;
    host.host = name;

    std::uint16_t port = 1000;
    for (const auto state : states) {
        asioscan::PortResult result;
        result.port = port++;
        result.state = state;
        host.ports.push_back(result);
    }

    return host;
}

} // namespace

TEST_CASE("ScanSummary defaults to an empty summary", "[result][summary]") {
    const asioscan::ScanSummary summary;

    REQUIRE_FALSE(summary.config.has_value());
    REQUIRE(summary.hosts.empty());
    REQUIRE(summary.total_hosts() == 0);
    REQUIRE(summary.total_ports() == 0);
    REQUIRE(summary.total_open_ports() == 0);
    REQUIRE(summary.hosts_up() == 0);
    REQUIRE(summary.hosts_down() == 0);
    REQUIRE_FALSE(summary.has_errors());
}

TEST_CASE("ScanSummary aggregates host-level counts consistently", "[result][summary]") {
    asioscan::ScanSummary summary;
    summary.hosts = {
        make_host("host-a", {asioscan::PortState::Open, asioscan::PortState::Closed}),
        make_host("host-b", {asioscan::PortState::Filtered, asioscan::PortState::Error}),
        make_host("host-c", {asioscan::PortState::Open})
    };

    REQUIRE(summary.total_hosts() == 3);
    REQUIRE(summary.total_ports() == 5);
    REQUIRE(summary.total_open_ports() == 2);
    REQUIRE(summary.hosts_up() == 2);
    REQUIRE(summary.hosts_down() == 1);
    REQUIRE(summary.has_errors());

    REQUIRE(summary.hosts_up() + summary.hosts_down() == summary.total_hosts());
    REQUIRE(summary.total_open_ports() <= summary.total_ports());
}

TEST_CASE("ScanSummary duration uses scan start and end timestamps", "[result][summary]") {
    asioscan::ScanSummary summary;

    const auto start = std::chrono::steady_clock::now();
    summary.start_time = start;
    summary.end_time = start + std::chrono::milliseconds{375};

    REQUIRE(summary.duration() == std::chrono::milliseconds{375});
}
