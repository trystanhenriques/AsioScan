#pragma once

/*
 * Formatter
 * ---------
 * Abstract interface for rendering scan results to output.
 *
 * This interface separates result presentation from result production,
 * allowing multiple output formats (text, XML, JSON) to be implemented
 * independently of the scanner engine.
 *
 * Concrete formatters are responsible for:
 *  - Determining output destination (stdout, file, stream)
 *  - Applying formatting rules and style
 *  - Handling serialization and encoding
 *
 * This interface makes no assumptions about configuration, I/O strategy,
 * or presentation details. It exists solely to define the contract between
 * the result model and the presentation layer.
 */

namespace asioscan {

// Forward declaration to avoid coupling
struct ScanSummary;

/*
 * Formatter
 * ---------
 * Pure interface for output formatting implementations.
 *
 * Implementations must be stateless or manage their own output resources.
 * This interface guarantees only that a completed scan can be rendered.
 */
class Formatter {
public:
    virtual ~Formatter() = default;

    /*
     * Render a completed scan summary to output.
     *
     * Implementations determine destination, format, and style.
     * This method may write to stdout, a file, or any other sink.
     *
     * The summary is passed by const reference and must not be modified.
     */
    virtual void print(const ScanSummary& summary) = 0;
};

} // namespace asioscan
