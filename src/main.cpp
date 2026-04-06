#include "cli/cli_parser.hpp"
#include "scanner/scanner.hpp"
#include "formatters/text_formatter.hpp"

#include <atomic>
#include <csignal>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

namespace {

std::atomic_bool g_interrupt_requested{false};

void handle_interrupt_signal(int) {
    g_interrupt_requested.store(true, std::memory_order_relaxed);
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::signal(SIGINT, handle_interrupt_signal);
#ifdef SIGTERM
        std::signal(SIGTERM, handle_interrupt_signal);
#endif

        auto parsed = asioscan::parse_cli(argc, argv);

        if (parsed.status != asioscan::ParseStatus::Ok) {
            return parsed.exit_code;
        }

        asioscan::Scanner scanner(std::move(parsed.config));

        std::atomic_bool scan_finished{false};
        std::thread cancel_watcher([&scanner, &scan_finished]() {
            bool cancel_sent = false;

            while (!scan_finished.load(std::memory_order_relaxed)) {
                if (g_interrupt_requested.load(std::memory_order_relaxed) && !cancel_sent) {
                    std::cerr << "\nInterrupt received. Cancelling scan...\n";
                    scanner.cancel();
                    cancel_sent = true;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        });

        auto stop_watcher = [&]() {
            scan_finished.store(true, std::memory_order_relaxed);
            if (cancel_watcher.joinable()) {
                cancel_watcher.join();
            }
        };

        asioscan::ScanSummary summary;
        try {
            summary = scanner.run();
        } catch (...) {
            stop_watcher();
            throw;
        }

        stop_watcher();

        asioscan::TextFormatter formatter;

        std::ofstream output_file;
        std::ostream* output_stream = &std::cout;

        if (parsed.output.output_file.has_value()) {
            output_file.open(*parsed.output.output_file, std::ios::out | std::ios::trunc);
            if (!output_file.is_open()) {
                std::cerr << "Error: failed to open output file: "
                          << *parsed.output.output_file << "\n";
                return 1;
            }
            output_stream = &output_file;
        }

        formatter.print(*output_stream, summary, parsed.output);

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
