# AsioScan

AsioScan is a fast, cross-platform TCP connect port scanner built with modern C++20 and **standalone Asio**. It provides a clean, modular command-line interface for network exploration, offering both human-readable and machine-parseable outputs.

## Design Goals

Designed with a focus on code readability and a modular architecture, AsioScan serves as both a practical networking utility and an educational reference implementation for asynchronous network operations using standalone Asio.

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
asioscan 127.0.0.1 -p 80,443
```

### Example Output

```text
========================================
AsioScan Scan Report
========================================
...
----------------------------------------
Host: 127.0.0.1
Host scan duration: 512 ms

PORT     STATE
80       Filtered    
443      Filtered    

========================================
Scan Summary
========================================

Hosts scanned: 1
Ports scanned: 2
Open ports:    0
Total duration: 0.51s
```

---

## Development Guide

The core logic, CLI parser, and network scanner are written predominantly in modern **C++20** and leverage **standalone Asio** for high-performance TCP connect sweeping.

* **Build System:** CMake (3.16+)
* **Testing:** Catch2 (via CTest)

Please refer to the [**Building from Source**](docs/build.md) and [**Testing**](docs/testing.md) guides for full prerequisites, library installation via your system's package manager, and exact compilation steps.

---

## Documentation

For comprehensive guides on usage, building, CLI references, and architecture, please visit the [**AsioScan Documentation Hub**](docs/index.md).

---

## Scope and Status

**Project Status:** *Experimental / Active Development*
The project is currently establishing its foundation. Internal APIs, behavior, and output format structures are subject to change.

**Current Scope Limitations:**
Presently, the focus is strictly on reliable TCP connect scanning. Advanced scan types (like SYN stealth scans, which require raw sockets and elevated privileges) are currently out of scope until the core architecture is fully stabilized.


