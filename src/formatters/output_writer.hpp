#pragma once

#include "formatters/OutputOptions.hpp"
#include "result/scan_summary.hpp"
#include <iosfwd>

namespace asioscan {

/*
 * write_output
 * ------------
 * Handles formatting and directing scan results to the appropriate
 * output stream or file based on CLI provided options.
 *
 * Returns true on success, false if a requested output file could
 * not be opened.
 */
bool write_output(const ScanSummary& summary, const OutputOptions& options, std::ostream& default_out);

} // namespace asioscan