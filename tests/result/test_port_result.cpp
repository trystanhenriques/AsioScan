#include <catch2/catch_test_macros.hpp>

#include "result/port_result.hpp"

#include <chrono>

TEST_CASE("PortResult default construction is predictable", "[result][port]") {
    const asioscan::PortResult result;

    REQUIRE(result.port == 0);
    REQUIRE(result.state == asioscan::PortState::Error);
    REQUIRE(result.latency == std::chrono::milliseconds{0});
    REQUIRE(result.reason.empty());

    REQUIRE_FALSE(result.is_open());
    REQUIRE_FALSE(result.is_closed());
    REQUIRE_FALSE(result.is_filtered());
    REQUIRE(result.is_error());
}

TEST_CASE("PortResult reflects assigned fields and state helpers", "[result][port]") {
    asioscan::PortResult result;

    SECTION("Open state") {
        result.port = 443;
        result.state = asioscan::PortState::Open;
        result.latency = std::chrono::milliseconds{42};
        result.reason = "Connection established";

        REQUIRE(result.port == 443);
        REQUIRE(result.latency.count() == 42);
        REQUIRE(result.reason == "Connection established");
        REQUIRE(result.is_open());
    }

    SECTION("Closed state") {
        result.state = asioscan::PortState::Closed;

        REQUIRE(result.is_closed());
        REQUIRE_FALSE(result.is_open());
        REQUIRE_FALSE(result.is_filtered());
        REQUIRE_FALSE(result.is_error());
    }

    SECTION("Filtered state") {
        result.state = asioscan::PortState::Filtered;

        REQUIRE(result.is_filtered());
        REQUIRE_FALSE(result.is_open());
        REQUIRE_FALSE(result.is_closed());
        REQUIRE_FALSE(result.is_error());
    }
}
