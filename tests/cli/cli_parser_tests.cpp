#include <catch2/catch_test_macros.hpp>

#include "cli/cli_parser.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

class StdStreamCapture {
public:
    StdStreamCapture()
        : old_out_(std::cout.rdbuf(out_.rdbuf()))
        , old_err_(std::cerr.rdbuf(err_.rdbuf())) {}

    ~StdStreamCapture() {
        std::cout.rdbuf(old_out_);
        std::cerr.rdbuf(old_err_);
    }

    std::string out() const { return out_.str(); }
    std::string err() const { return err_.str(); }

private:
    std::ostringstream out_;
    std::ostringstream err_;
    std::streambuf* old_out_;
    std::streambuf* old_err_;
};

struct ParseInvocation {
    asioscan::ParseResult result;
    std::string stdout_text;
    std::string stderr_text;
};

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

ParseInvocation run_parse_with_capture(std::initializer_list<const char*> args) {
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

    StdStreamCapture capture;
    ParseInvocation invocation;
    invocation.result = asioscan::parse_cli(static_cast<int>(argv.size()), argv.data());
    invocation.stdout_text = capture.out();
    invocation.stderr_text = capture.err();
    return invocation;
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

TEST_CASE("CLI parser maps verbose mode to output state", "[cli][parser]") {
    const auto parsed = run_parse({"-v", "-p", "80", "scanme.nmap.org"});

    REQUIRE(parsed.status == asioscan::ParseStatus::Ok);
    REQUIRE(parsed.output.text_mode == asioscan::TextMode::Verbose);
    REQUIRE(parsed.output.verbose);
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

TEST_CASE("CLI help output includes practical usage examples", "[cli][help]") {
    const auto invocation = run_parse_with_capture({"--help"});

    REQUIRE(invocation.result.status == asioscan::ParseStatus::Exit);
    REQUIRE(invocation.result.exit_code == 0);

    REQUIRE(invocation.stdout_text.find("Examples:") != std::string::npos);
    REQUIRE(invocation.stdout_text.find("asioscan scanme.nmap.org") != std::string::npos);
    REQUIRE(invocation.stdout_text.find("asioscan -p 22,80,443 scanme.nmap.org") != std::string::npos);
    REQUIRE(invocation.stdout_text.find("asioscan --summary -p 1-100 example.com") != std::string::npos);
    REQUIRE(invocation.stdout_text.find("asioscan --ports-only -o results.txt 192.168.1.1") != std::string::npos);
}

TEST_CASE("CLI parser reports invalid options cleanly", "[cli][parser]") {
    const auto invocation = run_parse_with_capture({"--not-a-real-option", "-p", "80", "scanme.nmap.org"});

    REQUIRE(invocation.result.status == asioscan::ParseStatus::Error);
    REQUIRE(invocation.result.exit_code != 0);
    REQUIRE(invocation.stderr_text.find("--not-a-real-option") != std::string::npos);
}

TEST_CASE("CLI parser requires at least one target", "[cli][parser]") {
    const auto invocation = run_parse_with_capture({"-p", "80"});

    REQUIRE(invocation.result.status == asioscan::ParseStatus::Error);
    REQUIRE(invocation.result.exit_code != 0);
    REQUIRE(invocation.stderr_text.find("at least one target is required") != std::string::npos);
}

TEST_CASE("CLI parser rejects blank output path", "[cli][parser]") {
    const auto invocation = run_parse_with_capture({"-p", "80", "-o", "   ", "scanme.nmap.org"});

    REQUIRE(invocation.result.status == asioscan::ParseStatus::Error);
    REQUIRE(invocation.result.exit_code != 0);
    REQUIRE(invocation.stderr_text.find("output path must not be blank") != std::string::npos);
}
