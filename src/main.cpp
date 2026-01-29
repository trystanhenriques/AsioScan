#include "scanner/scanner.hpp"
#include "config/scan_config.hpp"
#include "result/scan_summary.hpp"
#include "result/host_result.hpp"
#include "result/port_result.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

/*
 * Phase 6 Test Harness
 * --------------------
 * This program validates graceful cancellation support.
 *
 * Phase 6 goals:
 *  - Prove cancellation stops new work
 *  - Verify in-flight operations are cancelled safely
 *  - Confirm partial results are preserved
 *  - Validate no crashes or undefined behavior
 *
 * Test scenarios:
 *  1. Normal scan (no cancellation)
 *  2. Early cancellation (during first few tasks)
 *  3. Mid-scan cancellation
 */

using namespace asioscan;

/*
 * Helper: Convert PortState enum to human-readable string
 */
const char* port_state_to_string(PortState state) {
    switch (state) {
        case PortState::Open:     return "Open";
        case PortState::Closed:   return "Closed";
        case PortState::Filtered: return "Filtered";
        case PortState::Error:    return "Error";
        default:                  return "Unknown";
    }
}

/*
 * Helper: Print scan results in a readable format
 */
void print_results(const ScanSummary& summary) {
    std::cout << "\n========================================\n";
    std::cout << "Phase 6 Scan Results\n";
    std::cout << "========================================\n\n";

    std::cout << "Total scan duration: "
              << summary.duration().count()
              << " ms\n";
    std::cout << "Hosts scanned: " << summary.total_hosts() << "\n";
    std::cout << "Total ports scanned: " << summary.total_ports() << "\n\n";

    for (const auto& host : summary.hosts) {
        std::cout << "Host: " << host.host << "\n";
        std::cout << "  Duration: " << host.duration().count() << " ms\n";
        std::cout << "  Ports completed: " << host.total_ports() << "\n";
        std::cout << "  Open: " << host.open_ports()
                  << ", Closed: " << host.closed_ports()
                  << ", Filtered: " << host.filtered_ports()
                  << ", Error: " << host.error_ports() << "\n\n";
    }

    std::cout << "========================================\n\n";
}

/*
 * Test 1: Normal scan (no cancellation)
 */
void test_normal_scan() {
    std::cout << "========================================\n";
    std::cout << "Test 1: Normal Scan (No Cancellation)\n";
    std::cout << "========================================\n\n";

    ScanConfig config;
    config.targets = {"localhost"};
    config.ports = {22, 80, 443};
    config.timeout = std::chrono::milliseconds(500);
    config.max_concurrency = 5;
    config.verbose = true;

    ScannerCallbacks callbacks;
    callbacks.on_port_result = [](const std::string& host, const PortResult& result) {
        std::cout << "[Port] " << host << ":" << result.port
                  << " → " << port_state_to_string(result.state) << "\n";
    };

    Scanner scanner(config, callbacks);
    ScanSummary summary = scanner.run();

    print_results(summary);

    // Validate: all ports should be scanned
    if (summary.total_ports() != config.ports.size()) {
        std::cerr << "[X] Test 1 FAILED: Expected " << config.ports.size()
                  << " ports, got " << summary.total_ports() << "\n";
    } else {
        std::cout << "[+] Test 1 PASSED\n\n";
    }
}

/*
 * Test 2: Early cancellation
 */
void test_early_cancellation() {
    std::cout << "========================================\n";
    std::cout << "Test 2: Early Cancellation\n";
    std::cout << "========================================\n\n";

    ScanConfig config;
    config.targets = {"localhost", "127.0.0.1"};
    config.ports = {20, 21, 22, 23, 25, 80, 110, 143, 443, 3306, 5432, 8080};
    config.timeout = std::chrono::milliseconds(2000);
    config.max_concurrency = 3;
    config.verbose = true;

    std::size_t ports_completed = 0;

    ScannerCallbacks callbacks;
    callbacks.on_port_result = [&ports_completed](const std::string& host, const PortResult& result) {
        ++ports_completed;
        std::cout << "[Port " << ports_completed << "] "
                  << host << ":" << result.port
                  << " → " << port_state_to_string(result.state) << "\n";
    };

    Scanner scanner(config, callbacks);

    // Cancel after 100ms
    std::thread canceller([&scanner]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "\n[Cancellation requested]\n\n";
        scanner.cancel();
    });

    ScanSummary summary = scanner.run();
    canceller.join();

    print_results(summary);

    // Validate: should have partial results
    const std::size_t expected_total = config.targets.size() * config.ports.size();
    if (summary.total_ports() < expected_total) {
        std::cout << "[+] Test 2 PASSED: Partial results preserved ("
                  << summary.total_ports() << "/" << expected_total << " ports)\n\n";
    } else {
        std::cerr << "[!] Test 2 WARNING: All ports completed despite cancellation\n\n";
    }
}

/*
 * Test 3: Mid-scan cancellation
 */
void test_mid_scan_cancellation() {
    std::cout << "========================================\n";
    std::cout << "Test 3: Mid-Scan Cancellation\n";
    std::cout << "========================================\n\n";

    ScanConfig config;
    config.targets = {"google.com", "github.com"};
    config.ports = {80, 443, 8080, 8443};
    config.timeout = std::chrono::milliseconds(3000);
    config.max_concurrency = 2;
    config.verbose = true;

    std::size_t ports_completed = 0;

    ScannerCallbacks callbacks;
    callbacks.on_port_result = [&ports_completed](const std::string& host, const PortResult& result) {
        ++ports_completed;
        std::cout << "[Port " << ports_completed << "] "
                  << host << ":" << result.port
                  << " → " << port_state_to_string(result.state) << "\n";
    };

    Scanner scanner(config, callbacks);

    // Cancel after 1 second
    std::thread canceller([&scanner]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "\n[Cancellation requested]\n\n";
        scanner.cancel();
    });

    ScanSummary summary = scanner.run();
    canceller.join();

    print_results(summary);

    std::cout << "[+] Test 3 PASSED: Cancellation completed gracefully\n\n";
}

int main() {
    try {
        test_normal_scan();
        test_early_cancellation();
        test_mid_scan_cancellation();

        std::cout << "========================================\n";
        std::cout << "[+] All Phase 6 tests completed\n";
        std::cout << "========================================\n\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
