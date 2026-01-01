#pragma once

/*
 * PortResult
 * ----------
 * Represents the outcome of scanning a single TCP port on a single host.
 *
 * PortResult is produced by the scanner engine and consumed by
 * host aggregation and output formatters.
 */

#include <chrono>
#include <cstdint>
#include <string>

namespace asioscan {

/*
 * PortState
 * ---------
 * High-level classification of a TCP port after a scan attempt.
 *
 * These states are intentionally minimal and stable.
 * They map cleanly to common scanner terminology
 */
enum class PortState {
    Open,        // Connection succeeded
    Closed,      // Connection actively refused
    Filtered,    // No response / timeout (likely firewall)
    Error        // Unexpected error (DNS, network, internal)
};

/*
 * PortResult
 * ----------
 * Immutable record describing the result of scanning one TCP port.
 *
 * This struct is intentionally transparent and data-only.
 * It does not attempt to interpret policy or user intent.
 */
struct PortResult {
    /* ------------------------------------------------------------
     * Identification
     * ------------------------------------------------------------ */

    // TCP port number that was scanned.
    std::uint16_t port{0};

    // Final observed state of the port.
    PortState state{PortState::Error};

    /* ------------------------------------------------------------
     * Timing information
     * ------------------------------------------------------------ */

    // Duration of the scan attempt for this port.
    // This includes connection time and any timeout delay.
    std::chrono::milliseconds latency{0};

    /* ------------------------------------------------------------
     * Diagnostic information
     * ------------------------------------------------------------ */

    // Human-readable reason for the observed state.
    // Examples:
    //  - "connection refused"
    //  - "timeout"
    //  - "network unreachable"
    //
    // This field is optional in output and primarily useful for
    // verbose or diagnostic modes.
    std::string reason{};

    /* ------------------------------------------------------------
     * Lightweight semantic helpers
     * ------------------------------------------------------------ */

    // Returns true if the port was detected as open.
    bool is_open() const noexcept {
        return state == PortState::Open;
    }

    // Returns true if the port was detected as closed.
    bool is_closed() const noexcept {
        return state == PortState::Closed;
    }

    // Returns true if the port was filtered (no response).
    bool is_filtered() const noexcept {
        return state == PortState::Filtered;
    }

    // Returns true if the scan encountered an unexpected error.
    bool is_error() const noexcept {
        return state == PortState::Error;
    }
};

} // namespace asioscan
