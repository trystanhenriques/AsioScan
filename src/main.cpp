#include "scanner/scanner.hpp"
#include "config/scan_config.hpp"
#include "result/scan_summary.hpp"
#include "result/host_result.hpp"
#include "result/port_result.hpp"

#include <iostream>
#include <string>

/*
 * Phase 5 Test Harness
 * --------------------
 * This program validates multi-host scanning with bounded concurrency.
 *
 * Phase 5 goals:
 *  - Prove multi-host scanning works correctly
 *  - Verify per-host result aggregation is accurate
 *  - Confirm bounded concurrency applies globally (across all hosts)
 *  - Validate host completion detection
 *
 * Expected outcomes:
 *  - Multiple hosts scanned (localhost, 127.0.0.1, google.com)
 *  - Each host gets an independent HostResult
 *  - All port results are correctly associated with their host
 *  - Concurrency limit is respected globally
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
    std::cout << "Phase 5 Scan Results\n";
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
     * Phase 5 Test Configuration
     * --------------------------
     * Scan multiple hosts to validate multi-host support.
     */

    ScanConfig config;

    // Targets: multiple hosts (local + remote)
    config.targets = {
        "localhost",
        "127.0.0.1",
        "google.com"
    };

    // Ports: subset of common services
    config.ports = {
        22,    // SSH
        80,    // HTTP
        443,   // HTTPS
        8080   // HTTP alternate
    };

    // Timeout: 1000ms (longer for remote hosts)
    config.timeout = std::chrono::milliseconds(1000);

    // Max concurrency: 10 (applies globally across all hosts)
    config.max_concurrency = 10;

    // Enable verbose mode
    config.verbose = true;

    std::cout << "========================================\n";
    std::cout << "Phase 5 Scanner Test\n";
    std::cout << "========================================\n";
    std::cout << "Targets: " << config.targets.size() << " hosts\n";
    for (const auto& target : config.targets) {
        std::cout << "  - " << target << "\n";
    }
    std::cout << "Ports per host: " << config.ports.size() << " ports\n";
    std::cout << "Total tasks: " << (config.targets.size() * config.ports.size()) << "\n";
    std::cout << "Timeout: " << config.timeout.count() << " ms\n";
    std::cout << "Max Concurrency: " << config.max_concurrency << " (global)\n";
    std::cout << "========================================\n\n";

    try {
        /*
         * Create scanner with optional callbacks for live progress.
         * For Phase 5, we add host-complete callback.
         */
        ScannerCallbacks callbacks;

        // Optional: print live port results as they complete
        callbacks.on_port_result = [](const std::string& host, const PortResult& result) {
            std::cout << "\n[Live] Port " << result.port
                      << " on " << host
                      << " → " << port_state_to_string(result.state)
                      << " (" << result.reason << ")\n";
        };

        // Optional: print live host results as they complete
        callbacks.on_host_complete = [](const HostResult& host_result) {
            std::cout << "\n[Live] Host " << host_result.host
                      << " completed in " << host_result.duration().count() << " ms\n";
            std::cout << "  Open: " << host_result.open_ports()
                      << ", Closed: " << host_result.closed_ports()
                      << ", Filtered: " << host_result.filtered_ports() << "\n";
        };

        // Create scanner
        Scanner scanner(config, callbacks);

        // Run the scan (blocks until complete)
        ScanSummary summary = scanner.run();

        // Print formatted results
        print_results(summary);

        // Verify Phase 5 constraints
        if (summary.total_hosts() != config.targets.size()) {
            std::cerr << "ERROR: Expected " << config.targets.size()
                      << " hosts, got " << summary.total_hosts() << "\n";
            return 1;
        }

        const std::size_t expected_total_ports =
            config.targets.size() * config.ports.size();

        if (summary.total_ports() != expected_total_ports) {
            std::cerr << "ERROR: Expected " << expected_total_ports
                      << " total ports, got " << summary.total_ports() << "\n";
            return 1;
        }

        // Verify each host has correct number of ports
        for (const auto& host : summary.hosts) {
            if (host.total_ports() != config.ports.size()) {
                std::cerr << "ERROR: Host " << host.host
                          << " has " << host.total_ports()
                          << " ports, expected " << config.ports.size() << "\n";
                return 1;
            }
        }

        std::cout << "✓ Phase 5 test completed successfully.\n";
        std::cout << "✓ Scanned " << summary.total_hosts() << " hosts.\n";
        std::cout << "✓ Total ports: " << summary.total_ports() << "\n";
        std::cout << "✓ Total open: " << summary.total_open_ports() << "\n";
        std::cout << "✓ Duration: " << summary.duration().count() << " ms\n\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
