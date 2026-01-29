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
#include <unordered_map>

namespace asioscan {

/* ============================================================
 * Scanner::Impl
 * ============================================================
 *
 * Private implementation of the Scanner engine.
 *
 * Phase 5 Architecture:
 * --------------------
 * This implementation extends Phase 4's bounded-concurrency
 * port scanning to support MULTIPLE hosts with MULTIPLE ports.
 *
 * Key concepts:
 *  1. Task Queue: All (host, port) pairs enqueued at start
 *  2. In-flight Counter: Tracks active async operations GLOBALLY
 *  3. Max Concurrency: Limits simultaneous connections across ALL hosts
 *  4. Per-Host State: Tracks completion for each host independently
 *  5. Completion-Driven Scheduling: When any task finishes,
 *     the next task is automatically scheduled
 *  6. Single-Threaded: Still no threads, mutexes, or atomics
 *
 * Why per-host state?
 * ------------------
 * - Each host must produce an independent HostResult
 * - Host completion happens when ALL its ports finish
 * - We need to track:
 *   → Which ports belong to which host
 *   → How many ports per host are still pending
 *   → When to finalize each HostResult
 *
 * How host completion is detected:
 * --------------------------------
 * - Each host has a "pending ports" counter
 * - When a port scan completes:
 *   → Append PortResult to host's result vector
 *   → Decrement host's pending counter
 *   → If pending reaches 0: host is complete
 *     → Record end_time
 *     → Invoke on_host_complete callback
 *     → Move HostResult to ScanSummary
 *
 * Global task queue vs per-host queues:
 * -------------------------------------
 * - We use ONE global queue of (host, port) tasks
 * - This simplifies concurrency management
 * - Bounded concurrency applies globally (not per-host)
 * - This is correct because:
 *   → Resource limits (file descriptors) are global
 *   → Network bandwidth is shared across all targets
 *   → Fair scheduling emerges naturally from queue order
 *
 * Why this builds on Phase 4:
 * ---------------------------
 * - Phase 4 gave us: port queue + bounded concurrency
 * - Phase 5 generalizes: task queue (host+port) + per-host tracking
 * - Same async_connect + steady_timer logic
 * - Same completion-driven scheduling
 * - Same single-threaded io_context
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
         * Phase 5 Requirements:
         * - One or more hosts (Phase 5 enhancement)
         * - Multiple ports per host
         * - Bounded concurrency across ALL hosts
         */
        if (config_.targets.empty()) {
            throw std::invalid_argument(
                "No target hosts specified for scanning"
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

        /*
         * Initialize per-host state.
         *
         * For each target host, we create a HostState object that:
         *  - Stores the accumulating HostResult
         *  - Tracks how many ports are still pending
         *  - Allows us to detect host completion
         */
        for (const auto& host : config_.targets) {
            HostState state;
            state.result.host = host;
            state.result.start_time = std::chrono::steady_clock::now();
            state.pending_ports = config_.ports.size();

            host_states_[host] = std::move(state);
        }

        /*
         * Build the global task queue.
         *
         * Each task is a (host, port) pair.
         * All tasks are enqueued at the start in a predictable order:
         *  - Outer loop: hosts (in config order)
         *  - Inner loop: ports (in config order)
         *
         * This creates fair round-robin-like scheduling when
         * tasks are dequeued.
         */
        for (const auto& host : config_.targets) {
            for (std::uint16_t port : config_.ports) {
                task_queue_.push({host, port});
            }
        }

        /*
         * Initialize global concurrency tracking.
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
         * We schedule up to `max_concurrency` tasks immediately.
         * Each task, when it completes, will schedule the next one
         * (if tasks remain in the queue).
         *
         * This creates a self-sustaining "conveyor belt" of work
         * across ALL hosts.
         */
        const std::size_t initial_batch = std::min(
            max_concurrency,
            task_queue_.size()
        );

        for (std::size_t i = 0; i < initial_batch; ++i) {
            schedule_next_task();
        }

        /*
         * Run the event loop.
         *
         * This blocks until:
         *  - All tasks have been scanned
         *  - All async operations have completed
         *  - io_context has no more work
         *
         * The loop exits when the last in-flight scan completes
         * and sees that both the queue is empty AND in_flight_count_ == 0.
         */
        io_context_.run();

        /*
         * Finalize scan summary.
         *
         * All HostResult objects have been moved to summary.hosts
         * by the per-host completion handlers.
         */
        summary.end_time = std::chrono::steady_clock::now();

        // Move completed hosts into summary
        summary.hosts = std::move(completed_hosts_);

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
     * ScanTask
     * --------
     * Represents a single (host, port) scan operation.
     *
     * This is the atomic unit of work in Phase 5.
     * The task queue contains these, and they are dequeued
     * one at a time as async operations complete.
     */
    struct ScanTask {
        std::string host;
        std::uint16_t port;
    };

    /*
     * HostState
     * ---------
     * Per-host state for tracking scan progress and aggregating results.
     *
     * Why we need this:
     *  - HostResult must contain ALL PortResults for a host
     *  - Host completion happens when pending_ports reaches 0
     *  - We cannot know completion time until all ports finish
     *
     * Invariants:
     *  - pending_ports starts at config_.ports.size()
     *  - pending_ports decrements on each port completion
     *  - When pending_ports == 0: host is complete
     *  - result.ports grows as scans complete
     */
    struct HostState {
        HostResult result;              // Accumulating result
        std::size_t pending_ports = 0;  // Remaining ports to scan
    };

    /*
     * schedule_next_task
     * ------------------
     * Dequeues the next (host, port) task and initiates an async scan.
     *
     * This function is the heart of the bounded-concurrency mechanism:
     *  - Called at startup to fill the initial "window"
     *  - Called by completion handlers to keep the window full
     *  - Works across ALL hosts (not per-host)
     *
     * Preconditions:
     *  - task_queue_ is not empty
     *  - in_flight_count_ < max_concurrency
     */
    void schedule_next_task() {
        if (task_queue_.empty()) {
            return; // No more work
        }

        if (cancel_requested_) {
            return; // User requested cancellation
        }

        // Dequeue next task
        ScanTask task = task_queue_.front();
        task_queue_.pop();

        // Increment global in-flight counter
        ++in_flight_count_;

        // Launch async scan for this (host, port)
        scan_port_async(task.host, task.port);
    }

    /*
     * scan_port_async
     * ---------------
     * Initiates an asynchronous TCP connect attempt with timeout.
     *
     * This is the same Phase 3/4 logic, now adapted to:
     *  - Accept host/port as parameters
     *  - Store result in per-host state (host_states_[host])
     *  - Call on_task_complete when done
     *
     * Key difference from Phase 4:
     *  - Result is stored in host_states_[host].result.ports
     *  - Host completion is checked in on_task_complete
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
         * For Phase 5, we continue using synchronous resolution.
         * This is acceptable because:
         *  - Resolution happens per-task (once per host per scan_port_async call)
         *  - Async resolution will be added in later phases
         *  - The performance impact is acceptable for v1
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
            on_task_complete(host, *result);
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

                // Close the socket (best-effort)
                boost::system::error_code close_ec;
                socket->close(close_ec);

                // Notify completion
                on_task_complete(host, *result);
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

                // Close the socket (best-effort)
                boost::system::error_code close_ec;
                socket->close(close_ec);

                // Notify completion
                on_task_complete(host, *result);
            }
        );
    }

    /*
     * on_task_complete
     * ----------------
     * Called when a single (host, port) scan finishes.
     *
     * Responsibilities:
     *  1. Store the result in host_states_[host]
     *  2. Invoke on_port_result callback (if registered)
     *  3. Decrement the host's pending_ports counter
     *  4. Check if host is complete (pending_ports == 0)
     *  5. If host complete:
     *     - Record host end_time
     *     - Invoke on_host_complete callback
     *     - Move HostResult to completed_hosts_
     *  6. Decrement global in-flight counter
     *  7. Schedule the next task (if queue not empty)
     *
     * This function is the "glue" that:
     *  - Drives bounded concurrency
     *  - Detects per-host completion
     *  - Maintains global scan progress
     */
    void on_task_complete(const std::string& host, const PortResult& result) {
        // Lookup host state
        auto it = host_states_.find(host);
        if (it == host_states_.end()) {
            // This should never happen; indicates a logic error
            return;
        }

        HostState& state = it->second;

        // Store port result
        state.result.ports.push_back(result);

        // Invoke per-port callback
        if (callbacks_.on_port_result) {
            callbacks_.on_port_result(host, result);
        }

        // Decrement pending ports for this host
        --state.pending_ports;

        /*
         * Host completion detection.
         *
         * A host is complete when ALL its ports have finished.
         * This is the ONLY place where host completion is detected.
         *
         * When a host completes:
         *  - Record end_time
         *  - Invoke on_host_complete callback
         *  - Move HostResult to completed_hosts_
         *  - Remove HostState from host_states_
         *
         * Why move to completed_hosts_?
         *  - Preserves insertion order for ScanSummary
         *  - Prevents accidental re-access of completed hosts
         *  - Makes it easy to assemble final ScanSummary
         */
        if (state.pending_ports == 0) {
            // Host is complete
            state.result.end_time = std::chrono::steady_clock::now();

            // Invoke host completion callback
            if (callbacks_.on_host_complete) {
                callbacks_.on_host_complete(state.result);
            }

            // Move to completed list (preserves order)
            completed_hosts_.push_back(std::move(state.result));

            // Remove from active tracking
            host_states_.erase(it);
        }

        // Decrement global in-flight counter
        --in_flight_count_;

        /*
         * Schedule next task (if available).
         *
         * This is the key to bounded concurrency:
         *  - We just freed up one "slot" in the concurrency window
         *  - If more tasks remain, fill that slot immediately
         *  - This maintains a steady flow of `max_concurrency` scans
         *    across ALL hosts
         */
        if (!task_queue_.empty()) {
            schedule_next_task();
        }

        /*
         * Global completion detection.
         *
         * The scan is complete when:
         *  - task_queue_ is empty (no more work to schedule)
         *  - in_flight_count_ == 0 (all scheduled work has finished)
         *
         * When this condition is met, io_context.run() will naturally
         * return because there are no more pending async operations.
         *
         * At that point:
         *  - All HostResults are in completed_hosts_
         *  - host_states_ should be empty
         *  - run() will assemble the final ScanSummary
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
     * Task queue: all (host, port) pairs waiting to be scanned.
     *
     * Invariants:
     *  - Populated at start of run()
     *  - Dequeued by schedule_next_task()
     *  - Empty when all tasks have been scheduled
     */
    std::queue<ScanTask> task_queue_;

    /*
     * In-flight counter: number of active async operations GLOBALLY.
     *
     * Invariants:
     *  - Incremented by schedule_next_task()
     *  - Decremented by on_task_complete()
     *  - Always <= max_concurrency
     *  - Zero when scan is complete
     */
    std::size_t in_flight_count_ = 0;

    /*
     * Per-host state tracking.
     *
     * Maps host -> HostState for all hosts currently being scanned.
     *
     * Invariants:
     *  - Populated at start of run()
     *  - Entries removed when host completes
     *  - Empty when scan is complete
     *
     * Why unordered_map?
     *  - Fast O(1) lookup by host name
     *  - Handles arbitrary number of hosts
     *  - Order doesn't matter here (completed_hosts_ preserves order)
     */
    std::unordered_map<std::string, HostState> host_states_;

    /*
     * Completed hosts (in order of appearance in config).
     *
     * HostResults are moved here when their host completes.
     *
     * Invariants:
     *  - Starts empty
     *  - Grows as hosts complete
     *  - Final size == config_.targets.size()
     *  - Order matches config_.targets (roughly, unless completion order differs)
     *
     * Why a vector?
     *  - Preserves completion order
     *  - Easy to move into ScanSummary.hosts
     *  - No lookup needed
     */
    std::vector<HostResult> completed_hosts_;

    /*
     * Boost.Asio execution context.
     *
     * Phase 5 changes:
     *  - Still called ONCE at the top level
     *  - Still no restart() needed
     *  - Automatically exits when task_queue_ empty and in_flight_count_ == 0
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
