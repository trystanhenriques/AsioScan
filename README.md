# AsioScan

![CI](https://github.com/trystanhenriques/AsioScan/actions/workflows/ci.yml/badge.svg)

**AsioScan** is a lightning-fast, cross-platform TCP connect port scanner built using **Modern C++20** and **standalone Asio**. 

Designed for high throughput, memory safety, and non-blocking I/O, AsioScan serves as a practical utility for network exploration and a robust example of asynchronous programming in C++.

---

## Features

* **TCP Connect Sweeping:** Reliable, RFC-compliant connection-based port discovery.
* **Flexible Targeting:** Supports targeting single hosts or multiple IP/hostname resolutions concurrently.
* **Granular Concurrency:** Explicit bounding of max in-flight connections to avoid exhausting OS file descriptor limits.
* **Multiple Output Modes:** Output to standard text, quiet (script-friendly) mode, or full XML layouts for automated toolchains.

For a deeper dive into the tool's non-blocking, single-threaded cooperative concurrency engine, see the [**Architecture Documentation**](docs/architecture.md).

---

## Quick Start

AsioScan features a highly modular command-line interface powered by `CLI11`. 

### Basic Usage
```bash
asioscan <targets> -p <ports> [options]
```
```bash
# Scan a single host on common web ports
asioscan scanme.nmap.org -p 80,443

# Sweep a broad range of ports aggressively
asioscan scanme.nmap.org -p 1-10000 -c 500 -t 200
```

### Key CLI Options Preview
* `-p, --ports` : Ports to scan (e.g., `80`, `22,80,443`, `1-1024`).
* `-c, --concurrency` : Maximum concurrent async connections (default: `200`).
* `-t, --timeout` : Per-port TCP connection timeout in milliseconds (default: `500`).
* `-o, --output` : Write normal text results to a specific file.
* `--oX, --output-xml` : Write structured XML output to a file for automation.
* `-v, --verbose` : Show extended detail per port (latency, reasons).
* `-q, --quiet` : Script-friendly minimum output showing only open ports.
* `--summary` : Show only the final scan summary statistics.

### Example Output

<img width="706" height="804" alt="image" src="https://github.com/user-attachments/assets/9215a537-ac06-4214-a155-0a3fa14a2667" />


For the complete list of options and output formatting flags, see the [**CLI Reference**](docs/cli.md).

---

## Installation

Pre-compiled, ready-to-run binaries are available for Windows, macOS, and Linux on the [**GitHub Releases page**](https://github.com/trystanhenriques/AsioScan/releases).

1. Download the archive for your operating system.
2. Extract the file.
3. Run the executable directly from your terminal or command prompt. No dependencies or installations are required!

---

## Build Instructions

AsioScan uses modern **CMake (3.20+)** and manages dependencies automatically via `FetchContent`. 

We ship with standard `CMakePresets.json` configurations making building unified across OS environments (Linux, macOS, and Windows):

```bash
# 1. Clone the repository
git clone https://github.com/trystanhenriques/AsioScan.git
cd AsioScan

# 2. Configure for your OS (e.g., ubuntu-latest, macos-latest, or windows-latest)
cmake --preset ubuntu-latest

# 3. Build the project
cmake --build --preset ubuntu-latest
```

For full system requirements and advanced toolchain details, see the [**Building Documentation**](docs/build.md).

---

## Testing & CI

This project maintains rigorous test coverage isolating the async engine, CLI parser bounds, OS state classifications, and formatter outputs using **Catch2 v3**.

```bash
# Run the test suite against the target preset
ctest --preset ubuntu-latest
```

The repository is fully integrated with **GitHub Actions**, automatically validating builds and running tests across all major operating systems. 

For more details on the testing framework and test organization, see the [**Testing Documentation**](docs/testing.md).

---

## Documentation Hub

For all other information, including advanced usage, architecture breakdowns, and pre-release roadmaps, please explore the [**AsioScan Documentation Hub**](docs/index.md).


