#include "formatters/xml_formatter.hpp"
#include "formatters/OutputOptions.hpp"
#include "result/scan_summary.hpp"
#include <ostream>

namespace asioscan {

void XmlFormatter::print(std::ostream& out,
                         const ScanSummary& /*summary*/,
                         const OutputOptions& /*options*/) {
    // Phase 1: Minimal stub to allow compilation of failing tests.
    // The tests will expect specific XML structures to be implemented here.
    out << "<?xml version=\"1.0\"?>\n";
    out << "<asioscan version=\"0.1.0\">\n";
    out << "</asioscan>\n";
}

} // namespace asioscan
