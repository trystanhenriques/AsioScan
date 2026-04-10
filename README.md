# AsioScan

AsioScan is a cross-platform TCP connect port scanner written in modern C++ using **Boost.Asio**. It is designed as a clean, modular command-line tool for network exploration and port scanning.

## Key Features

* TCP connect scanning
* Single-host and multi-host scans
* Port ranges and common-port presets
* Human-readable text and XML output formats
* Command-line interface

## Project Status

This project is experimental and educational in nature. It is currently under active development. APIs, behavior, and structure are subject to change.

## Supported Platforms

* Cross-platform (Windows, macOS, Linux)

## Build from source

AsioScan is built using CMake. Ensure you have a modern C++20 compiler, CMake, and Boost installed.

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Running tests

The project uses Catch2 for unit testing. After building, you can run the test suite:

```bash
cd build
ctest --output-on-failure
```

## Quick usage examples

To scan a target:
```bash
asioscan --target 127.0.0.1 --ports 80,443
```

## Output formats

AsioScan supports multiple output formats:
* **Text**: Standard human-readable output to the console.
* **XML**: Structured XML output for integration with other tools and scripts.

## Current scope / out of scope

Presently, the focus is on robust TCP connect scanning. Advanced scan types (like SYN stealth scans that require raw sockets and elevated privileges) are planned for future development but are currently out of scope until the foundational architecture is complete.

## Documentation

Further detailed documentation linking to `docs/` is planned for a future release.


