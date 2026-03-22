#include "cli/cli_parser.hpp"

#include <iostream>

/*
 * Temporary main for validating CLI parser wiring.
 * Will be replaced with full scanner integration later.
 */

int main(int argc, char** argv) {
    auto result = asioscan::parse_cli(argc, argv);

    if (result.status != asioscan::ParseStatus::Ok) {
        return result.exit_code;
    }

    const auto& config = result.config;

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

    return 0;
}
