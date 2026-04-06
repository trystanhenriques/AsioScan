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
 * Convert PortState to lowercase string.
 */
const char* port_state_to_lowercase(PortState state) {
    switch (state) {
        case PortState::Open:     return "open";
        case PortState::Closed:   return "closed";
        case PortState::Filtered: return "filtered";
        case PortState::Error:    return "error";
        default:                  return "unknown";
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
void print_header(std::ostream& out, const ScanSummary& summary) {
    out << "========================================\n";
    out << "AsioScan Scan Report\n";
    out << "========================================\n\n";

    out << "Scan start time: " << format_timestamp(summary.start_time) << "\n";
    out << "Scan end time:   " << format_timestamp(summary.end_time) << "\n";
    out << "Scan duration:   " << format_duration(summary.duration()) << "\n\n";
}

/*
 * Print per-host section with port table.
 */
void print_host(std::ostream& out, const HostResult& host, bool show_reason, bool verbose) {
    out << "----------------------------------------\n";
    out << "Host: " << host.host << "\n";
    out << "Host scan duration: " << host.duration().count() << " ms\n\n";
    
    if (verbose) {
        out << std::left
            << std::setw(8) << "PORT"
            << " " << std::setw(12) << "STATE"
            << " " << std::setw(15) << "LATENCY(ms)"
            << " REASON\n";
        
        for (const auto& port : host.ports) {
            out << std::left << std::setw(8) << port.port
                << " " << std::setw(12) << port_state_to_string(port.state)
                << " " << std::setw(15) << port.latency.count()
                << " " << port.reason << "\n";
            out << "    Attempts: 1\n";
        }
    } else if (show_reason) {
        out << std::left
            << std::setw(8) << "PORT"
            << " " << std::setw(12) << "STATE"
            << " REASON\n";
        
        for (const auto& port : host.ports) {
            out << std::left << std::setw(8) << port.port
                << " " << std::setw(12) << port_state_to_string(port.state)
                << " " << port.reason << "\n";
        }
    } else {
        out << std::left
            << std::setw(8) << "PORT"
            << " STATE\n";
        
        for (const auto& port : host.ports) {
            out << std::left << std::setw(8) << port.port
                << " " << std::setw(12) << port_state_to_string(port.state)
                << "\n";
        }
    }

    out << "\n";
}

/*
 * Print scan summary footer.
 */
void print_summary(std::ostream& out, const ScanSummary& summary) {
    out << "========================================\n";
    out << "Scan Summary\n";
    out << "========================================\n\n";

    out << "Hosts scanned: " << summary.total_hosts() << "\n";
    out << "Ports scanned: " << summary.total_ports() << "\n";
    out << "Open ports:    " << summary.total_open_ports() << "\n";
    out << "Total duration: " << format_duration(summary.duration()) << "\n";
}

/*
 * Print quiet mode output.
 * Outputs only open ports in minimal format.
 */
void print_quiet(std::ostream& out, const ScanSummary& summary) {
    for (const auto& host : summary.hosts) {
        for (const auto& port : host.ports) {
            if (port.state == PortState::Open) {
                out << host.host << ":" << port.port << " open\n";
            }
        }
    }
}

/*
 * Print summary-only mode output.
 * Outputs high-level scan statistics without per-host or per-port details.
 * A host is considered "up" if it has at least one open port.
 */
void print_summary_only(std::ostream& out, const ScanSummary& summary) {
    out << "Hosts scanned: " << summary.total_hosts() << "\n";
    out << "Hosts up: " << summary.hosts_up() << "\n";
    out << "Ports scanned: " << summary.total_ports() << "\n";
    out << "Open ports: " << summary.total_open_ports() << "\n";
    out << "Scan duration: " << format_duration(summary.duration()) << "\n";
}

/*
 * Print ports-only mode output.
 * Outputs port results with minimal framing per host.
 */
void print_ports_only(std::ostream& out, const ScanSummary& summary) {
    for (std::size_t i = 0; i < summary.hosts.size(); ++i) {
        const auto& host = summary.hosts[i];
        
        out << "Host: " << host.host << "\n";
        
        for (const auto& port : host.ports) {
            out << port.port << "/tcp "
                << port_state_to_lowercase(port.state) << "\n";
        }
        
        if (i < summary.hosts.size() - 1) {
            out << "\n";
        }
    }
}

/*
 * Print hosts-only mode output.
 * Outputs one line per host with availability status and open port count.
 * Host is "up" if it has at least one port result, "down" otherwise.
 */
void print_hosts_only(std::ostream& out, const ScanSummary& summary) {
    for (const auto& host : summary.hosts) {
        const std::size_t open_count = host.open_ports();
        const bool is_up = host.total_ports() > 0;
        
        out << host.host << ": "
            << (is_up ? "up" : "down")
            << " (" << open_count << " open port"
            << (open_count == 1 ? "" : "s") << ")\n";
    }
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
void TextFormatter::print(std::ostream& out,
                          const ScanSummary& summary,
                          const OutputOptions& options) {
    if (options.text_mode == TextMode::Quiet) {
        print_quiet(out, summary);
        return;
    }
    
    if (options.text_mode == TextMode::Summary) {
        print_summary_only(out, summary);
        return;
    }
    
    if (options.text_mode == TextMode::PortsOnly) {
        print_ports_only(out, summary);
        return;
    }
    
    if (options.text_mode == TextMode::HostsOnly) {
        print_hosts_only(out, summary);
        return;
    }
    
    const bool verbose = (options.text_mode == TextMode::Verbose);
    
    print_header(out, summary);
    
    for (const auto& host : summary.hosts) {
        print_host(out, host, options.show_reason, verbose);
    }
    
    print_summary(out, summary);
}

} // namespace asioscan
