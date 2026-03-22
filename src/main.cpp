#include "cli/cli_parser.hpp"

#include <iostream>

/*
 * Temporary main for validating CLI parser wiring.
 * Will be replaced with full scanner integration later.
 */

using namespace asioscan;

namespace {

const char* text_mode_name(TextMode mode) {
    switch (mode) {
        case TextMode::Normal:    return "Normal";
        case TextMode::Quiet:     return "Quiet";
        case TextMode::Summary:   return "Summary";
        case TextMode::PortsOnly: return "PortsOnly";
        case TextMode::HostsOnly: return "HostsOnly";
        case TextMode::Verbose:   return "Verbose";
        default:                  return "Unknown";
    }
}

} // anonymous namespace

int main(int argc, char** argv) {
    auto result = parse_cli(argc, argv);

    if (result.status != ParseStatus::Ok) {
        return result.exit_code;
    }

    const auto& config = result.config;
    const auto& output = result.output;

    std::cout << "--- ScanConfig ---\n";
    std::cout << "Targets (" << config.targets.size() << "):\n";
    for (const auto& t : config.targets) {
        std::cout << "  " << t << "\n";
    }

    std::cout << "Ports (" << config.ports.size() << "): ";
    for (std::size_t i = 0; i < config.ports.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << config.ports[i];
    }
    std::cout << "\n";

    std::cout << "Timeout: " << config.timeout.count() << " ms\n";
    std::cout << "Concurrency: " << config.max_concurrency << "\n";

    std::cout << "\n--- OutputOptions ---\n";
    std::cout << "Text mode: " << text_mode_name(output.text_mode) << "\n";
    std::cout << "Show reason: " << (output.show_reason ? "yes" : "no") << "\n";
    std::cout << "Output file: "
              << (output.output_file ? *output.output_file : "(stdout)") << "\n";

    return 0;
}
