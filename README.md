# AsioScan

AsioScan is a fast, cross-platform TCP connect port scanner built with modern C++20 and **Boost.Asio**. It provides a clean, modular command-line interface for network exploration, offering both human-readable and machine-parseable outputs.

## Design Goals

Designed with a focus on code readability and a modular architecture, AsioScan serves as both a practical networking utility and an educational reference implementation for asynchronous network operations using Boost.Asio.

## Features

* **TCP Connect Scanning:** Reliable, connection-based port discovery.
* **Flexible Targeting:** Supports single-host and multi-host scans.
* **Port Configuration:** Scan explicit port lists, ranges, or utilize common-port presets.
* **Multiple Output Formats:** Standard console text for users, and XML for automated tool integration.
* **Cross-Platform:** Native support for Windows, macOS, and Linux.

---

## Usage

### Quick Start

```bash
asioscan 127.0.0.1 --ports 80,443
```

### Example Output

```text
Scanning 127.0.0.1...
Port 80    : OPEN
Port 443   : OPEN
Scan complete.
```

---

## Development Guide

### Prerequisites

* A modern **C++20** compatible compiler (GCC, Clang, or MSVC)
* **CMake** (3.15+)
* **Boost** libraries (specifically Boost.Asio)

### Building from Source

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Running Tests

AsioScan integrates **Catch2** for unit testing. After building, run the test suite via CTest:

```bash
cd build
ctest --output-on-failure
```

---

## Scope and Status

**Project Status:** *Experimental / Active Development*
The project is currently establishing its foundation. Internal APIs, behavior, and output format structures are subject to change. Extensive documentation linking to `docs/` is planned for a future stable release.

**Current Scope Limitations:**
Presently, the focus is strictly on reliable TCP connect scanning. Advanced scan types (like SYN stealth scans, which require raw sockets and elevated privileges) are currently out of scope until the core architecture is fully stabilized.


