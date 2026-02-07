#pragma once

/*
 * TextFormatter
 * -------------
 * Concrete formatter implementation for plain text output.
 *
 * This formatter renders scan results in a human-readable text format,
 * with behavior controlled by OutputOptions (mode, color, verbosity).
 *
 * The text formatter is the primary output format for AsioScan and
 * supports multiple presentation modes (normal, quiet, summary, etc.).
 */

#include "formatters/formatter.hpp"

namespace asioscan {

// Forward declarations
struct ScanSummary;
struct OutputOptions;

/*
 * TextFormatter
 * -------------
 * Plain text implementation of the Formatter interface.
 *
 * This class is stateless and determines all behavior from the
 * OutputOptions passed to print().
 */
class TextFormatter : public Formatter {
public:
    TextFormatter() = default;
    ~TextFormatter() override = default;

    TextFormatter(const TextFormatter&) = delete;
    TextFormatter& operator=(const TextFormatter&) = delete;
    TextFormatter(TextFormatter&&) = delete;
    TextFormatter& operator=(TextFormatter&&) = delete;

    /*
     * Render scan results as plain text.
     *
     * Output destination, format mode, and styling are determined by
     * the provided options.
     */
    void print(const ScanSummary& summary, const OutputOptions& options) override;
};

} // namespace asioscan
