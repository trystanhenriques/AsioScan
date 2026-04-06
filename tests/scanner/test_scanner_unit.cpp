#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "scanner/scanner.hpp"

#include <chrono>

using Catch::Matchers::ContainsSubstring;

namespace {

asioscan::ScanConfig make_minimal_valid_config() {
    asioscan::ScanConfig config;
    config.targets = {"localhost"};
    config.ports = {80};
    config.timeout = std::chrono::milliseconds{250};
    config.max_concurrency = 32;
    return config;
}

} // namespace

TEST_CASE("Scanner exposes an immutable config snapshot", "[scanner][unit]") {
    const auto config = make_minimal_valid_config();
    asioscan::Scanner scanner(config);

    const auto& snapshot = scanner.config();

    REQUIRE(snapshot.targets == config.targets);
    REQUIRE(snapshot.ports == config.ports);
    REQUIRE(snapshot.timeout == config.timeout);
    REQUIRE(snapshot.max_concurrency == config.max_concurrency);
}

TEST_CASE("Scanner rejects missing targets before starting I/O", "[scanner][unit]") {
    auto config = make_minimal_valid_config();
    config.targets.clear();

    asioscan::Scanner scanner(std::move(config));

    REQUIRE_THROWS_WITH(scanner.run(), ContainsSubstring("No target hosts specified"));
}

TEST_CASE("Scanner rejects missing ports before starting I/O", "[scanner][unit]") {
    auto config = make_minimal_valid_config();
    config.ports.clear();

    asioscan::Scanner scanner(std::move(config));

    REQUIRE_THROWS_WITH(scanner.run(), ContainsSubstring("No ports specified"));
}

TEST_CASE("Scanner cancel is safe and idempotent before run", "[scanner][unit]") {
    asioscan::Scanner scanner(make_minimal_valid_config());

    REQUIRE_NOTHROW(scanner.cancel());
    REQUIRE_NOTHROW(scanner.cancel());
}
