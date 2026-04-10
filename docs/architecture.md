# Architecture

AsioScan is designed as a modular, modern C++20 command-line application. It cleanly separates command-line parsing, asynchronous network I/O, and output formatting. This architectural separation ensures the tool is easy to maintain, test, and extend.

## High-Level Data Flow

The core execution path of AsioScan follows a strict, unidirectional pipeline:

```text
  [CLI Arguments]
         |
         v
  +--------------+       Produces
  |  CLI Parser  | --------------------+
  |   (CLI11)    |                     |
  +--------------+                     |
         |                             |
    ScanConfig                   OutputOptions
         |                             |
         v                             |
  +--------------+                     |
  |   Scanner    |                     |
  | (Boost.Asio) |                     v
  +--------------+             +---------------+
         |                     | Output Writer |
    ScanSummary  ------------> |  (Formatters) |
         |                     +---------------+
         v                             |
  [Network]                            v
                                [stdout / file]
```

1. **Input:** The user provides arguments via the command line.
2. **Parsing:** The `cli_parser` processes these arguments, enforcing constraints and validating inputs. It outputs two detached configurations: a `ScanConfig` and an `OutputOptions`.
3. **Execution:** The `Scanner` engine takes the immutable `ScanConfig`, connects to the network via `Boost.Asio`, and aggregates the results into a `ScanSummary`.
4. **Formatting:** The `output_writer` evaluates the `OutputOptions` to select a specific `Formatter` (Text or XML) and renders the `ScanSummary` to the specified output stream.

---

## Main Components

### 1. External CLI Parser (`src/cli/`)
Powered by `CLI11`, this module has one responsibility: translating raw string arguments (`argc`, `argv`) into strongly typed, validated C++ structs. It intentionally does not contain any networking or application logic. If parsing fails, it yields an error or an exit state (like printing the help menu) before the scanner ever initializes.

### 2. State and Models (`src/config/` and `src/result/`)
AsioScan uses simple, immutable (or strictly managed) data structures as internal contracts between modules:
*   **`ScanConfig`**: Dictates **what** and **how** to scan (targets, ports, timeout, concurrency).
*   **`OutputOptions`**: Dictates **where** and **what style** to render (XML vs. Text, console vs. file, quiet vs. verbose).
*   **`ScanSummary`**: The final artifact of a scan run. It contains a list of `HostResult` objects, each of which contains a list of `PortResult` objects (detailing individual port states, latencies, and connection reasons).

### 3. Asynchronous Scanner (`src/scanner/`)
The `Scanner` is the engine of AsioScan. It runs a single-threaded asynchronous task loop utilizing `Boost.Asio::io_context`.
*   It dynamically bounds concurrency by maintaining a strict limit of in-flight connection attempts.
*   It utilizes cooperative cancellation: if the user interrupts the program (`Ctrl+C`), the scanner stops scheduling new ports, drains the existing active connections, and gracefully returns a partial `ScanSummary`.

### 4. Output Formatters (`src/formatters/`)
Formatters operate purely on the finished `ScanSummary` tree.
AsioScan implements an abstract `Formatter` interface which allows polymorphic rendering of the results:
*   **`TextFormatter`**: Renders human-readable tables and summaries based on terminal output flags.
*   **`XmlFormatter`**: Safely escapes and serializes the summary tree into structured XML tags.
Because formatters are decoupled from the scanner, they can process data predictably without worrying about network states or asynchronous completion handlers.

---

## How Tests Fit into the Architecture

This strict component isolation makes unit testing straightforward and highly deterministic:
*   **CLI Tests**: We pass mocked `argv` arrays to the parser and assert that the resulting `ScanConfig` has the exact port vectors and timeouts we expect.
*   **Formatter Tests**: Instead of scanning a real network, the test suite constructs a fake, memory-only `ScanSummary` (injecting specific latencies, states, and string payloads) and passes it directly to the formatters. This allows us to assert that the generated Text or XML perfectly matches the desired schema without touching a live socket.
*   **Scanner Tests**: By mocking local listeners, the engine can be validated in isolation to ensure concurrency limits and timeouts function identically across operating systems. 
