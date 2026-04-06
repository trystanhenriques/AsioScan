#include "cli/cli_parser.hpp"
#include "scanner/scanner.hpp"
#include "formatters/text_formatter.hpp"

#include <fstream>
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

        std::ofstream output_file;
        std::ostream* output_stream = &std::cout;

        if (parsed.output.output_file.has_value()) {
            output_file.open(*parsed.output.output_file, std::ios::out | std::ios::trunc);
            if (!output_file.is_open()) {
                std::cerr << "Error: failed to open output file: "
                          << *parsed.output.output_file << "\n";
                return 1;
            }
            output_stream = &output_file;
        }

        formatter.print(*output_stream, summary, parsed.output);

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
