#include "formatters/text_formatter.hpp"
#include "formatters/OutputOptions.hpp"
#include "result/scan_summary.hpp"
#include "result/host_result.hpp"
#include "result/port_result.hpp"

#include <CLI/CLI.hpp>
#include <chrono>
#include <iostream>
#include <string>

/*
 * Comprehensive test for TextFormatter modes.
 *
 * This test manually constructs a ScanSummary with realistic data
 * to demonstrate all implemented text output formats.
 */

using namespace asioscan;

int main(int argc, char** argv) {
    try {
        // Quick CLI11 smoke test
        CLI::App app{"AsioScan - TCP port scanner"};
        
        std::string target = "localhost";
        app.add_option("-t,--target", target, "Target host");
        
        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            return app.exit(e);
        }
        
        std::cout << "CLI11 OK - target: " << target << "\n\n";

        // Create scan summary with multiple hosts
        ScanSummary summary;
        
        const auto scan_start = std::chrono::steady_clock::now();
        summary.start_time = scan_start;
        
        // First host: scanme.nmap.org (multiple open ports)
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
        
        // Second host: example.com (one open port)
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
        h2_port2.state = PortState::Closed;
        h2_port2.latency = std::chrono::milliseconds(30);
        h2_port2.reason = "Connection refused";
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
        
        PortResult h2_port5;
        h2_port5.port = 5432;
        h2_port5.state = PortState::Error;
        h2_port5.latency = std::chrono::milliseconds(0);
        h2_port5.reason = "Network unreachable";
        host2.ports.push_back(h2_port5);
        
        host2.end_time = scan_start + std::chrono::milliseconds(1100);
        summary.hosts.push_back(host2);
        
        // Third host: 192.168.1.1 (no open ports)
        HostResult host3;
        host3.host = "192.168.1.1";
        host3.start_time = scan_start + std::chrono::milliseconds(1200);
        
        PortResult h3_port1;
        h3_port1.port = 22;
        h3_port1.state = PortState::Closed;
        h3_port1.latency = std::chrono::milliseconds(12);
        h3_port1.reason = "Connection refused";
        host3.ports.push_back(h3_port1);
        
        PortResult h3_port2;
        h3_port2.port = 80;
        h3_port2.state = PortState::Filtered;
        h3_port2.latency = std::chrono::milliseconds(500);
        h3_port2.reason = "Timeout";
        host3.ports.push_back(h3_port2);
        
        PortResult h3_port3;
        h3_port3.port = 443;
        h3_port3.state = PortState::Closed;
        h3_port3.latency = std::chrono::milliseconds(14);
        h3_port3.reason = "Connection refused";
        host3.ports.push_back(h3_port3);
        
        host3.end_time = scan_start + std::chrono::milliseconds(1700);
        summary.hosts.push_back(host3);
        
        // Fourth host: offline.local (completely down - no ports scanned)
        HostResult host4;
        host4.host = "offline.local";
        host4.start_time = scan_start + std::chrono::milliseconds(1800);
        host4.end_time = scan_start + std::chrono::milliseconds(1800);
        summary.hosts.push_back(host4);
        
        summary.end_time = scan_start + std::chrono::milliseconds(1800);
        
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
        
        std::cout << "\n\n";
        
        // Test 4: Summary-only mode
        std::cout << "=== Test 4: Summary-Only Mode ===\n\n";
        
        OutputOptions options4;
        options4.format = OutputFormat::Text;
        options4.text_mode = TextMode::Summary;
        options4.show_reason = false;
        
        formatter.print(summary, options4);
        
        std::cout << "\n\n";
        
        // Test 5: Ports-only mode
        std::cout << "=== Test 5: Ports-Only Mode ===\n\n";
        
        OutputOptions options5;
        options5.format = OutputFormat::Text;
        options5.text_mode = TextMode::PortsOnly;
        options5.show_reason = false;
        
        formatter.print(summary, options5);
        
        std::cout << "\n\n";
        
        // Test 6: Hosts-only mode
        std::cout << "=== Test 6: Hosts-Only Mode ===\n\n";
        
        OutputOptions options6;
        options6.format = OutputFormat::Text;
        options6.text_mode = TextMode::HostsOnly;
        options6.show_reason = false;
        
        formatter.print(summary, options6);
        
        std::cout << "\n\n";
        
        // Test 7: Verbose mode
        std::cout << "=== Test 7: Verbose Mode ===\n\n";
        
        OutputOptions options7;
        options7.format = OutputFormat::Text;
        options7.text_mode = TextMode::Verbose;
        options7.show_reason = false;
        
        formatter.print(summary, options7);
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
