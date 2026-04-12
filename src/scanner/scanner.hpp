#pragma once

/*
 * Scanner Engine (Connect Scan, v1)
 * ---------------------------------
 * Public interface for the scanning engine.
 *
 * Design goals:
 *  - Consume a fully-formed ScanConfig
 *  - Produce a ScanSummary (data-only result model)
 *  - Perform asynchronous I/O using a single-threaded event loop internally
 *
 * The engine is intentionally built so we can later add additional scan engines
 * (e.g., OS-specific SYN scan) behind the same public interface without breaking
 * CLI/output modules.
 */

#include "config/scan_config.hpp"
#include "result/scan_summary.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace asioscan {

/*
 * ScannerCallbacks
 * ----------------
 * Optional callbacks to observe scan progress without coupling the engine
 * to any output/formatting decisions.
 *
 * Notes:
 *  - Callbacks are invoked on the scanner's thread (single-threaded model).
 *  - Keep callbacks lightweight (avoid blocking); treat them as notifications.
 *  - If you do not need progress reporting, leave callbacks empty.
 */
struct ScannerCallbacks {
    // Called when a single port finishes scanning for a host.
    // Useful for live progress updates or streaming output later.
    std::function<void(const std::string& host, const PortResult& port)> on_port_result;

    // Called when an entire host completes (all ports done).
    std::function<void(const HostResult& host)> on_host_complete;

    // Called for high-level milestones (optional).
    // Example: "Resolving host", "Starting scan", "Scan completed".
    std::function<void(const std::string& message)> on_status;
};

/*
 * Scanner
 * -------
 * Scans targets described by ScanConfig and returns a ScanSummary.
 *
 * v1 scanning method:
 *  - TCP connect scan (portable across Windows/Linux/macOS)
 *  - Asynchronous operations driven by a single-threaded event loop
 *
 * Lifetime/ownership:
 *  - The Scanner owns its internal I/O objects.
 *  - The provided ScanConfig is copied to ensure the engine operates on an immutable snapshot.
 */
class Scanner final {
public:
    // Construct a scanner with a fully configured ScanConfig.
    // The config is copied internally to enforce immutability during execution.
    explicit Scanner(ScanConfig config, ScannerCallbacks callbacks = {});

    // Non-copyable (owns internal resources).
    Scanner(const Scanner&) = delete;
    Scanner& operator=(const Scanner&) = delete;

    // Movable (allowed).
    Scanner(Scanner&&) noexcept;
    Scanner& operator=(Scanner&&) noexcept;

    // Destructor (defined in .cpp).
    ~Scanner();

    /*
     * run()
     * -----
     * Executes the scan and returns a complete ScanSummary.
     *
     * Behavior:
     *  - This is a *blocking* call from the caller’s perspective (it returns when scanning is done),
     *    but internally uses asynchronous I/O to run many connection attempts concurrently.
     *  - The function is expected to be called once per Scanner instance.
     *
     * Error handling:
     *  - Non-fatal per-port/per-host errors are recorded in results (PortState::Error + reason).
     *  - Only unrecoverable setup failures should throw exceptions.
     */
    [[nodiscard]] ScanSummary run();

    /*
     * cancel()
     * --------
     * Requests cancellation of in-flight operations.
     *
     * Notes:
     *  - Cancellation is best-effort: in-flight async operations may complete with
     *    operation_aborted or a similar cancellation outcome.
     *  - After cancellation, run() will return a partial ScanSummary containing
     *    whatever results were collected up to that point.
     */
    void cancel() noexcept;

    /*
     * Accessors
     * ---------
     * These provide read-only visibility into the scanner configuration.
     */
    [[nodiscard]] const ScanConfig& config() const noexcept;

private:
    class Impl;                     // Opaque implementation (defined in scanner.cpp)
    std::unique_ptr<Impl> impl_;    // PIMPL to keep Asio out of the public header
};

} // namespace asioscan
