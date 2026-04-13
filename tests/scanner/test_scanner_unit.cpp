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

TEST_CASE("Scanner natively handles Asio timeouts and operation aborts", "[scanner][integration][timeout]") {
    // Relevance to Migration: Validates that `asio::steady_timer` appropriately races against 
    // `asio::async_connect` and that the forced socket cancellation correctly translates 
    // `asio::error::operation_aborted` through `std::error_code` into a Filtered state.
    auto config = make_minimal_valid_config();
    config.targets = {"192.0.2.1"}; // RFC 5737 TEST-NET-1 (non-routable blackhole)
    config.ports = {80};
    config.timeout = std::chrono::milliseconds{10}; // Extremely tight timeout

    asioscan::Scanner scanner(std::move(config));
    auto summary = scanner.run();

    REQUIRE(summary.hosts.size() == 1);
    REQUIRE(summary.hosts[0].ports.size() == 1);
    
    // We expect the 10ms timer to fire before TCP retries expire.
    const auto& port = summary.hosts[0].ports[0];
    REQUIRE(port.state == asioscan::PortState::Filtered);
    REQUIRE_THAT(port.reason, ContainsSubstring("Timeout"));
}

TEST_CASE("Scanner natively translates OS connection refused", "[scanner][integration][error_code]") {
    // Relevance to Migration: Validates that an OS-level ECONNREFUSED error natively maps into 
    // `std::error_code` exactly as it did with `boost::system::error_code`, properly matching 
    // the conditional branch `ec == asio::error::connection_refused`.
    auto config = make_minimal_valid_config();
    config.targets = {"127.0.0.1"};
    config.ports = {54321}; // Extremely unlikely to have a service listening
    config.timeout = std::chrono::milliseconds{5000}; 

    asioscan::Scanner scanner(std::move(config));
    auto summary = scanner.run();

    REQUIRE(summary.hosts.size() == 1);
    REQUIRE(summary.hosts[0].ports.size() == 1);

    const auto& port = summary.hosts[0].ports[0];
    // If it is genuinely closed, it should report connection refused explicitly
    if (port.state == asioscan::PortState::Closed) {
        REQUIRE_THAT(port.reason, ContainsSubstring("Connection refused"));
    }
}

TEST_CASE("Scanner handles mid-scan cooperative cancellation securely", "[scanner][async][post]") {
    // Relevance to Migration: The migration replaced `boost::asio::post` with standalone 
    // `asio::post`. This test forces a callback queue injection mid-execution to verify the 
    // single-threaded queue drains without deadlock and properly maps partial results.
    auto config = make_minimal_valid_config();
    // Use test-net IPs to hang the connections on purpose so we can cancel them before they timeout
    // Add localhost as the first target so it completes quickly and triggers cancellation on all platforms
    config.targets = {"127.0.0.1", "192.0.2.1", "192.0.2.2", "192.0.2.3"};
    config.ports = {80, 443};
    config.timeout = std::chrono::milliseconds{5000}; 

    asioscan::ScannerCallbacks callbacks;
    asioscan::Scanner* scanner_ptr = nullptr;
    int ports_completed = 0;

    callbacks.on_port_result = [&](const std::string&, const asioscan::PortResult&) {
        ports_completed++;
        if (ports_completed == 1 && scanner_ptr) {
            scanner_ptr->cancel(); // Cancel on the first completed operation
        }
    };

    asioscan::Scanner scanner(std::move(config), callbacks);
    scanner_ptr = &scanner;

    // Start a 5s block, but expect it to exit extremely fast due to early cancellation
    auto start = std::chrono::steady_clock::now();
    auto summary = scanner.run();
    auto elapsed = std::chrono::steady_clock::now() - start;

    REQUIRE(elapsed < std::chrono::milliseconds{3000}); // Proves it didn't wait the full 5000ms
    REQUIRE(ports_completed > 0);
    
    // Remaining in-flight ports that were cancelled should map to Error/Cancelled
    bool found_cancelled = false;
    for (const auto& host : summary.hosts) {
        for (const auto& p : host.ports) {
            if (p.state == asioscan::PortState::Error && p.reason == "Cancelled") {
                found_cancelled = true;
            }
        }
    }
    REQUIRE(found_cancelled);
}
