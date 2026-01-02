#pragma once

/*
 * ScanSummary
 * -----------
 * Represents the global outcome of an entire scan run.
 *
 * This is the top-level data structure produced by the scanner engine
 * and consumed by output formatters (text, XML, etc.).
 *
 */

#include "config/scan_config.hpp"
#include "result/host_result.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

namespace asioscan {

/*
 * ScanSummary
 * -----------
 * Immutable record describing scan-wide metadata and aggregated results.
 *
 * This struct intentionally contains no behavior beyond lightweight
 * aggregation helpers. All policy and interpretation belongs elsewhere.
 */
struct ScanSummary {
    /* ------------------------------------------------------------
     * Scan configuration (optional)
     * ------------------------------------------------------------ */

    // Configuration used to perform the scan.
    // Stored as an optional copy to allow:
    //  - Output formatters to reference scan parameters
    //  - Future export modes (JSON/XML metadata)
    //
    // This is optional to avoid forcing ownership if not needed.
    std::optional<ScanConfig> config;

    /* ------------------------------------------------------------
     * Per-host results
     * ------------------------------------------------------------ */

    // Results for each scanned host.
    std::vector<HostResult> hosts;

    /* ------------------------------------------------------------
     * Timing information
     * ------------------------------------------------------------ */

    // Time at which the scan began.
    std::chrono::steady_clock::time_point start_time;

    // Time at which the scan completed.
    std::chrono::steady_clock::time_point end_time;

    /* ------------------------------------------------------------
     * Lightweight semantic helpers
     * ------------------------------------------------------------ */

    // Returns the total number of hosts scanned.
    std::size_t total_hosts() const noexcept {
        return hosts.size();
    }

    // Returns the total number of ports scanned across all hosts.
    std::size_t total_ports() const noexcept {
        std::size_t count = 0;
        for (const auto& host : hosts) {
            count += host.total_ports();
        }
        return count;
    }

    // Returns the total number of open ports across all hosts.
    std::size_t total_open_ports() const noexcept {
        std::size_t count = 0;
        for (const auto& host : hosts) {
            count += host.open_ports();
        }
        return count;
    }

    // Returns the number of hosts with at least one open port.
    std::size_t hosts_up() const noexcept {
        std::size_t count = 0;
        for (const auto& host : hosts) {
            if (host.open_ports() > 0) {
                ++count;
            }
        }
        return count;
    }

    // Returns the number of hosts with no open ports detected.
    std::size_t hosts_down() const noexcept {
        return total_hosts() - hosts_up();
    }

    // Returns the total elapsed time of the scan.
    std::chrono::milliseconds duration() const noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        );
    }

    // Returns true if any port resulted in an error state.
    bool has_errors() const noexcept {
        for (const auto& host : hosts) {
            if (host.error_ports() > 0) {
                return true;
            }
        }
        return false;
    }
};

} // namespace asioscan
