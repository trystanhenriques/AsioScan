#include "scanner/scanner.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <stdexcept>
#include <utility>

namespace asioscan {

/* ============================================================
 * Scanner::Impl
 * ============================================================
 *
 * Private implementation of the Scanner engine.
 *
 * Responsibilities:
 *  - Own async execution context
 *  - Schedule and execute port scan tasks
 *  - Aggregate results into ScanSummary
 *  - Support cancellation
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

    // Movable
    Impl(Impl&&) noexcept = delete;
    Impl& operator=(Impl&&) noexcept = delete;

    ~Impl() = default;

    // Entry point for scan execution
    ScanSummary run() {
        /*
         * Phase 3 enforcement:
         * This implementation supports exactly ONE host and ONE port.
         * This restriction exists to validate async correctness without
         * introducing concurrency complexity.
         */
        if (config_.targets.size() != 1) {
            throw std::invalid_argument(
                "Phase 3 requires exactly one target host"
            );
        }
        if (config_.ports.size() != 1) {
            throw std::invalid_argument(
                "Phase 3 requires exactly one port"
            );
        }

        ScanSummary summary;
        summary.config = config_;

        // Record global scan start time
        summary.start_time = std::chrono::steady_clock::now();

        // Extract the single target and port
        const std::string& target_host = config_.targets[0];
        const std::uint16_t target_port = config_.ports[0];

        // Prepare host result container
        HostResult host_result;
        host_result.host = target_host;
        host_result.start_time = std::chrono::steady_clock::now();

        // Execute the single port scan
        PortResult port_result = scan_single_port(target_host, target_port);

        // Capture host completion time
        host_result.end_time = std::chrono::steady_clock::now();

        // Attach port result to host
        host_result.ports.push_back(std::move(port_result));

        // Notify callback (if registered)
        if (callbacks_.on_host_complete) {
            callbacks_.on_host_complete(host_result);
        }

        // Attach host to summary
        summary.hosts.push_back(std::move(host_result));

        // Record global scan end time
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
     * scan_single_port
     * ----------------
     * Performs a single asynchronous TCP connect attempt with timeout.
     *
     * This function demonstrates the core async pattern:
     *  1. Create socket and timer
     *  2. Initiate async_connect (races with timer)
     *  3. Initiate async_wait on timer (races with connect)
     *  4. Whichever completes first wins
     *  5. Cancel the other operation
     *  6. Use a completion guard to prevent double-handling
     *
     * Latency measurement:
     *  - Start time: immediately before initiating async operations
     *  - End time: when the first operation completes
     *  - This accurately reflects connection establishment time or timeout duration
     *
     * Error classification:
     *  - Open: successful connect (no error)
     *  - Closed: connection_refused
     *  - Filtered: operation_aborted from timer expiration
     *  - Error: all other errors
     */
    PortResult scan_single_port(
        const std::string& host,
        std::uint16_t port
    ) {
        using boost::asio::ip::tcp;
        using boost::system::error_code;

        PortResult result;
        result.port = port;

        // Start latency measurement
        const auto start_time = std::chrono::steady_clock::now();

        /*
         * Resolve the target host to an endpoint.
         *
         * For Phase 3, we use synchronous resolution for simplicity.
         * Later phases may introduce async_resolve.
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
            result.state = PortState::Error;
            result.reason = "Resolution failed: " + resolve_ec.message();
            result.latency = std::chrono::milliseconds(0);
            return result;
        }

        /*
         * Create socket and timer.
         *
         * Both objects are owned by this function scope.
         * They will be destroyed when the function returns, which is safe
         * because io_context.run() is synchronous and completes before return.
         */
        tcp::socket socket(io_context_);
        boost::asio::steady_timer timer(io_context_, config_.timeout);

        /*
         * Completion guard.
         *
         * Prevents double-completion when both async operations fire.
         * The first operation to complete sets this to true and wins.
         * The second operation sees true and does nothing.
         *
         * This is a manual replacement for strand-based exclusion.
         */
        bool completed = false;

        /*
         * Initiate async_connect.
         *
         * This races with the timer. If connect completes first (success or error),
         * we cancel the timer. If the timer fires first, we cancel the socket.
         */
        boost::asio::async_connect(
            socket,
            endpoints,
            [&](const error_code& ec, const tcp::endpoint&) {
                // Guard: only the first completion is processed
                if (completed) {
                    return;
                }
                completed = true;

                // Cancel the timer (best-effort; may already have fired)
                timer.cancel();

                // Stop latency measurement
                const auto end_time = std::chrono::steady_clock::now();
                result.latency = std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(end_time - start_time);

                // Classify connection outcome
                if (!ec) {
                    // Successful connect
                    result.state = PortState::Open;
                    result.reason = "Connection established";
                } else if (ec == boost::asio::error::connection_refused) {
                    // Port is closed (active rejection)
                    result.state = PortState::Closed;
                    result.reason = "Connection refused";
                } else if (ec == boost::asio::error::operation_aborted) {
                    // Socket was cancelled by timer expiration
                    result.state = PortState::Filtered;
                    result.reason = "Timeout";
                } else {
                    // All other errors
                    result.state = PortState::Error;
                    result.reason = ec.message();
                }

                // Optional callback notification
                if (callbacks_.on_port_result) {
                    callbacks_.on_port_result(host, result);
                }
            }
        );

        /*
         * Initiate timer expiration.
         *
         * This races with async_connect. If the timer fires first,
         * we cancel the socket, which causes async_connect to complete
         * with operation_aborted.
         */
        timer.async_wait([&](const error_code& ec) {
            // Guard: only the first completion is processed
            if (completed) {
                return;
            }
            completed = true;

            // If timer was cancelled by successful connect, do nothing
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }

            // Timer fired: cancel the socket
            socket.cancel();

            // Stop latency measurement
            const auto end_time = std::chrono::steady_clock::now();
            result.latency = std::chrono::duration_cast<
                std::chrono::milliseconds
            >(end_time - start_time);

            // Classify as filtered (timeout)
            result.state = PortState::Filtered;
            result.reason = "Timeout";

            // Optional callback notification
            if (callbacks_.on_port_result) {
                callbacks_.on_port_result(host, result);
            }
        });

        /*
         * Run the event loop.
         *
         * This is a blocking call that processes async operations.
         * It returns when all handlers have been invoked (exactly one
         * of the two handlers above will fire and set completed = true).
         *
         * Why this works:
         *  - io_context.run() is synchronous from the caller's perspective
         *  - Internally it drives asynchronous I/O
         *  - When both operations are complete (one succeeded, one cancelled),
         *    run() returns
         *  - This allows Scanner::run() to be a synchronous API while using
         *    async I/O under the hood
         */
        io_context_.run();

        /*
         * Reset the io_context for potential future use.
         *
         * This is not needed in Phase 3 (single use), but prepares for
         * later phases where io_context may be reused across multiple ports.
         */
        io_context_.restart();

        return result;
    }

    // Immutable snapshot of scan configuration
    ScanConfig config_;

    // Optional progress callbacks
    ScannerCallbacks callbacks_;

    // Cancellation flag (checked by async logic)
    bool cancel_requested_ = false;

    /*
     * Boost.Asio execution context.
     *
     * Responsibilities:
     *  - Drive asynchronous I/O operations
     *  - Dispatch completion handlers
     *  - Manage internal state for sockets, timers, resolvers
     *
     * Lifetime:
     *  - Owned by Scanner::Impl
     *  - Created once, reused across operations (later phases)
     *  - Single-threaded model: no external thread pool
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
