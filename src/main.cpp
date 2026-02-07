#include "formatters/text_formatter.hpp"
#include "formatters/OutputOptions.hpp"
#include "result/scan_summary.hpp"
#include "result/host_result.hpp"
#include "result/port_result.hpp"

#include <chrono>
#include <iostream>
#include <string>

/*
 * Minimal test for TextFormatter Normal mode output.
 *
 * This test manually constructs a ScanSummary with realistic data
 * to demonstrate the canonical text output format.
 */

using namespace asioscan;

int main() {
    try {
        // Create scan summary
        ScanSummary summary;
        
        const auto scan_start = std::chrono::steady_clock::now();
        summary.start_time = scan_start;
        
        // Create host result
        HostResult host;
        host.host = "scanme.nmap.org";
        host.start_time = scan_start;
        
        // Add port results with different states
        PortResult port1;
        port1.port = 22;
        port1.state = PortState::Open;
        port1.latency = std::chrono::milliseconds(42);
        port1.reason = "Connection established";
        host.ports.push_back(port1);
        
        PortResult port2;
        port2.port = 80;
        port2.state = PortState::Closed;
        port2.latency = std::chrono::milliseconds(15);
        port2.reason = "Connection refused";
        host.ports.push_back(port2);
        
        PortResult port3;
        port3.port = 443;
        port3.state = PortState::Open;
        port3.latency = std::chrono::milliseconds(38);
        port3.reason = "Connection established";
        host.ports.push_back(port3);
        
        PortResult port4;
        port4.port = 8080;
        port4.state = PortState::Filtered;
        port4.latency = std::chrono::milliseconds(500);
        port4.reason = "Timeout";
        host.ports.push_back(port4);
        
        host.end_time = scan_start + std::chrono::milliseconds(550);
        
        summary.hosts.push_back(host);
        summary.end_time = scan_start + std::chrono::milliseconds(550);
        
        // Test 1: Normal output without reasons
        std::cout << "=== Test 1: Normal Output (no reasons) ===\n\n";
        
        OutputOptions options1;
        options1.format = OutputFormat::Text;
        options1.text_mode = TextMode::Normal;
        options1.show_reason = false;
        
        TextFormatter formatter;
        formatter.print(summary, options1);
        
        std::cout << "\n\n";
        
        // Test 2: Normal output with reasons
        std::cout << "=== Test 2: Normal Output (with reasons) ===\n\n";
        
        OutputOptions options2;
        options2.format = OutputFormat::Text;
        options2.text_mode = TextMode::Normal;
        options2.show_reason = true;
        
        formatter.print(summary, options2);
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
