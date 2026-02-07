#pragma once

#include <optional>
#include <string>

namespace asioscan {

     /**
     * @brief Output format selection.
     *
     * Determines the formatter used to render ScanSummary.
     */
     enum class OutputFormat {
          Text,
          Xml,
          Json
     };

     /**
      * @brief Text output mode.
      *
      * Applies only when Format::Text is selected.
      */
     enum class TextMode {
          Normal,     // Full canonical output (default)
          Quiet,      // Open ports only
          Summary,    // Summary footer only
          PortsOnly,  // Per-host port tables only
          HostsOnly,  // Host-level information only
          Verbose     // Extended detail (planned)
     };

     /**
      * @brief Terminal color behavior.
      */
     enum class ColorMode {
          Auto,   // Enable colors if terminal supports it
          Always, // Force-enable colors
          Never   // Disable colors
     };

     /**
      * @brief OutputOptions
      *
      * Configuration for how scan results are presented.
      *
      * This struct controls output behavior ONLY.
      * It must not affect scanning logic, scheduling, or networking.
      *
      * Owned and populated by the CLI layer.
      * Consumed by formatter implementations.
      */
     struct OutputOptions {
          

          // ------------------------------------------------------------
          // Output selection
          // ------------------------------------------------------------

          OutputFormat format{ OutputFormat::Text };
          TextMode text_mode{ TextMode::Normal };

          // ------------------------------------------------------------
          // Output destination
          // ------------------------------------------------------------

          /**
           * @brief Optional output file path.
           *
           * If not set, output is written to stdout.
           */
          std::optional<std::string> output_file{};

          // ------------------------------------------------------------
          // Output behavior flags
          // ------------------------------------------------------------

          /**
           * @brief Emit additional informational output.
           *
           * Intended for progress or diagnostics.
           * Does NOT imply verbose scan results.
           */
          bool verbose{ false };

          /**
           * @brief Include connection reasons in text output.
           *
           * Example: timeout, connection refused, aborted.
           */
          bool show_reason{ false };

          /**
           * @brief Terminal color policy.
           *
           * Applies only to text output.
           */
          ColorMode color_mode{ ColorMode::Auto };
     };

} // namespace asioscan
