#pragma once

#include "formatters/formatter.hpp"
#include <iosfwd>

namespace asioscan {

struct ScanSummary;
struct OutputOptions;

class XmlFormatter : public Formatter {
public:
    XmlFormatter() = default;
    ~XmlFormatter() override = default;

    XmlFormatter(const XmlFormatter&) = delete;
    XmlFormatter& operator=(const XmlFormatter&) = delete;
    XmlFormatter(XmlFormatter&&) = delete;
    XmlFormatter& operator=(XmlFormatter&&) = delete;

    void print(std::ostream& out,
               const ScanSummary& summary,
               const OutputOptions& options) override;
};

} // namespace asioscan
