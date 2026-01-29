#include "scanner/scanner.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/post.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <stdexcept>
#include <utility>
#include <queue>
#include <memory>
#include <unordered_map>
#include <vector>

namespace asioscan {

/* ============================================================
 * Scanner::Impl
 * ============================================================
 *
 * Private implementation of the Scanner engine.
 *
 * Phase 6 Architecture:
 * --------------------
 * This implementation extends Phase 5's multi-host bounded-concurrency
 * scanner to support GRACEFUL CANCELLATION.
 *
 * Key concepts:
 *  1. Cooperative Cancellation: Cancellation is requested, not forced
 *  2. No New Work: When cancelled, no new tasks are scheduled
 *  3. Active Cancellation: In-flight sockets and timers are cancelled
 *  4. Safe Completion: Handlers may still fire after cancellation
 *  5. Partial Results: HostResults are finalized with whatever was completed
 *  6. Clean Shutdown: io_context exits gracefully
 *
 * What happens when cancel() is called:
 * ------------------------------------
 * 1. cancel_requested_ flag is set to true
 * 2. All tracked in-flight sockets are cancelled
 * 3. All tracked in-flight timers are cancelled
 * 4. No new tasks are scheduled from task_queue_
 * 5. Existing async handlers complete (may see operation_aborted)
 * 6. When in_flight_count_ reaches 0:
 *    - Finalize any incomplete hosts
 *    - Allow io_context.run() to return
 *
 * What happens to in-flight operations:
 * ------------------------------------
 * - Sockets: socket->cancel() is called
 *   -> async_connect completion handler fires with operation_aborted
 * - Timers: timer->cancel() is called
 *   -> async_wait completion handler fires with operation_aborted
 * - Both handlers check cancel_requested_ and act accordingly
 * - Handlers still call on_task_complete to maintain invariants
 *
 * How partial results are preserved:
 * ----------------------------------
 * - PortResults are appended as they complete (even during cancellation)
 * - When cancellation completes:
 *   -> Hosts with pending_ports > 0 are finalized
 *   -> Their end_time is set
 *   -> They are moved to completed_hosts_
 * - ScanSummary contains all completed and partially-completed hosts
 *
 * Cancellation invariants:
 * -----------------------
 * - cancel_requested_ can only transition false -> true (never backwards)
 * - After cancellation, schedule_next_task() does nothing
 * - in_flight_count_ still tracks active operations accurately
 * - Hosts still complete exactly once
 * - No double-completion of ports or hosts
 * - No use-after-free or dangling references
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
         * Phase 6 cancellation behavior:
         *  - If cancel() is called during execution:
         *    -> In-flight operations are cancelled
         *    -> No new operations are scheduled
         *    -> io_context.run() returns when in_flight_count_ == 0
         *  - Partial results are preserved and returned
         */
        io_context_.run();

        /*
         * Finalize any incomplete hosts.
         *
         * If cancellation occurred, some hosts may still be in host_states_
         * with pending_ports > 0. We need to finalize them and move them
         * to completed_hosts_ so they appear in the ScanSummary.
         *
         * This ensures partial results are never lost.
         */
        finalize_incomplete_hosts();

        /*
         * Finalize scan summary.
         *
         * All HostResult objects have been moved to summary.hosts
         * by the per-host completion handlers.
         */
        summary.end_time = std::chrono::steady_clock::now();
        summary.hosts = std::move(completed_hosts_);

        return summary;
    }

    // Request cancellation
    void cancel() noexcept {
        /*
         * Phase 6 Cancellation Entry Point
         * ---------------------------------
         * This function is called from outside (potentially from a signal handler
         * in the CLI layer, or from another thread in future phases).
         *
         * Safety requirements:
         *  - Must be safe to call at any time
         *  - Must be safe to call multiple times (idempotent)
         *  - Must not throw
         *  - Must not block
         *
         * Actions taken:
         *  1. Set cancellation flag (prevents new work)
         *  2. Cancel all tracked in-flight sockets
         *  3. Cancel all tracked in-flight timers
         *  4. Post completion detection task to io_context
         *
         * Note: We do NOT stop io_context directly. We let the natural
         * completion of handlers drain in_flight_count_ to zero.
         */

        // Idempotency: only cancel once
        if (cancel_requested_) {
            return;
        }

        cancel_requested_ = true;

        /*
         * Cancel all active sockets.
         *
         * For each in-flight socket:
         *  - socket->cancel() causes async_connect to complete with operation_aborted
         *  - The completion handler will see cancel_requested_ == true
         *  - The handler will still call on_task_complete (to maintain invariants)
         *
         * Why copy the vector first?
         *  - active_sockets_ may be modified during cancellation
         *  - Copying ensures iterator stability
         */
        std::vector<std::shared_ptr<boost::asio::ip::tcp::socket>> sockets_to_cancel;
        sockets_to_cancel.reserve(active_sockets_.size());
        for (const auto& socket_weak : active_sockets_) {
            if (auto socket = socket_weak.lock()) {
                sockets_to_cancel.push_back(socket);
            }
        }

        for (auto& socket : sockets_to_cancel) {
            boost::system::error_code ec;
            socket->cancel(ec);
            // Ignore errors: socket may already be closed/cancelled
        }

        /*
         * Cancel all active timers.
         *
         * For each in-flight timer:
         *  - timer->cancel() causes async_wait to complete with operation_aborted
         *  - The completion handler will see cancel_requested_ == true
         *  - The handler will still call on_task_complete (to maintain invariants)
         */
        std::vector<std::shared_ptr<boost::asio::steady_timer>> timers_to_cancel;
        timers_to_cancel.reserve(active_timers_.size());
        for (const auto& timer_weak : active_timers_) {
            if (auto timer = timer_weak.lock()) {
                timers_to_cancel.push_back(timer);
            }
        }

        for (auto& timer : timers_to_cancel) {
            // cancel() returns the number of cancelled operations
            timer->cancel();
            // Ignore errors: timer may already be expired/cancelled
        }

        /*
         * Post a check to finalize if needed.
         *
         * If cancellation happens when in_flight_count_ == 0
         * (e.g., between tasks), we need to ensure io_context exits.
         *
         * Posting a completion check ensures we don't hang.
         */
        boost::asio::post(io_context_, [this]() {
            check_cancellation_complete();
        });
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
        /*
         * Phase 6 Cancellation Check
         * --------------------------
         * If cancellation has been requested, do NOT schedule new tasks.
         *
         * This is the primary mechanism that stops new work from starting
         * during cancellation.
         *
         * Existing in-flight tasks will complete naturally (or be cancelled),
         * but no new tasks are pulled from the queue.
         */
        if (cancel_requested_) {
            return; // Cancellation requested: stop scheduling
        }

        if (task_queue_.empty()) {
            return;
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

        /*
         * Phase 6 Enhancement: Track active async operations
         * --------------------------------------------------
         * Store weak_ptr references to sockets and timers so we can
         * cancel them if cancel() is called.
         *
         * Why weak_ptr?
         *  - Doesn't extend lifetime
         *  - Allows handlers to own the objects
         *  - Prevents use-after-free during cancellation
         */
        active_sockets_.push_back(socket);
        active_timers_.push_back(timer);

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
         * Phase 6 Enhancement:
         *  - Handler checks cancel_requested_ after completion
         *  - If cancelled, operation_aborted is expected
         *  - Handler still calls on_task_complete to maintain invariants
         */
        boost::asio::async_connect(
            *socket,
            endpoints,
            [this, socket, timer, result, completed, start_time, host]
            (const error_code& ec, const tcp::endpoint&) {
                if (*completed) {
                    return;
                }
                *completed = true;

                timer->cancel();

                const auto end_time = std::chrono::steady_clock::now();
                result->latency = std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(end_time - start_time);

                /*
                 * Phase 6 Cancellation Handling in Connect Handler
                 * ------------------------------------------------
                 * If cancellation occurred:
                 *  - ec will likely be operation_aborted
                 *  - We still classify the result (as Error)
                 *  - We still call on_task_complete to maintain counts
                 *
                 * This ensures cancellation doesn't break invariants.
                 */
                if (cancel_requested_ && ec == boost::asio::error::operation_aborted) {
                    result->state = PortState::Error;
                    result->reason = "Cancelled";
                } else if (!ec) {
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

                boost::system::error_code close_ec;
                socket->close(close_ec);

                on_task_complete(host, *result);
            }
        );

        /*
         * Initiate timer expiration.
         *
         * Phase 6 Enhancement: Same cancellation handling as connect handler
         */
        timer->async_wait(
            [this, socket, timer, result, completed, start_time, host]
            (const error_code& ec) {
                if (*completed) {
                    return;
                }
                *completed = true;

                if (ec == boost::asio::error::operation_aborted) {
                    // Timer was cancelled (either by connect success or by cancel())
                    return;
                }

                socket->cancel();

                const auto end_time = std::chrono::steady_clock::now();
                result->latency = std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(end_time - start_time);

                result->state = PortState::Filtered;
                result->reason = "Timeout";

                boost::system::error_code close_ec;
                socket->close(close_ec);

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
            // Host already finalized (can happen during cancellation)
            --in_flight_count_;
            check_cancellation_complete();
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
         * Phase 6 Enhancement: Check cancellation completion
         * --------------------------------------------------
         * If cancellation was requested and in_flight_count_ is now 0,
         * we need to finalize incomplete hosts and stop the io_context.
         *
         * Otherwise, continue normal scheduling.
         */
        if (cancel_requested_) {
            check_cancellation_complete();
        } else {
            if (!task_queue_.empty()) {
                schedule_next_task();
            }
        }
    }

    /*
     * check_cancellation_complete
     * ---------------------------
     * Called when cancellation is active and an operation completes.
     *
     * Checks if all in-flight operations have finished.
     * If so, finalizes incomplete hosts and stops the io_context.
     *
     * This ensures cancellation completes gracefully without hanging.
     */
    void check_cancellation_complete() {
        if (!cancel_requested_) {
            return; // Not in cancellation mode
        }

        if (in_flight_count_ > 0) {
            return; // Still have active operations
        }

        /*
         * All in-flight operations have completed.
         * Finalize any incomplete hosts and let io_context exit.
         *
         * Note: We don't explicitly stop io_context here.
         * Since in_flight_count_ == 0 and no new work is scheduled,
         * io_context.run() will naturally return.
         */
        finalize_incomplete_hosts();
    }

    /*
     * finalize_incomplete_hosts
     * -------------------------
     * Finalizes any hosts that still have pending_ports > 0.
     *
     * This happens in two scenarios:
     *  1. Cancellation occurred before all ports finished
     *  2. run() is ending and we want to ensure all hosts are accounted for
     *
     * For each incomplete host:
     *  - Set end_time to now
     *  - Move HostResult to completed_hosts_
     *  - Invoke on_host_complete callback (if registered)
     *
     * This ensures partial results are never lost.
     */
    void finalize_incomplete_hosts() {
        /*
         * Copy host keys first to avoid iterator invalidation
         * (we're modifying host_states_ during iteration).
         */
        std::vector<std::string> incomplete_hosts;
        incomplete_hosts.reserve(host_states_.size());

        for (const auto& [host, state] : host_states_) {
            incomplete_hosts.push_back(host);
        }

        /*
         * Finalize each incomplete host.
         */
        for (const auto& host : incomplete_hosts) {
            auto it = host_states_.find(host);
            if (it == host_states_.end()) {
                continue; // Already finalized
            }

            HostState& state = it->second;

            // Set end time
            state.result.end_time = std::chrono::steady_clock::now();

            // Invoke callback
            if (callbacks_.on_host_complete) {
                callbacks_.on_host_complete(state.result);
            }

            // Move to completed list
            completed_hosts_.push_back(std::move(state.result));

            // Remove from active tracking
            host_states_.erase(it);
        }
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
     * Phase 6 Addition: Active operation tracking
     * -------------------------------------------
     * We track weak_ptr references to all in-flight sockets and timers
     * so we can cancel them when cancel() is called.
     *
     * Why vector of weak_ptr?
     *  - Allows cancellation without extending lifetimes
     *  - Handlers own the shared_ptr (RAII cleanup)
     *  - We can iterate and cancel safely
     *
     * Cleanup:
     *  - Expired weak_ptrs are harmless (lock() returns nullptr)
     *  - We don't explicitly clean up this vector (not necessary)
     *  - Memory overhead is minimal (one weak_ptr per in-flight operation)
     */
    std::vector<std::weak_ptr<boost::asio::ip::tcp::socket>> active_sockets_;
    std::vector<std::weak_ptr<boost::asio::steady_timer>> active_timers_;

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
