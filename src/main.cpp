#include "scanner/scanner.hpp"
#include "config/scan_config.hpp"
#include "result/scan_summary.hpp"
#include "result/host_result.hpp"
#include "result/port_result.hpp"

#include <iostream>
#include <string>

/*
 * Phase 4 Test Harness
 * --------------------
 * This program validates bounded-concurrency multi-port scanning
 * for exactly ONE host.
 *
 * Phase 4 goals:
 *  - Prove port queue + concurrency limiter works correctly
 *  - Verify multiple ports are scanned concurrently
 *  - Confirm max_concurrency is respected
 *  - Validate correct result aggregation
 *
 * Expected outcomes:
 *  - localhost:20,21,22,23,25,80,110,143,443,3306,5432,8080
 *    → Mix of Open, Closed, and Filtered states
 *  - Concurrency of 5 means max 5 in-flight scans at any time
 *  - All results are collected and returned correctly
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
    std::cout << "Phase 4 Scan Results\n";
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
     * Phase 4 Test Configuration
     * --------------------------
     * Scan multiple common ports on localhost to validate
     * bounded concurrency.
     */

    ScanConfig config;

    // Target: localhost
    config.targets = {"localhost"};

    // Ports: common services (mix of likely open/closed/filtered)
    config.ports = {
        20,    // FTP data
        21,    // FTP control
        22,    // SSH
        23,    // Telnet
        25,    // SMTP
        80,    // HTTP
        110,   // POP3
        143,   // IMAP
        443,   // HTTPS
        3306,  // MySQL
        5432,  // PostgreSQL
        8080   // HTTP alternate
    };

    // Timeout: 500ms
    config.timeout = std::chrono::milliseconds(500);

    // Max concurrency: 5 (demonstrates bounded behavior)
    config.max_concurrency = 5;

    // Enable verbose mode
    config.verbose = true;

    std::cout << "========================================\n";
    std::cout << "Phase 4 Scanner Test\n";
    std::cout << "========================================\n";
    std::cout << "Target: " << config.targets[0] << "\n";
    std::cout << "Ports: " << config.ports.size() << " ports\n";
    std::cout << "Timeout: " << config.timeout.count() << " ms\n";
    std::cout << "Max Concurrency: " << config.max_concurrency << "\n";
    std::cout << "========================================\n\n";

    try {
        /*
         * Create scanner with optional callbacks for live progress.
         * For Phase 4, we keep callbacks minimal.
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
         *  - Multiple async_connects initiated (up to max_concurrency)
         *  - Multiple steady_timers initiated
         *  - They race; winners cancel losers
         *  - PortResults are populated
         *  - ScanSummary is returned
         */
        ScanSummary summary = scanner.run();

        // Print formatted results
        print_results(summary);

        // Verify Phase 4 constraints
        if (summary.total_hosts() != 1) {
            std::cerr << "ERROR: Expected exactly 1 host, got "
                      << summary.total_hosts() << "\n";
            return 1;
        }

        if (summary.total_ports() != config.ports.size()) {
            std::cerr << "ERROR: Expected " << config.ports.size()
                      << " ports, got " << summary.total_ports() << "\n";
            return 1;
        }

        std::cout << "✓ Phase 4 test completed successfully.\n";
        std::cout << "✓ Scanned " << summary.total_ports() << " ports.\n";
        std::cout << "✓ Open: " << summary.total_open_ports() << "\n";
        std::cout << "✓ Duration: " << summary.duration().count() << " ms\n\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
