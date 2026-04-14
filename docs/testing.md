# Testing AsioScan

Reliable network tools require robust testing. AsioScan employs a comprehensive unit testing strategy to ensure that core logic, parsing, and output formatting remain stable across platforms and updates.

## Framework Overview

AsioScan uses **Catch2 (v3)** as its primary testing framework. 
It is defined within the project's CMake configuration using `FetchContent`, meaning the framework is downloaded and built automatically. You do not need to install it manually on your system.

Tests are discovered and integrated into CMake's testing tool, **CTest**, via `catch_discover_tests()`.

---

## Building the Tests

The test suite is compiled automatically alongside the main library when you build AsioScan from source. 

```bash
mkdir build && cd build
cmake ..
cmake --build .
```
This produces the test executable `asioscan_tests` (or `asioscan_tests.exe` on Windows) within the `build/tests/` directory.

---

## Running the Tests

There are two primary ways to run the test suite:

### 1. Using CTest (Recommended)
CTest runs the discovered test cases and provides a high-level pass/fail summary. This is the standard way to verify a build.

```bash
cd build
ctest --output-on-failure
```

### 2. Invoking Catch2 Directly
For granular control, debugging, or running a subset of tests, you can run the compiled executable directly. This exposes standard Catch2 command-line arguments.

```bash
# Run all tests with extensive output
./tests/asioscan_tests -s

# Run tests related only to the CLI parser using tags
./tests/asioscan_tests [cli]

# Run tests related to XML formatting
./tests/asioscan_tests [xml]
```

---

## Test Organization

The tests are physically separated from the main source code and reside in the `tests/` directory. The structure mirrors the `src/` directory layout:

* **`tests/cli/`** 
  * Validates command-line parsing, constraint enforcement (e.g., port ranges, timeout limits), and mutually exclusive flags. It includes tests that capture `stdout`/`stderr` to verify help messages and error outputs.
* **`tests/formatters/`** 
  * Ensures output generation is strictly deterministic. Tests inject fabricated `ScanSummary` structural data to verify that the Text and XML formatters emit exactly the expected strings, whitespace, and XML tags without relying on live network data.
* **`tests/result/`** 
  * Unit tests validating the state tracking structures (`HostResult`, `PortResult`, `ScanSummary`), ensuring connection states and timings evaluate correctly.
* **`tests/scanner/`** 
  * Unit tests for the core asynchronous scanning engine and concurrency bounding.
* **`tests/test_smoke.cpp`** 
  * High-level basic sanity checks.

## Determinism and Portability

To provide reliable CI/CD and local development execution, the vast majority of AsioScan's tests are completely deterministic. 
For example, the formatter and CLI tests do not access the live network. They test internal parsing rules and string generation using mocked inputs, ensuring that a test passing on Linux will also pass flawlessly on Windows.

---

## Continuous Integration (CI)

AsioScan features a rigorous, multi-platform **GitHub Actions CI** matrix targeting Ubuntu, Windows, and macOS endpoints. 
Each pull request and push to the repository seamlessly provisions the latest CMake Presets environment, builds the solution, and executes `ctest` targeting 100% pass coverage to guarantee the integrity of the project before a release.