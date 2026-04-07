#include "formatters/xml_formatter.hpp"
#include "formatters/OutputOptions.hpp"
#include "result/scan_summary.hpp"
#include "result/host_result.hpp"
#include "result/port_result.hpp"
#include "config/scan_config.hpp"
#include <chrono>
#include <iomanip>
#include <ostream>
#include <string>

namespace asioscan {

namespace {
    std::string escape_xml(const std::string& input) {
        std::string output;
        output.reserve(input.size());
        for (char c : input) {
            // Strip invalid XML 1.0 control characters
            if (static_cast<unsigned char>(c) < 0x20 && c != '\t' && c != '\n' && c != '\r') {
                continue;
            }

            switch (c) {
                case '&':  output += "&amp;"; break;
                case '<':  output += "&lt;"; break;
                case '>':  output += "&gt;"; break;
                case '"':  output += "&quot;"; break;
                case '\'': output += "&apos;"; break;
                default:   output += c; break;
            }
        }
        return output;
    }

    const char* port_state_to_string_lower(PortState state) {
        switch (state) {
            case PortState::Open:     return "open";
            case PortState::Closed:   return "closed";
            case PortState::Filtered: return "filtered";
            case PortState::Error:    return "error";
            default:                  return "unknown";
        }
    }
}

void XmlFormatter::print(std::ostream& out,
                         const ScanSummary& summary,
                         const OutputOptions& /*options*/) {
    out << "<?xml version=\"1.0\"?>\n";
    out << "<asioscan version=\"0.1.0\">\n";
    
    if (summary.config) {
        out << "  <scaninfo type=\"connect\"";
        out << " timeout=\"" << summary.config->timeout.count() << "\" unit=\"ms\"";
        out << " concurrency=\"" << summary.config->max_concurrency << "\"";
        
        if (!summary.config->ports.empty()) {
            out << " ports=\"";
            for (size_t i = 0; i < summary.config->ports.size(); ++i) {
                out << summary.config->ports[i];
                if (i + 1 < summary.config->ports.size()) {
                    out << ",";
                }
            }
            out << "\"";
        }
        out << " />\n";
    }

    for (const auto& host : summary.hosts) {
        out << "  <host>\n";
        out << "    <address addr=\"" << escape_xml(host.host) << "\"/>\n";
        
        if (!host.ports.empty()) {
            out << "    <ports>\n";
            for (const auto& port : host.ports) {
                out << "      <port protocol=\"tcp\" portid=\"" << port.port << "\">\n";
                out << "        <state state=\"" << port_state_to_string_lower(port.state) 
                    << "\" reason=\"" << escape_xml(port.reason) << "\"/>\n";
                out << "      </port>\n";
            }
            out << "    </ports>\n";
        }
        
        out << "  </host>\n";
    }
    
    double duration_s = std::chrono::duration_cast<std::chrono::milliseconds>(
                            summary.end_time - summary.start_time
                        ).count() / 1000.0;
                        
    std::size_t filtered_count = 0;
    std::size_t closed_count = 0;
    for (const auto& h : summary.hosts) {
        for (const auto& p : h.ports) {
            if (p.state == PortState::Filtered) {
                filtered_count++;
            } else if (p.state == PortState::Closed) {
                closed_count++;
            }
        }
    }

    out << "  <runstats up=\"" << summary.hosts_up() 
        << "\" down=\"" << summary.hosts_down()
        << "\" total=\"" << summary.total_hosts() 
        << "\" ports-scanned=\"" << summary.total_ports() 
        << "\" open=\"" << summary.total_open_ports() 
        << "\" closed=\"" << closed_count
        << "\" filtered=\"" << filtered_count 
        << "\" duration=\"" << std::fixed << std::setprecision(2) << duration_s 
        << "\" unit=\"seconds\"/>\n";

    out << "</asioscan>\n";
}

} // namespace asioscan
