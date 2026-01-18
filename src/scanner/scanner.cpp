#include "scanner/scanner.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <stdexcept>
#include <utility>
#include <queue>
#include <memory>

namespace asioscan {

/* ============================================================
 * Scanner::Impl
 * ============================================================
 *
 * Private implementation of the Scanner engine.
 *
 * Phase 4 Architecture:
 * --------------------
 * This implementation introduces bounded-concurrency port scanning
 * for a SINGLE host with MULTIPLE ports.
 *
 * Key concepts:
 *  1. Port Queue: All ports to scan are enqueued at start
 *  2. In-flight Counter: Tracks active async operations
 *  3. Max Concurrency: Limits simultaneous connection attempts
 *  4. Completion-Driven Scheduling: When a port scan finishes,
 *     the next port is automatically scheduled
 *  5. Single-Threaded: No threads, mutexes, or atomics
 *
 * Why a queue?
 * -----------
 * - We need to scan potentially thousands of ports
 * - We cannot launch all scans simultaneously (resource limits)
 * - A queue allows us to control the "flow" of work
 * - Completed scans pull new work from the queue
 *
 * How bounded concurrency works:
 * -----------------------------
 * - At start: launch up to `max_concurrency` scans
 * - When ANY scan completes:
 *   -> decrement in-flight counter
 *   -> if queue not empty: schedule next port
 *   -> if queue empty AND in-flight == 0: scanning done
 * - This creates a "sliding window" of active scans
 *
 * Why this is concurrent but single-threaded:
 * ------------------------------------------
 * - Concurrency != Parallelism
 * - Multiple async I/O operations are in-flight simultaneously
 * - The OS kernel handles actual I/O (connect, timeout)
 * - io_context multiplexes completion events on ONE thread
 * - No locking needed: all state mutations happen in handlers
 */
class Scanner::Impl {
public:
    explicit Impl(ScanConfig config, ScannerCallbacks callbacks)
        : config_(std::move(config)),
          callbacks_(std::move(callbacks)),
          io_context_() {}

    // Non-copyable
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    // Non-movable (io_context is non-movable)
    Impl(Impl&&) noexcept = delete;
    Impl& operator=(Impl&&) noexcept = delete;

    ~Impl() = default;

    // Entry point for scan execution
    ScanSummary run() {
        /*
         * Phase 4 Requirements:
         * - Exactly ONE host (multi-host is Phase 5+)
         * - Multiple ports (Phase 4 enhancement)
         * - Bounded concurrency (NEW)
         */
        if (config_.targets.size() != 1) {
            throw std::invalid_argument(
                "Phase 4 requires exactly one target host"
            );
        }
        if (config_.ports.empty()) {
            throw std::invalid_argument(
                "No ports specified for scanning"
            );
        }

        ScanSummary summary;
        summary.config = config_;
        summary.start_time = std::chrono::steady_clock::now();

        const std::string& target_host = config_.targets[0];

        // Prepare host result container
        current_host_result_ = HostResult{};
        current_host_result_.host = target_host;
        current_host_result_.start_time = std::chrono::steady_clock::now();

        /*
         * Build the port queue.
         *
         * All ports are enqueued at the start. As scans complete,
         * ports are dequeued and new scans are initiated.
         *
         * This queue-based approach allows us to:
         *  - Control memory usage (limited in-flight operations)
         *  - Respect system resource limits
         *  - Implement sophisticated scheduling policies later
         */
        for (std::uint16_t port : config_.ports) {
            port_queue_.push(port);
        }

        /*
         * Initialize concurrency tracking.
         *
         * Invariants maintained throughout execution:
         *  - in_flight_count_ >= 0
         *  - in_flight_count_ <= max_concurrency
         *  - When queue is empty AND in_flight_count_ == 0, scan is done
         */
        in_flight_count_ = 0;
        const std::size_t max_concurrency = config_.max_concurrency;

        /*
         * Kick-start the scan by launching initial batch.
         *
         * We schedule up to `max_concurrency` port scans immediately.
         * Each scan, when it completes, will schedule the next one
         * (if ports remain in the queue).
         *
         * This creates a self-sustaining "conveyor belt" of work.
         */
        const std::size_t initial_batch = std::min(
            max_concurrency,
            port_queue_.size()
        );

        for (std::size_t i = 0; i < initial_batch; ++i) {
            schedule_next_port(target_host);
        }

        /*
         * Run the event loop.
         *
         * This blocks until:
         *  - All ports have been scanned
         *  - All async operations have completed
         *  - io_context has no more work
         *
         * The loop exits when the last in-flight scan completes
         * and sees that both the queue is empty AND in_flight_count_ == 0.
         */
        io_context_.run();

        /*
         * Finalize host result.
         *
         * All PortResult objects have been appended to
         * current_host_result_.ports by the completion handlers.
         */
        current_host_result_.end_time = std::chrono::steady_clock::now();

        // Notify host completion callback
        if (callbacks_.on_host_complete) {
            callbacks_.on_host_complete(current_host_result_);
        }

        // Attach host to summary
        summary.hosts.push_back(std::move(current_host_result_));
        summary.end_time = std::chrono::steady_clock::now();

        return summary;
    }

    // Request cancellation
    void cancel() noexcept {
        cancel_requested_ = true;
    }

    const ScanConfig& config() const noexcept {
        return config_;
    }

private:
    /*
     * schedule_next_port
     * ------------------
     * Dequeues the next port (if available) and initiates an async scan.
     *
     * This function is the heart of the bounded-concurrency mechanism:
     *  - Called at startup to fill the initial "window"
     *  - Called by completion handlers to keep the window full
     *  - Respects max_concurrency automatically (caller's responsibility)
     *
     * Preconditions:
     *  - port_queue_ is not empty
     *  - in_flight_count_ < max_concurrency
     */
    void schedule_next_port(const std::string& host) {
        if (port_queue_.empty()) {
            return; // No more work
        }

        if (cancel_requested_) {
            return; // User requested cancellation
        }

        // Dequeue next port
        std::uint16_t port = port_queue_.front();
        port_queue_.pop();

        // Increment in-flight counter
        ++in_flight_count_;

        // Launch async scan for this port
        scan_port_async(host, port);
    }

    /*
     * scan_port_async
     * ---------------
     * Initiates an asynchronous TCP connect attempt with timeout.
     *
     * This is the Phase 3 logic, refactored to:
     *  - Accept host/port as parameters
     *  - Store result in shared state (current_host_result_)
     *  - Call on_port_complete when done
     *
     * Key difference from Phase 3:
     *  - Does NOT call io_context_.run() or io_context_.restart()
     *  - Those are managed by the top-level run() function
     *  - Multiple instances of this function run concurrently
     */
    void scan_port_async(const std::string& host, std::uint16_t port) {
        using boost::asio::ip::tcp;
        using boost::system::error_code;

        /*
         * Allocate socket and timer on the heap.
         *
         * Why heap allocation?
         *  - These objects must outlive this function scope
         *  - They are owned by their completion handlers
         *  - shared_ptr ensures automatic cleanup when both handlers complete
         *
         * This is a standard Boost.Asio pattern for managing
         * per-operation state in async code.
         */
        auto socket = std::make_shared<tcp::socket>(io_context_);
        auto timer = std::make_shared<boost::asio::steady_timer>(
            io_context_,
            config_.timeout
        );

        // Result will be populated by completion handlers
        auto result = std::make_shared<PortResult>();
        result->port = port;

        // Latency measurement
        const auto start_time = std::chrono::steady_clock::now();

        /*
         * Completion guard.
         *
         * Shared between both handlers to prevent double-processing.
         * The first handler to execute sets this to true.
         */
        auto completed = std::make_shared<bool>(false);

        /*
         * Resolve hostname to endpoints.
         *
         * For Phase 4, we continue using synchronous resolution.
         * This is acceptable because:
         *  - Resolution is per-host, not per-port
         *  - We only support one host in Phase 4
         *  - Async resolution will be added in later phases
         */
        tcp::resolver resolver(io_context_);
        error_code resolve_ec;

        auto endpoints = resolver.resolve(
            host,
            std::to_string(port),
            resolve_ec
        );

        if (resolve_ec) {
            // Resolution failed
            result->state = PortState::Error;
            result->reason = "Resolution failed: " + resolve_ec.message();
            result->latency = std::chrono::milliseconds(0);
            on_port_complete(host, *result);
            return;
        }

        /*
         * Initiate async_connect.
         *
         * Captures: socket, timer, result, completed, start_time, host
         * All captured by value (shared_ptr) to extend lifetime.
         */
        boost::asio::async_connect(
            *socket,
            endpoints,
            [this, socket, timer, result, completed, start_time, host]
            (const error_code& ec, const tcp::endpoint&) {
                // Guard: only the first completion is processed
                if (*completed) {
                    return;
                }
                *completed = true;

                // Cancel the timer (best-effort)
                timer->cancel();

                // Stop latency measurement
                const auto end_time = std::chrono::steady_clock::now();
                result->latency = std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(end_time - start_time);

                // Classify connection outcome
                if (!ec) {
                    result->state = PortState::Open;
                    result->reason = "Connection established";
                } else if (ec == boost::asio::error::connection_refused) {
                    result->state = PortState::Closed;
                    result->reason = "Connection refused";
                } else if (ec == boost::asio::error::operation_aborted) {
                    result->state = PortState::Filtered;
                    result->reason = "Timeout";
                } else {
                    result->state = PortState::Error;
                    result->reason = ec.message();
                }

                // Notify completion
                on_port_complete(host, *result);
            }
        );

        /*
         * Initiate timer expiration.
         *
         * Captures: socket, timer, result, completed, start_time, host
         */
        timer->async_wait(
            [this, socket, timer, result, completed, start_time, host]
            (const error_code& ec) {
                // Guard: only the first completion is processed
                if (*completed) {
                    return;
                }
                *completed = true;

                // If timer was cancelled, do nothing
                if (ec == boost::asio::error::operation_aborted) {
                    return;
                }

                // Timer fired: cancel the socket
                socket->cancel();

                // Stop latency measurement
                const auto end_time = std::chrono::steady_clock::now();
                result->latency = std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(end_time - start_time);

                // Classify as filtered
                result->state = PortState::Filtered;
                result->reason = "Timeout";

                // Notify completion
                on_port_complete(host, *result);
            }
        );
    }

    /*
     * on_port_complete
     * ----------------
     * Called when a single port scan finishes (success, timeout, or error).
     *
     * Responsibilities:
     *  1. Store the result in current_host_result_
     *  2. Invoke user callback (if registered)
     *  3. Decrement in-flight counter
     *  4. Schedule the next port (if queue not empty)
     *  5. Detect completion (queue empty + in-flight == 0)
     *
     * This function is the "glue" that drives the bounded-concurrency loop.
     * It ensures that as soon as one scan finishes, another begins
     * (until we run out of ports).
     */
    void on_port_complete(const std::string& host, const PortResult& result) {
        // Store result
        current_host_result_.ports.push_back(result);

        // Invoke user callback
        if (callbacks_.on_port_result) {
            callbacks_.on_port_result(host, result);
        }

        // Decrement in-flight counter
        --in_flight_count_;

        /*
         * Schedule next port (if available).
         *
         * This is the key to bounded concurrency:
         *  - We just freed up one "slot" in the concurrency window
         *  - If more ports remain, fill that slot immediately
         *  - This maintains a steady flow of `max_concurrency` scans
         */
        if (!port_queue_.empty()) {
            schedule_next_port(host);
        }

        /*
         * Completion detection.
         *
         * The scan is complete when:
         *  - port_queue_ is empty (no more work to schedule)
         *  - in_flight_count_ == 0 (all scheduled work has finished)
         *
         * When this condition is met, io_context.run() will naturally
         * return because there are no more pending async operations.
         */
        // (No explicit action needed; io_context will exit automatically)
    }

    // Immutable snapshot of scan configuration
    ScanConfig config_;

    // Optional progress callbacks
    ScannerCallbacks callbacks_;

    // Cancellation flag
    bool cancel_requested_ = false;

    /*
     * Port queue: all ports waiting to be scanned.
     *
     * Invariants:
     *  - Populated at start of run()
     *  - Dequeued by schedule_next_port()
     *  - Empty when all ports have been scheduled
     */
    std::queue<std::uint16_t> port_queue_;

    /*
     * In-flight counter: number of active async port scans.
     *
     * Invariants:
     *  - Incremented by schedule_next_port()
     *  - Decremented by on_port_complete()
     *  - Always <= max_concurrency
     *  - Zero when scan is complete
     */
    std::size_t in_flight_count_ = 0;

    /*
     * Current host result (accumulator).
     *
     * As port scans complete, PortResult objects are appended
     * to current_host_result_.ports.
     *
     * This is safe because all mutations happen on the same thread
     * (within io_context handlers).
     */
    HostResult current_host_result_;

    /*
     * Boost.Asio execution context.
     *
     * Phase 4 changes:
     *  - io_context.run() is called ONCE at the top level
     *  - No restart() needed (all async operations are initiated upfront)
     *  - Automatically exits when queue is empty and in-flight == 0
     */
    boost::asio::io_context io_context_;
};

/* ============================================================
 * Scanner public interface
 * ============================================================ */

Scanner::Scanner(ScanConfig config, ScannerCallbacks callbacks)
    : impl_(std::make_unique<Impl>(std::move(config), std::move(callbacks))) {}

Scanner::Scanner(Scanner&& other) noexcept = default;
Scanner& Scanner::operator=(Scanner&& other) noexcept = default;

Scanner::~Scanner() = default;

ScanSummary Scanner::run() {
    return impl_->run();
}

void Scanner::cancel() noexcept {
    impl_->cancel();
}

const ScanConfig& Scanner::config() const noexcept {
    return impl_->config();
}

} // namespace asioscan
