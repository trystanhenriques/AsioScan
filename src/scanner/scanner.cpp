#include "scanner/scanner.hpp"



#include <utility>   // std::move

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

    // Entry point for scan execution (stub for Phase 1)
    ScanSummary run() {
        ScanSummary summary;
        summary.config = config_;

        // NOTE:
        // start_time / end_time will be set in Phase 2.
        // hosts will be populated in later phases.

        return summary;
    }

    // Request cancellation (stub for Phase 1)
    void cancel() noexcept {
        cancel_requested_ = true;
    }

    const ScanConfig& config() const noexcept {
        return config_;
    }

private:
    // Immutable snapshot of scan configuration
    ScanConfig config_;

    // Optional progress callbacks
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
