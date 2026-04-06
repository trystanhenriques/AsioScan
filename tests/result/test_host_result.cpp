#include <catch2/catch_test_macros.hpp>

#include "result/host_result.hpp"

#include <chrono>

namespace {

asioscan::PortResult make_port(std::uint16_t port, asioscan::PortState state) {
    asioscan::PortResult result;
    result.port = port;
    result.state = state;
    return result;
}

} // namespace

TEST_CASE("HostResult defaults to an empty host result", "[result][host]") {
    const asioscan::HostResult host;

    REQUIRE(host.host.empty());
    REQUIRE(host.ports.empty());
    REQUIRE(host.total_ports() == 0);
    REQUIRE(host.open_ports() == 0);
    REQUIRE(host.closed_ports() == 0);
    REQUIRE(host.filtered_ports() == 0);
    REQUIRE(host.error_ports() == 0);
}

TEST_CASE("HostResult aggregates per-port state counts", "[result][host]") {
    asioscan::HostResult host;
    host.host = "scanme.nmap.org";
    host.ports = {
        make_port(22, asioscan::PortState::Open),
        make_port(80, asioscan::PortState::Closed),
        make_port(443, asioscan::PortState::Open),
        make_port(8080, asioscan::PortState::Filtered),
        make_port(3306, asioscan::PortState::Error)
    };

    REQUIRE(host.host == "scanme.nmap.org");
    REQUIRE(host.total_ports() == 5);
    REQUIRE(host.open_ports() == 2);
    REQUIRE(host.closed_ports() == 1);
    REQUIRE(host.filtered_ports() == 1);
    REQUIRE(host.error_ports() == 1);

    REQUIRE(host.open_ports() + host.closed_ports() + host.filtered_ports() + host.error_ports()
            == host.total_ports());
}

TEST_CASE("HostResult duration is derived from start and end timestamps", "[result][host]") {
    asioscan::HostResult host;

    const auto start = std::chrono::steady_clock::now();
    host.start_time = start;
    host.end_time = start + std::chrono::milliseconds{125};

    REQUIRE(host.duration() == std::chrono::milliseconds{125});
}
