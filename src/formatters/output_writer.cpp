#include "formatters/output_writer.hpp"
#include "formatters/text_formatter.hpp"
#include "formatters/xml_formatter.hpp"

#include <fstream>
#include <memory>
#include <ostream>

namespace asioscan {

bool write_output(const ScanSummary& summary, const OutputOptions& options, std::ostream& default_out) {
    std::unique_ptr<Formatter> formatter;
    
    if (options.format == OutputFormat::Xml) {
        formatter = std::make_unique<XmlFormatter>();
    } else {
        formatter = std::make_unique<TextFormatter>();
    }

    std::ofstream file_out;
    std::ostream* active_stream = &default_out;

    if (options.output_file.has_value()) {
        file_out.open(*options.output_file, std::ios::out | std::ios::trunc);
        if (!file_out.is_open()) {
            return false;
        }
        active_stream = &file_out;
    }

    formatter->print(*active_stream, summary, options);
    return true;
}

} // namespace asioscan