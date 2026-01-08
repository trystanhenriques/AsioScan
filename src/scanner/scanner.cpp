#include "scanner/scanner.hpp"



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
          callbacks_(std::move(callbacks)) {}

    // Non-copyable
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    // Movable
    Impl(Impl&&) noexcept = default;
    Impl& operator=(Impl&&) noexcept = default;

    ~Impl() = default;

    // Entry point for scan execution
    ScanSummary run() {
        ScanSummary summary;
        summary.config = config_;

        // Record scan start time
        summary.start_time = std::chrono::steady_clock::now();

        /*
         * Iterate over configured target hosts.
         *
         * At this stage:
         *  - No ports are scanned
         *  - Port vectors remain empty
         *  - Host timing is still meaningful (setup overhead)
         */
        for (const auto& host_name : config_.targets) {
            HostResult host_result;
            host_result.host = host_name;

            // Host scan lifecycle (even with no ports)
            host_result.start_time = std::chrono::steady_clock::now();
            host_result.end_time   = host_result.start_time;

            // ports vector intentionally left empty

            summary.hosts.push_back(std::move(host_result));

            // Optional callback notification
            if (callbacks_.on_host_complete) {
                callbacks_.on_host_complete(summary.hosts.back());
            }
        }

        // Record scan end time
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
    // Immutable snapshot of scan configuration
    ScanConfig config_;

    // optional progress callbacks
    ScannerCallbacks callbacks_;

    // Cancellation flag (checked by async logic)
    bool cancel_requested_ = false;

    /*
     * Future members (intentionally not implemented yet):
     *
     * - boost::asio::io_context
     * - task queues
     * - in-flight counters
     * - host aggregation structures
     * - timers and sockets
     */
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
