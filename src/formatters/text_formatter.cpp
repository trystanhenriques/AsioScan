#include "formatters/text_formatter.hpp"

#include "formatters/OutputOptions.hpp"
#include "result/scan_summary.hpp"

#include <iostream>

namespace asioscan {

/*
 * TextFormatter::print
 * --------------------
 * Skeleton implementation for text output rendering.
 *
 * This is a placeholder that ensures the formatter compiles and can be
 * invoked without crashing. Actual formatting logic will be implemented
 * in subsequent phases.
 */
void TextFormatter::print(const ScanSummary& summary, const OutputOptions& options) {
    // Placeholder implementation
    // Prevents crashes and validates interface contract
    // Actual formatting logic will be added in later phases

    (void)summary;  // Suppress unused parameter warning
    (void)options;  // Suppress unused parameter warning

    // Minimal placeholder output to confirm execution
    std::cout << "[TextFormatter placeholder - formatting not yet implemented]\n";
}

} // namespace asioscan
