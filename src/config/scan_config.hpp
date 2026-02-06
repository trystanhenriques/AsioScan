#pragma once

/*
 * ScanConfig
 * ----------
 * Immutable, data-only configuration describing a single port scan.
 *
 * This struct represents a fully validated snapshot of user intent,
 * produced by the CLI parsing layer and consumed by the scanner and
 * formatter layers.
 *
 * ScanConfig is the single source of truth for how a scan should run.
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asioscan {

/*
 * Preferred IP version for outgoing connections.
 * Auto allows the resolver / OS to choose.
 */
enum class IpVersion {
    Auto,
    IPv4,
    IPv6
};


/*
 * ScanConfig
 * ----------
 * Data-only structure describing what scan to perform and how.
 *
 * Immutability is enforced by convention: the config is built once
 * and passed as const& throughout the system.
 */
struct ScanConfig {
    /* ------------------------------------------------------------
     * Target selection
     * ------------------------------------------------------------ */

    // List of target hostnames or IP addresses to scan.
    // These are provided as strings to remain independent of DNS
    // resolution and networking libraries.
    std::vector<std::string> targets{};

    /* ------------------------------------------------------------
     * Port selection
     * ------------------------------------------------------------ */

    // Canonical list of TCP ports to scan.
    // All ranges, service names, and files should already be
    // expanded by the CLI layer before constructing this config.
    std::vector<std::uint16_t> ports{};

    // Human-readable description of how ports were selected.
    // Example: "top 100 common ports", "ports 1-1024"
    std::string port_description{};

    /* ------------------------------------------------------------
     * Timing & performance
     * ------------------------------------------------------------ */

    // Per-port timeout for connection attempts.
    // Default: 500 milliseconds.
    std::chrono::milliseconds timeout{500};

    // Maximum number of concurrent in-flight connection attempts.
    std::size_t max_concurrency{200};

    // Optional rate limit (connections per second).
    // Not required for v1, but reserved for future use.
    std::optional<std::size_t> rate_limit{};

    /* ------------------------------------------------------------
     * Network behavior
     * ------------------------------------------------------------ */

    // Preferred IP version (IPv4 / IPv6 / Auto).
    IpVersion ip_version{IpVersion::Auto};

 
    /* ------------------------------------------------------------
     * Metadata
     * ------------------------------------------------------------ */

    // Program version string (e.g., "0.1.0").
    std::string program_version{};

    // Time at which the scan was initiated.
    std::chrono::steady_clock::time_point start_time{};

    /* ------------------------------------------------------------
     * Lightweight semantic helpers
     * ------------------------------------------------------------ */

    // Returns true if exactly one target is being scanned.
    bool is_single_host() const noexcept {
        return targets.size() == 1;
    }

    // Returns true if multiple targets are being scanned.
    bool is_multi_host() const noexcept {
        return targets.size() > 1;
    }

};

} // namespace asioscan
