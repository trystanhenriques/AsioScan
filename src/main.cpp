#include "scanner/scanner.hpp"
#include "config/scan_config.hpp"
#include "result/scan_summary.hpp"
#include "result/host_result.hpp"
#include "result/port_result.hpp"

#include <iostream>
#include <string>

/*
 * Phase 3 Test Harness
 * --------------------
 * This program validates the asynchronous correctness of the scanner engine
 * for exactly ONE host and ONE port.
 *
 * Phase 3 goals:
 *  - Prove async_connect + steady_timer race logic works correctly
 *  - Verify timeout behavior (Filtered state)
 *  - Verify connection refused (Closed state)
 *  - Verify successful connection (Open state)
 *  - Measure latency accurately using steady_clock
 *
 * Expected outcomes:
 *  - localhost:80 → Open (if HTTP server running)
 *  - localhost:1 → Closed (connection refused)
 *  - 10.255.255.1:80 → Filtered (timeout, non-routable IP)
 *
 * This is NOT a production CLI tool. It is a minimal test to validate
 * the core async behavior before adding concurrency in Phase 4.
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
    std::cout << "Phase 3 Scan Results\n";
    std::cout << "========================================\n\n";

    // Print overall timing
    std::cout << "Total scan duration: "
              << summary.duration().count()
              << " ms\n\n";

    // Print per-host results
    for (const auto& host : summary.hosts) {
        std::cout << "Host: " << host.host << "\n";
        std::cout << "Host scan duration: "
                  << host.duration().count()
                  << " ms\n\n";

        // Print per-port results
        for (const auto& port : host.ports) {
            std::cout << "  Port " << port.port << ":\n";
            std::cout << "    State:   " << port_state_to_string(port.state) << "\n";
            std::cout << "    Reason:  " << port.reason << "\n";
            std::cout << "    Latency: " << port.latency.count() << " ms\n";
            std::cout << "\n";
        }
    }

    std::cout << "========================================\n\n";
}

int main() {
    /*
     * Phase 3 Test Configuration
     * --------------------------
     * We test exactly ONE host and ONE port to validate async correctness.
     *
     * Test scenarios (uncomment one at a time):
     *  1. localhost:80  → Should be Open if HTTP server running, else Closed
     *  2. localhost:1   → Should be Closed (connection refused)
     *  3. 10.255.255.1:80 → Should be Filtered (timeout, non-routable IP)
     */

    // Create scan configuration
    ScanConfig config;

    // Test Case 1: localhost:80 (likely Open or Closed)
    config.targets = {"localhost"};
    config.ports = {80};

    // Test Case 2: localhost:1 (should be Closed)
    // config.targets = {"localhost"};
    // config.ports = {1};

    // Test Case 3: Non-routable IP (should timeout → Filtered)
    // config.targets = {"10.255.255.1"};
    // config.ports = {80};

    // Set timeout (Phase 3 default: 500ms)
    config.timeout = std::chrono::milliseconds(500);

    // Optional: enable verbose mode for future phases
    config.verbose = false;

    std::cout << "Starting Phase 3 Scanner Test...\n";
    std::cout << "Target: " << config.targets[0] << "\n";
    std::cout << "Port: " << config.ports[0] << "\n";
    std::cout << "Timeout: " << config.timeout.count() << " ms\n";

    try {
        /*
         * Create scanner with optional callbacks for live progress.
         * For Phase 3, we keep callbacks minimal.
         */
        ScannerCallbacks callbacks;

        // Optional: print live port results as they complete
        callbacks.on_port_result = [](const std::string& host, const PortResult& result) {
            std::cout << "\n[Live] Port " << result.port
                      << " on " << host
                      << " → " << port_state_to_string(result.state)
                      << " (" << result.reason << ")\n";
        };

        // Create scanner
        Scanner scanner(config, callbacks);

        /*
         * Run the scan.
         *
         * This call is SYNCHRONOUS from main's perspective:
         *  - It blocks until the scan completes
         *  - Internally it uses async I/O (async_connect + steady_timer)
         *  - When run() returns, all async operations have finished
         *
         * Expected behavior:
         *  - One async_connect initiated
         *  - One steady_timer initiated
         *  - They race; winner cancels loser
         *  - PortResult is populated
         *  - ScanSummary is returned
         */
        ScanSummary summary = scanner.run();

        // Print formatted results
        print_results(summary);

        // Verify Phase 3 constraints
        if (summary.total_hosts() != 1) {
            std::cerr << "ERROR: Expected exactly 1 host, got "
                      << summary.total_hosts() << "\n";
            return 1;
        }

        if (summary.total_ports() != 1) {
            std::cerr << "ERROR: Expected exactly 1 port, got "
                      << summary.total_ports() << "\n";
            return 1;
        }

        std::cout << "✓ Phase 3 test completed successfully.\n\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
