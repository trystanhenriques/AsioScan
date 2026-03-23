#include "cli/cli_parser.hpp"
#include "scanner/scanner.hpp"
#include "formatters/text_formatter.hpp"

#include <iostream>

int main(int argc, char** argv) {
    try {
        auto parsed = asioscan::parse_cli(argc, argv);

        if (parsed.status != asioscan::ParseStatus::Ok) {
            return parsed.exit_code;
        }

        asioscan::Scanner scanner(std::move(parsed.config));
        auto summary = scanner.run();

        asioscan::TextFormatter formatter;
        formatter.print(summary, parsed.output);

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
