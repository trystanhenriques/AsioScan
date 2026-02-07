#include "formatters/text_formatter.hpp"
#include "formatters/OutputOptions.hpp"
#include "result/scan_summary.hpp"
#include "result/host_result.hpp"
#include "result/port_result.hpp"

#include <chrono>
#include <iostream>
#include <string>

/*
 * Comprehensive test for TextFormatter Normal and Quiet modes.
 *
 * This test manually constructs a ScanSummary with realistic data
 * to demonstrate both canonical and quiet text output formats.
 */

using namespace asioscan;

int main() {
    try {
        // Create scan summary with multiple hosts
        ScanSummary summary;
        
        const auto scan_start = std::chrono::steady_clock::now();
        summary.start_time = scan_start;
        
        // First host: scanme.nmap.org
        HostResult host1;
        host1.host = "scanme.nmap.org";
        host1.start_time = scan_start;
        
        PortResult h1_port1;
        h1_port1.port = 22;
        h1_port1.state = PortState::Open;
        h1_port1.latency = std::chrono::milliseconds(42);
        h1_port1.reason = "Connection established";
        host1.ports.push_back(h1_port1);
        
        PortResult h1_port2;
        h1_port2.port = 80;
        h1_port2.state = PortState::Closed;
        h1_port2.latency = std::chrono::milliseconds(15);
        h1_port2.reason = "Connection refused";
        host1.ports.push_back(h1_port2);
        
        PortResult h1_port3;
        h1_port3.port = 443;
        h1_port3.state = PortState::Open;
        h1_port3.latency = std::chrono::milliseconds(38);
        h1_port3.reason = "Connection established";
        host1.ports.push_back(h1_port3);
        
        PortResult h1_port4;
        h1_port4.port = 8080;
        h1_port4.state = PortState::Filtered;
        h1_port4.latency = std::chrono::milliseconds(500);
        h1_port4.reason = "Timeout";
        host1.ports.push_back(h1_port4);
        
        host1.end_time = scan_start + std::chrono::milliseconds(550);
        summary.hosts.push_back(host1);
        
        // Second host: example.com
        HostResult host2;
        host2.host = "example.com";
        host2.start_time = scan_start + std::chrono::milliseconds(600);
        
        PortResult h2_port1;
        h2_port1.port = 80;
        h2_port1.state = PortState::Open;
        h2_port1.latency = std::chrono::milliseconds(25);
        h2_port1.reason = "Connection established";
        host2.ports.push_back(h2_port1);
        
        PortResult h2_port2;
        h2_port2.port = 443;
        h2_port2.state = PortState::Open;
        h2_port2.latency = std::chrono::milliseconds(30);
        h2_port2.reason = "Connection established";
        host2.ports.push_back(h2_port2);
        
        PortResult h2_port3;
        h2_port3.port = 22;
        h2_port3.state = PortState::Filtered;
        h2_port3.latency = std::chrono::milliseconds(500);
        h2_port3.reason = "Timeout";
        host2.ports.push_back(h2_port3);
        
        PortResult h2_port4;
        h2_port4.port = 3306;
        h2_port4.state = PortState::Closed;
        h2_port4.latency = std::chrono::milliseconds(10);
        h2_port4.reason = "Connection refused";
        host2.ports.push_back(h2_port4);
        
        host2.end_time = scan_start + std::chrono::milliseconds(1100);
        summary.hosts.push_back(host2);
        
        summary.end_time = scan_start + std::chrono::milliseconds(1100);
        
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
        
        std::cout << "\n\n";
        
        // Test 3: Quiet mode
        std::cout << "=== Test 3: Quiet Mode ===\n\n";
        
        OutputOptions options3;
        options3.format = OutputFormat::Text;
        options3.text_mode = TextMode::Quiet;
        options3.show_reason = false;
        
        formatter.print(summary, options3);
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
