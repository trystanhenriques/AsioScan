#pragma once

/*
 * HostResult
 * ----------
 * Represents the aggregate result of scanning all requested ports
 * on a single target host.
 *
 * HostResult groups multiple PortResult objects and provides
 * per-host summary information that higher layers can consume.
 */

#include "result/port_result.hpp"

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace asioscan {

/*
 * HostResult
 * ----------
 * Immutable record describing the scan outcome for a single host.
 *
 * A host may be specified by hostname or IP address; resolution
 * details are intentionally kept out of this structure.
 */
struct HostResult {
    /* ------------------------------------------------------------
     * Identification
     * ------------------------------------------------------------ */

    // Host identifier as provided by the user (hostname or IP).
    std::string host;

    /* ------------------------------------------------------------
     * Per-port results
     * ------------------------------------------------------------ */

    // Results for each scanned TCP port on this host.
    std::vector<PortResult> ports;

    /* ------------------------------------------------------------
     * Timing information
     * ------------------------------------------------------------ */

    // Time at which scanning of this host began.
    std::chrono::steady_clock::time_point start_time;

    // Time at which scanning of this host completed.
    std::chrono::steady_clock::time_point end_time;

    /* ------------------------------------------------------------
     * Lightweight semantic helpers
     * ------------------------------------------------------------ */

    // Returns the total number of ports scanned for this host.
    std::size_t total_ports() const noexcept {
        return ports.size();
    }

    // Returns the number of ports detected as open.
    std::size_t open_ports() const noexcept {
        return count_by_state(PortState::Open);
    }

    // Returns the number of ports detected as closed.
    std::size_t closed_ports() const noexcept {
        return count_by_state(PortState::Closed);
    }

    // Returns the number of ports detected as filtered.
    std::size_t filtered_ports() const noexcept {
        return count_by_state(PortState::Filtered);
    }

    // Returns the number of ports that resulted in an error.
    std::size_t error_ports() const noexcept {
        return count_by_state(PortState::Error);
    }

    // Returns the total elapsed time spent scanning this host.
    std::chrono::milliseconds duration() const noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        );
    }

private:
    // Counts the number of ports in a given state.
    std::size_t count_by_state(PortState state) const noexcept {
        std::size_t count = 0;
        for (const auto& port : ports) {
            if (port.state == state) {
                ++count;
            }
        }
        return count;
    }
};

} // namespace asioscan
