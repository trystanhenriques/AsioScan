#include "cli/cli_parser.hpp"

#include <CLI/CLI.hpp>

#include <charconv>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace asioscan {

namespace {

/*
 * Parse a single port number from a string.
 * Throws std::invalid_argument on malformed or out-of-range input.
 */
std::uint16_t parse_single_port(std::string_view token) {
    unsigned long value = 0;
    auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);

    if (ec != std::errc{} || ptr != token.data() + token.size()) {
        throw std::invalid_argument("Invalid port: '" + std::string(token) + "'");
    }

    if (value == 0 || value > 65535) {
        throw std::invalid_argument(
            "Port out of range (1-65535): " + std::string(token));
    }

    return static_cast<std::uint16_t>(value);
}

/*
 * Parse a port specification string into a sorted, unique port list.
 *
 * Supported formats:
 *   "80"             single port
 *   "22,80,443"      comma-separated
 *   "1-1024"         range
 *   "22,80,1000-1010"  mixed
 */
std::vector<std::uint16_t> parse_ports(const std::string& spec) {
    std::set<std::uint16_t> ports;

    std::string_view remaining(spec);

    while (!remaining.empty()) {
        // Find next comma-delimited token
        auto comma = remaining.find(',');
        std::string_view token = remaining.substr(0, comma);
        remaining = (comma == std::string_view::npos)
            ? std::string_view{}
            : remaining.substr(comma + 1);

        if (token.empty()) {
            throw std::invalid_argument("Empty port token in: '" + spec + "'");
        }

        // Check for range separator (dash not at position 0)
        auto dash = token.find('-');
        if (dash != std::string_view::npos && dash != 0) {
            auto start = parse_single_port(token.substr(0, dash));
            auto end = parse_single_port(token.substr(dash + 1));

            if (start > end) {
                throw std::invalid_argument(
                    "Invalid port range: " + std::string(token));
            }

            for (unsigned long p = start; p <= end; ++p) {
                ports.insert(static_cast<std::uint16_t>(p));
            }
        } else {
            ports.insert(parse_single_port(token));
        }
    }

    if (ports.empty()) {
        throw std::invalid_argument("No valid ports in: '" + spec + "'");
    }

    return {ports.begin(), ports.end()};
}

bool is_blank(const std::string& value) {
    for (char c : value) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

} // anonymous namespace

ParseResult parse_cli(int argc, char** argv) {
    ParseResult result;

    CLI::App app{"AsioScan - Cross-platform TCP port scanner"};
    app.failure_message(CLI::FailureMessage::help);
    app.footer(
        "Examples:\n"
        "  asioscan scanme.nmap.org\n"
        "  asioscan -p 22,80,443 scanme.nmap.org\n"
        "  asioscan --summary -p 1-100 example.com\n"
        "  asioscan --ports-only -o results.txt 192.168.1.1"
    );

    // --- Scan config raw values ---
    std::string port_spec;
    std::uint64_t timeout_ms = result.config.timeout.count();
    std::size_t concurrency = result.config.max_concurrency;

    app.add_option("targets", result.config.targets,
                   "Target hostnames or IP addresses")
        ->expected(-1);

    app.add_option("-p,--ports", port_spec,
                   "Ports to scan (e.g. 80, 22,80,443, 1-1024)")
        ->required();

    app.add_option("-t,--timeout", timeout_ms,
                   "Per-port timeout in milliseconds (default: 500)")
        ->check(CLI::Range(static_cast<std::uint64_t>(1), static_cast<std::uint64_t>(300000)));

    app.add_option("-c,--concurrency", concurrency,
                   "Max concurrent connections (default: 200)")
        ->check(CLI::Range(static_cast<std::size_t>(1), static_cast<std::size_t>(10000)));

    // --- Output mode flags (mutually exclusive) ---
    bool quiet = false;
    bool summary = false;
    bool ports_only = false;
    bool hosts_only = false;
    bool verbose = false;

    auto* flag_quiet = app.add_flag("-q,--quiet", quiet,
                                    "Show only open ports (minimal output)");
    auto* flag_summary = app.add_flag("--summary", summary,
                                      "Show scan summary statistics only");
    auto* flag_ports = app.add_flag("--ports-only", ports_only,
                                    "Show per-host port results only");
    auto* flag_hosts = app.add_flag("--hosts-only", hosts_only,
                                    "Show host availability only");
    auto* flag_verbose = app.add_flag("-v,--verbose", verbose,
                                      "Show extended detail per port");

    // Enforce mutual exclusion
    flag_quiet->excludes(flag_summary, flag_ports, flag_hosts, flag_verbose);
    flag_summary->excludes(flag_quiet, flag_ports, flag_hosts, flag_verbose);
    flag_ports->excludes(flag_quiet, flag_summary, flag_hosts, flag_verbose);
    flag_hosts->excludes(flag_quiet, flag_summary, flag_ports, flag_verbose);
    flag_verbose->excludes(flag_quiet, flag_summary, flag_ports, flag_hosts);

    // --- Additive output flags ---
    app.add_flag("-r,--reason", result.output.show_reason,
                 "Include connection reason in output");

    // --- Output destination ---
    std::string output_path;
    app.add_option("-o,--output", output_path,
                   "Write output to file instead of stdout");

    // --- Parse ---
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        result.exit_code = app.exit(e);
        result.status = (result.exit_code == 0) ? ParseStatus::Exit
                                                : ParseStatus::Error;
        return result;
    }

    // Validate targets
    if (result.config.targets.empty()) {
        std::cerr << "Error: at least one target is required.\n";
        result.status = ParseStatus::Error;
        result.exit_code = 1;
        return result;
    }

    if (!output_path.empty()) {
        if (is_blank(output_path)) {
            std::cerr << "Error: output path must not be blank.\n";
            result.status = ParseStatus::Error;
            result.exit_code = 1;
            return result;
        }
        result.output.output_file = output_path;
    }

    // Parse and validate ports
    try {
        result.config.ports = parse_ports(port_spec);
        result.config.port_description = port_spec;
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: " << e.what() << "\n";
        result.status = ParseStatus::Error;
        result.exit_code = 1;
        return result;
    }

    // Map scan config values
    result.config.timeout = std::chrono::milliseconds(timeout_ms);
    result.config.max_concurrency = concurrency;

    // Map output mode
    if (quiet)           result.output.text_mode = TextMode::Quiet;
    else if (summary)    result.output.text_mode = TextMode::Summary;
    else if (ports_only) result.output.text_mode = TextMode::PortsOnly;
    else if (hosts_only) result.output.text_mode = TextMode::HostsOnly;
    else if (verbose)    result.output.text_mode = TextMode::Verbose;

    result.output.verbose = verbose;

    result.status = ParseStatus::Ok;
    return result;
}

} // namespace asioscan
