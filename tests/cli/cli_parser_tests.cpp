#include <catch2/catch_test_macros.hpp>

#include "cli/cli_parser.hpp"

#include <string>
#include <vector>

namespace {

asioscan::ParseResult run_parse(std::initializer_list<const char*> args) {
    std::vector<std::string> owned;
    owned.reserve(args.size() + 1);
    owned.emplace_back("asioscan");
    for (const char* arg : args) {
        owned.emplace_back(arg);
    }

    std::vector<char*> argv;
    argv.reserve(owned.size());
    for (auto& arg : owned) {
        argv.push_back(arg.data());
    }

    return asioscan::parse_cli(static_cast<int>(argv.size()), argv.data());
}

} // namespace

TEST_CASE("CLI parser builds ScanConfig from valid core args", "[cli][parser]") {
    const auto parsed = run_parse({"-p", "22,80,443", "-t", "750", "-c", "120", "scanme.nmap.org"});

    REQUIRE(parsed.status == asioscan::ParseStatus::Ok);
    REQUIRE(parsed.config.targets.size() == 1);
    REQUIRE(parsed.config.targets[0] == "scanme.nmap.org");

    REQUIRE(parsed.config.ports == std::vector<std::uint16_t>{22, 80, 443});
    REQUIRE(parsed.config.timeout.count() == 750);
    REQUIRE(parsed.config.max_concurrency == 120);
}

TEST_CASE("CLI parser normalizes mixed port expression", "[cli][parser]") {
    const auto parsed = run_parse({"-p", "443,22,80,80,1000-1002", "host-a", "host-b"});

    REQUIRE(parsed.status == asioscan::ParseStatus::Ok);
    REQUIRE(parsed.config.targets == std::vector<std::string>{"host-a", "host-b"});
    REQUIRE(parsed.config.ports == std::vector<std::uint16_t>{22, 80, 443, 1000, 1001, 1002});
}

TEST_CASE("CLI parser maps output mode and additive flags", "[cli][parser]") {
    const auto parsed = run_parse({"--summary", "-r", "-o", "results.txt", "-p", "80", "scanme.nmap.org"});

    REQUIRE(parsed.status == asioscan::ParseStatus::Ok);
    REQUIRE(parsed.output.text_mode == asioscan::TextMode::Summary);
    REQUIRE(parsed.output.show_reason);
    REQUIRE(parsed.output.output_file.has_value());
    REQUIRE(parsed.output.output_file.value() == "results.txt");
}

TEST_CASE("CLI parser rejects malformed or invalid ports", "[cli][parser]") {
    SECTION("Port zero") {
        const auto parsed = run_parse({"-p", "0", "scanme.nmap.org"});
        REQUIRE(parsed.status == asioscan::ParseStatus::Error);
    }

    SECTION("Port above range") {
        const auto parsed = run_parse({"-p", "70000", "scanme.nmap.org"});
        REQUIRE(parsed.status == asioscan::ParseStatus::Error);
    }

    SECTION("Malformed token") {
        const auto parsed = run_parse({"-p", "22,,80", "scanme.nmap.org"});
        REQUIRE(parsed.status == asioscan::ParseStatus::Error);
    }

    SECTION("Invalid range") {
        const auto parsed = run_parse({"-p", "100-50", "scanme.nmap.org"});
        REQUIRE(parsed.status == asioscan::ParseStatus::Error);
    }
}

TEST_CASE("CLI parser enforces mode exclusivity", "[cli][parser]") {
    const auto parsed = run_parse({"-q", "--summary", "-p", "80", "scanme.nmap.org"});

    REQUIRE(parsed.status == asioscan::ParseStatus::Error);
}

TEST_CASE("CLI parser returns clean exit status for help", "[cli][parser]") {
    const auto parsed = run_parse({"--help"});

    REQUIRE(parsed.status == asioscan::ParseStatus::Exit);
    REQUIRE(parsed.exit_code == 0);
}
