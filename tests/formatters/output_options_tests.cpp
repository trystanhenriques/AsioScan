#include <catch2/catch_test_macros.hpp>

#include "formatters/OutputOptions.hpp"

TEST_CASE("OutputOptions defaults are stable", "[formatter][options]") {
    const asioscan::OutputOptions options;

    REQUIRE(options.format == asioscan::OutputFormat::Text);
    REQUIRE(options.text_mode == asioscan::TextMode::Normal);
    REQUIRE_FALSE(options.output_file.has_value());
    REQUIRE_FALSE(options.verbose);
    REQUIRE_FALSE(options.show_reason);
    REQUIRE(options.color_mode == asioscan::ColorMode::Auto);
}

TEST_CASE("OutputOptions can be configured explicitly", "[formatter][options]") {
    asioscan::OutputOptions options;

    options.format = asioscan::OutputFormat::Json;
    options.text_mode = asioscan::TextMode::Quiet;
    options.output_file = std::string{"results.txt"};
    options.verbose = true;
    options.show_reason = true;
    options.color_mode = asioscan::ColorMode::Never;

    REQUIRE(options.format == asioscan::OutputFormat::Json);
    REQUIRE(options.text_mode == asioscan::TextMode::Quiet);
    REQUIRE(options.output_file.has_value());
    REQUIRE(options.output_file.value() == "results.txt");
    REQUIRE(options.verbose);
    REQUIRE(options.show_reason);
    REQUIRE(options.color_mode == asioscan::ColorMode::Never);
}
