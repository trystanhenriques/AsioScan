#include <catch2/catch_test_macros.hpp>

#include "result/port_result.hpp"

TEST_CASE("PortResult stores scan outcome and exposes state helpers", "[smoke][result]") {
    asioscan::PortResult result;

    result.port = 443;
    result.state = asioscan::PortState::Open;
    result.latency = std::chrono::milliseconds(42);
    result.reason = "Connection established";

    REQUIRE(result.port == 443);
    REQUIRE(result.latency.count() == 42);
    REQUIRE(result.reason == "Connection established");

    REQUIRE(result.is_open());
    REQUIRE_FALSE(result.is_closed());
    REQUIRE_FALSE(result.is_filtered());
    REQUIRE_FALSE(result.is_error());
}
