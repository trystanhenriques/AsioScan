#pragma once

/*
 * CliParser
 * ---------
 * Parses command-line arguments into structured data using CLI11.
 *
 * This module is responsible for argument parsing only.
 * It does not perform scanning, formatting, or I/O.
 *
 * The parser produces a ParsedCliOptions struct that downstream
 * layers use to build ScanConfig and OutputOptions.
 */

#include <string>
#include <vector>

namespace asioscan {

/*
 * ParsedCliOptions
 * ----------------
 * Lightweight result of CLI argument parsing.
 *
 * Contains raw parsed values before validation or transformation.
 * This struct will grow as more CLI options are added.
 */
struct ParsedCliOptions {
    // Positional target hostnames or IP addresses.
    std::vector<std::string> targets;
};

/*
 * ParseResult
 * -----------
 * Outcome of a parse attempt.
 *
 * Distinguishes between successful parsing, help/version display
 * (where the program should exit cleanly), and errors.
 */
enum class ParseStatus {
    Ok,       // Parsing succeeded; options are populated
    Exit,     // Help or version was shown; exit with code 0
    Error     // Parse error; exit with non-zero code
};

struct ParseResult {
    ParseStatus status{ParseStatus::Ok};
    int exit_code{0};
    ParsedCliOptions options;
};

/*
 * Parse command-line arguments.
 *
 * Uses CLI11 internally. Handles --help output automatically.
 * Returns structured parse result without performing any scanning.
 */
ParseResult parse_cli(int argc, char** argv);

} // namespace asioscan
