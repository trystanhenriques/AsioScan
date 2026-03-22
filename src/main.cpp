#include "cli/cli_parser.hpp"

#include <iostream>
#include <string>

/*
 * Temporary main for validating CLI parser wiring.
 * Will be replaced with full scanner integration later.
 */

int main(int argc, char** argv) {
    auto result = asioscan::parse_cli(argc, argv);

    if (result.status != asioscan::ParseStatus::Ok) {
        return result.exit_code;
    }

    if (result.options.targets.empty()) {
        std::cerr << "No targets specified. Use --help for usage.\n";
        return 1;
    }

    std::cout << "Parsed " << result.options.targets.size() << " target(s):\n";
    for (const auto& target : result.options.targets) {
        std::cout << "  " << target << "\n";
    }

    return 0;
}
