#pragma once

/*
 * CliParser
 * ---------
 * Parses command-line arguments into structured data using CLI11.
 *
 * This module is responsible for argument parsing only.
 * It does not perform scanning, formatting, or I/O.
 *
 * The parser produces a ScanConfig and OutputOptions that
 * downstream layers consume independently.
 */

#include "config/scan_config.hpp"
#include "formatters/OutputOptions.hpp"

#include <string>
#include <vector>

namespace asioscan {

/*
 * Outcome of a parse attempt.
 *
 * Distinguishes between successful parsing, help/version display
 * (where the program should exit cleanly), and errors.
 */
enum class ParseStatus {
    Ok,       // Parsing succeeded; config and options are populated
    Exit,     // Help or version was shown; exit with code 0
    Error     // Parse error; exit with non-zero code
};

struct ParseResult {
    ParseStatus status{ParseStatus::Ok};
    int exit_code{0};
    ScanConfig config;
    OutputOptions output;
};

/*
 * Parse command-line arguments.
 *
 * Uses CLI11 internally. Handles --help output automatically.
 * Validates and maps arguments into ScanConfig and OutputOptions.
 * Returns structured parse result without performing any scanning.
 */
ParseResult parse_cli(int argc, char** argv);

} // namespace asioscan
