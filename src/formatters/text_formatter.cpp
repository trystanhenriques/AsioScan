#include "formatters/text_formatter.hpp"

#include "formatters/OutputOptions.hpp"
#include "result/scan_summary.hpp"
#include "result/host_result.hpp"
#include "result/port_result.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>

namespace asioscan {

namespace {

/*
 * Convert PortState to human-readable string.
 */
const char* port_state_to_string(PortState state) {
    switch (state) {
        case PortState::Open:     return "Open";
        case PortState::Closed:   return "Closed";
        case PortState::Filtered: return "Filtered";
        case PortState::Error:    return "Error";
        default:                  return "Unknown";
    }
}

/*
 * Format duration in milliseconds to human-readable string.
 */
std::string format_duration(std::chrono::milliseconds ms) {
    const auto seconds = ms.count() / 1000.0;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << seconds << "s";
    return oss.str();
}

/*
 * Format time_point to readable timestamp string.
 */
std::string format_timestamp(std::chrono::steady_clock::time_point tp) {
    const auto system_time = std::chrono::system_clock::now() +
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            tp - std::chrono::steady_clock::now()
        );
    
    const std::time_t time = std::chrono::system_clock::to_time_t(system_time);
    std::tm tm_buf;
    
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/*
 * Print scan header section.
 */
void print_header(const ScanSummary& summary) {
    std::cout << "========================================\n";
    std::cout << "AsioScan Scan Report\n";
    std::cout << "========================================\n\n";
    
    std::cout << "Scan start time: " << format_timestamp(summary.start_time) << "\n";
    std::cout << "Scan end time:   " << format_timestamp(summary.end_time) << "\n";
    std::cout << "Scan duration:   " << format_duration(summary.duration()) << "\n\n";
}

/*
 * Print per-host section with port table.
 */
void print_host(const HostResult& host, bool show_reason) {
    std::cout << "----------------------------------------\n";
    std::cout << "Host: " << host.host << "\n";
    std::cout << "Host scan duration: " << host.duration().count() << " ms\n\n";
    
    if (show_reason) {
        std::cout << std::left
                  << std::setw(8) << "PORT"
                  << " " << std::setw(12) << "STATE"
                  << " REASON\n";
    } else {
        std::cout << std::left
                  << std::setw(8) << "PORT"
                  << " STATE\n";
    }
    
    for (const auto& port : host.ports) {
        std::cout << std::left << std::setw(8) << port.port
                  << " " << std::setw(12) << port_state_to_string(port.state);
        
        if (show_reason) {
            std::cout << " " << port.reason;
        }
        
        std::cout << "\n";
    }
    
    std::cout << "\n";
}

/*
 * Print scan summary footer.
 */
void print_summary(const ScanSummary& summary) {
    std::cout << "========================================\n";
    std::cout << "Scan Summary\n";
    std::cout << "========================================\n\n";
    
    std::cout << "Hosts scanned: " << summary.total_hosts() << "\n";
    std::cout << "Ports scanned: " << summary.total_ports() << "\n";
    std::cout << "Open ports:    " << summary.total_open_ports() << "\n";
    std::cout << "Total duration: " << format_duration(summary.duration()) << "\n";
}

/*
 * Print quiet mode output.
 * Outputs only open ports in minimal format.
 */
void print_quiet(const ScanSummary& summary) {
    for (const auto& host : summary.hosts) {
        for (const auto& port : host.ports) {
            if (port.state == PortState::Open) {
                std::cout << host.host << ":" << port.port << " open\n";
            }
        }
    }
}

/*
 * Print summary-only mode output.
 * Outputs high-level scan statistics without per-host or per-port details.
 * A host is considered "up" if it has at least one open port.
 */
void print_summary_only(const ScanSummary& summary) {
    std::cout << "Hosts scanned: " << summary.total_hosts() << "\n";
    std::cout << "Hosts up: " << summary.hosts_up() << "\n";
    std::cout << "Ports scanned: " << summary.total_ports() << "\n";
    std::cout << "Open ports: " << summary.total_open_ports() << "\n";
    std::cout << "Scan duration: " << format_duration(summary.duration()) << "\n";
}

} // anonymous namespace

/*
 * TextFormatter::print
 * --------------------
 * Canonical Normal mode text output implementation.
 *
 * Formats the scan results into a human-readable text report with
 * a header, per-host sections, and a summary footer.
 */
void TextFormatter::print(const ScanSummary& summary, const OutputOptions& options) {
    if (options.text_mode == TextMode::Quiet) {
        print_quiet(summary);
        return;
    }
    
    if (options.text_mode == TextMode::Summary) {
        print_summary_only(summary);
        return;
    }
    
    print_header(summary);
    
    for (const auto& host : summary.hosts) {
        print_host(host, options.show_reason);
    }
    
    print_summary(summary);
}

} // namespace asioscan
