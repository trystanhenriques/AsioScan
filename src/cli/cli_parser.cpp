#include "cli/cli_parser.hpp"

#include <CLI/CLI.hpp>

namespace asioscan {

ParseResult parse_cli(int argc, char** argv) {
    ParseResult result;

    CLI::App app{"AsioScan - Cross-platform TCP port scanner"};

    app.add_option("targets", result.options.targets, "Target hostnames or IP addresses")
        ->expected(-1);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        result.exit_code = app.exit(e);
        result.status = (result.exit_code == 0) ? ParseStatus::Exit : ParseStatus::Error;
        return result;
    }

    result.status = ParseStatus::Ok;
    return result;
}

} // namespace asioscan
