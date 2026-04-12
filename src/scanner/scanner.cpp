#include "scanner/scanner.hpp"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <asio/connect.hpp>
#include <asio/post.hpp>
#include <asio/error.hpp>

#include <chrono>
#include <stdexcept>
#include <utility>
#include <queue>
#include <memory>
#include <unordered_map>
#include <vector>

namespace asioscan {

/*
 * Scanner::Impl
 * =============
 *
 * Private implementation of the TCP connect scanner engine.
 *
 * Architecture:
 * ------------
 * - Single-threaded asynchronous I/O using one io_context
 * - Bounded-concurrency task queue across all hosts
 * - Per-host result aggregation with completion detection
 * - Cooperative cancellation with partial result preservation
 *
 * Concurrency Model:
 * -----------------
 * Multiple async operations (TCP connects + timeouts) execute concurrently,
 * but all completion handlers run sequentially on a single thread.
 * This eliminates the need for locks while achieving high throughput.
 *
 * Task Scheduling:
 * ---------------
 * Tasks are (host, port) pairs enqueued at startup. A sliding window of
 * up to `max_concurrency` operations is maintained. When any operation
 * completes, the next task is scheduled, keeping the window full until
 * the queue is exhausted.
 *
 * Host Completion:
 * ---------------
 * Each host tracks pending port counts. When a host's pending count
 * reaches zero, its HostResult is finalized and moved to the completed list.
 * This ensures accurate timing and prevents accidental re-access.
 *
 * Cancellation:
 * ------------
 * Cancellation is cooperative: new tasks are not scheduled, and in-flight
 * sockets/timers are cancelled. Completion handlers may still execute,
 * but check the cancellation flag and avoid further scheduling. Incomplete
 * hosts are finalized when all in-flight operations drain.
 *
 * Invariants Maintained:
 * ---------------------
 * - in_flight_count <= max_concurrency
 * - Each port result belongs to exactly one host
 * - Each host completes exactly once
 * - No scheduling occurs after cancellation is requested
 * - io_context.run() returns only when queue is empty AND in_flight == 0
 */
class Scanner::Impl {
public:
    explicit Impl(ScanConfig config, ScannerCallbacks callbacks)
        : config_(std::move(config)),
          callbacks_(std::move(callbacks)),
          io_context_() {}

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) noexcept = delete;
    Impl& operator=(Impl&&) noexcept = delete;

    ~Impl() = default;

    ScanSummary run() {
        validate_config();

        ScanSummary summary;
        summary.config = config_;
        summary.start_time = std::chrono::steady_clock::now();

        initialize_host_states();
        build_task_queue();
        schedule_initial_batch();

        // Block until all tasks complete or cancellation drains
        io_context_.run();

        // Capture any partial results if cancelled early
        finalize_incomplete_hosts();

        summary.end_time = std::chrono::steady_clock::now();
        summary.hosts = std::move(completed_hosts_);

        return summary;
    }

    void cancel() noexcept {
        if (cancel_requested_) {
            return; // Idempotent
        }

        cancel_requested_ = true;

        cancel_active_sockets();
        cancel_active_timers();

        // Ensure io_context exits if no operations are in-flight
        asio::post(io_context_.get_executor(), [this]() {
            finalize_if_cancelled_and_idle();
        });
    }

    const ScanConfig& config() const noexcept {
        return config_;
    }

private:
    /*
     * Internal Types
     * ==============
     */

    struct ScanTask {
        std::string host;
        std::uint16_t port;
    };

    struct HostState {
        HostResult result;
        std::size_t pending_ports = 0;
    };

    /*
     * Initialization
     * ==============
     */

    void validate_config() {
        if (config_.targets.empty()) {
            throw std::invalid_argument("No target hosts specified");
        }
        if (config_.ports.empty()) {
            throw std::invalid_argument("No ports specified");
        }
    }

    void initialize_host_states() {
        for (const auto& host : config_.targets) {
            HostState state;
            state.result.host = host;
            state.result.start_time = std::chrono::steady_clock::now();
            state.pending_ports = config_.ports.size();

            host_states_[host] = std::move(state);
        }
    }

    void build_task_queue() {
        // Enqueue all (host, port) pairs for fair scheduling
        for (const auto& host : config_.targets) {
            for (std::uint16_t port : config_.ports) {
                task_queue_.push({host, port});
            }
        }
    }

    void schedule_initial_batch() {
        const std::size_t batch_size = std::min(
            config_.max_concurrency,
            task_queue_.size()
        );

        for (std::size_t i = 0; i < batch_size; ++i) {
            schedule_next_task();
        }
    }

    /*
     * Task Scheduling
     * ===============
     */

    void schedule_next_task() {
        // Respect cancellation: stop pulling new work
        if (cancel_requested_) {
            return;
        }

        if (task_queue_.empty()) {
            return;
        }

        ScanTask task = task_queue_.front();
        task_queue_.pop();

        ++in_flight_count_;

        scan_port_async(task.host, task.port);
    }

    /*
     * Async Port Scanning
     * ===================
     */

    void scan_port_async(const std::string& host, std::uint16_t port) {
        using asio::ip::tcp;
        using asio::error_code;

        // Allocate per-operation resources
        auto socket = std::make_shared<tcp::socket>(io_context_.get_executor());
        auto timer = std::make_shared<asio::steady_timer>(
            io_context_.get_executor(),
            config_.timeout
        );

        // Track for cancellation
        active_sockets_.push_back(socket);
        active_timers_.push_back(timer);

        auto result = std::make_shared<PortResult>();
        result->port = port;

        const auto start_time = std::chrono::steady_clock::now();

        // Prevent double-completion from timer and connect handlers
        auto completed = std::make_shared<bool>(false);

        // Synchronous resolution (acceptable for v1; async in future)
        tcp::resolver resolver(io_context_.get_executor());
        error_code resolve_ec;

        auto endpoints = resolver.resolve(
            host,
            std::to_string(port),
            resolve_ec
        );

        if (resolve_ec) {
            result->state = PortState::Error;
            result->reason = "Resolution failed: " + resolve_ec.message();
            result->latency = std::chrono::milliseconds(0);
            on_task_complete(host, *result);
            return;
        }

        // Launch async connect
        asio::async_connect(
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

                classify_connect_result(ec, *result);

                error_code close_ec;
                socket->close(close_ec);

                on_task_complete(host, *result);
            }
        );

        // Launch timeout timer
        timer->async_wait(
            [this, socket, timer, result, completed, start_time, host]
            (const error_code& ec) {
                if (*completed) {
                    return;
                }
                *completed = true;

                // Timer cancelled by connect success
                if (ec == asio::error::operation_aborted) {
                    return;
                }

                socket->cancel();

                const auto end_time = std::chrono::steady_clock::now();
                result->latency = std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(end_time - start_time);

                result->state = PortState::Filtered;
                result->reason = "Timeout";

                error_code close_ec;
                socket->close(close_ec);

                on_task_complete(host, *result);
            }
        );
    }

    void classify_connect_result(const asio::error_code& ec,
                                  PortResult& result) {
        if (cancel_requested_ && ec == asio::error::operation_aborted) {
            result.state = PortState::Error;
            result.reason = "Cancelled";
        } else if (!ec) {
            result.state = PortState::Open;
            result.reason = "Connection established";
        } else if (ec == asio::error::connection_refused) {
            result.state = PortState::Closed;
            result.reason = "Connection refused";
        } else if (ec == asio::error::operation_aborted) {
            result.state = PortState::Filtered;
            result.reason = "Timeout";
        } else {
            result.state = PortState::Error;
            result.reason = ec.message();
        }
    }

    /*
     * Completion Handling
     * ===================
     */

    void on_task_complete(const std::string& host, const PortResult& result) {
        auto it = host_states_.find(host);
        if (it == host_states_.end()) {
            // Host already finalized (rare edge case during cancellation)
            --in_flight_count_;
            finalize_if_cancelled_and_idle();
            return;
        }

        HostState& state = it->second;

        // Append port result
        state.result.ports.push_back(result);

        // Notify callback
        if (callbacks_.on_port_result) {
            callbacks_.on_port_result(host, result);
        }

        // Check for host completion
        --state.pending_ports;
        if (state.pending_ports == 0) {
            finalize_host(it);
        }

        // Maintain concurrency window
        --in_flight_count_;

        if (cancel_requested_) {
            finalize_if_cancelled_and_idle();
        } else if (!task_queue_.empty()) {
            schedule_next_task();
        }
    }

    void finalize_host(std::unordered_map<std::string, HostState>::iterator it) {
        HostState& state = it->second;

        state.result.end_time = std::chrono::steady_clock::now();

        if (callbacks_.on_host_complete) {
            callbacks_.on_host_complete(state.result);
        }

        completed_hosts_.push_back(std::move(state.result));
        host_states_.erase(it);
    }

    /*
     * Cancellation Support
     * ====================
     */

    void cancel_active_sockets() {
        std::vector<std::shared_ptr<asio::ip::tcp::socket>> sockets;
        sockets.reserve(active_sockets_.size());

        for (const auto& weak : active_sockets_) {
            if (auto socket = weak.lock()) {
                sockets.push_back(socket);
            }
        }

        for (auto& socket : sockets) {
            asio::error_code ec;
            socket->cancel(ec);
        }
    }

    void cancel_active_timers() {
        std::vector<std::shared_ptr<asio::steady_timer>> timers;
        timers.reserve(active_timers_.size());

        for (const auto& weak : active_timers_) {
            if (auto timer = weak.lock()) {
                timers.push_back(timer);
            }
        }

        for (auto& timer : timers) {
            timer->cancel();
        }
    }

    void finalize_if_cancelled_and_idle() {
        if (!cancel_requested_) {
            return;
        }

        if (in_flight_count_ > 0) {
            return;
        }

        // All operations drained; finalize partial results
        finalize_incomplete_hosts();
    }

    void finalize_incomplete_hosts() {
        // Avoid iterator invalidation
        std::vector<std::string> incomplete;
        incomplete.reserve(host_states_.size());

        for (const auto& [host, _] : host_states_) {
            incomplete.push_back(host);
        }

        for (const auto& host : incomplete) {
            auto it = host_states_.find(host);
            if (it != host_states_.end()) {
                finalize_host(it);
            }
        }
    }

    /*
     * Member Variables
     * ================
     */

    // Configuration (immutable after construction)
    ScanConfig config_;
    ScannerCallbacks callbacks_;

    // Cancellation flag
    bool cancel_requested_ = false;

    // Task queue and concurrency tracking
    std::queue<ScanTask> task_queue_;
    std::size_t in_flight_count_ = 0;

    // Per-host state tracking
    std::unordered_map<std::string, HostState> host_states_;
    std::vector<HostResult> completed_hosts_;

    // Active operation tracking for cancellation
    std::vector<std::weak_ptr<asio::ip::tcp::socket>> active_sockets_;
    std::vector<std::weak_ptr<asio::steady_timer>> active_timers_;

    // Event loop
    asio::io_context io_context_;
};

/*
 * Scanner Public Interface
 * ========================
 */

Scanner::Scanner(ScanConfig config, ScannerCallbacks callbacks)
    : impl_(std::make_unique<Impl>(std::move(config), std::move(callbacks))) {}

Scanner::Scanner(Scanner&&) noexcept = default;
Scanner& Scanner::operator=(Scanner&&) noexcept = default;

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
