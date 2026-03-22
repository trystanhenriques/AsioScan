#include "cli/cli_parser.hpp"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
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

        // Check for range separator
        auto dash = token.find('-');
        if (dash != std::string_view::npos) {
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

} // anonymous namespace

ParseResult parse_cli(int argc, char** argv) {
    ParseResult result;

    CLI::App app{"AsioScan - Cross-platform TCP port scanner"};

    // Raw CLI values before mapping into ScanConfig
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

    // Map remaining values
    result.config.timeout = std::chrono::milliseconds(timeout_ms);
    result.config.max_concurrency = concurrency;

    result.status = ParseStatus::Ok;
    return result;
}

} // namespace asioscan
