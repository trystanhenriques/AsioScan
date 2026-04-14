# Building AsioScan from Source

This guide provides instructions for compiling AsioScan across Windows, macOS, and Linux. AsioScan relies on a modern CMake build system and automatically manages most of its dependencies.

## Prerequisites

To build AsioScan natively, your system must have the following tools installed:

* **C++ Compiler**: A modern compiler supporting C++20 (e.g., GCC 10+, Clang 10+, or MSVC 2019+).
* **CMake**: Version 3.20 or newer (Required for `CMakePresets.json` support).

### Dependency Management

The build system automatically fetches and builds the following internal dependencies using CMake's `FetchContent` module. You do **not** need to install them manually:
* **Standalone Asio** (for cross-platform networking)
* **CLI11** (for command-line argument parsing)
* **Catch2 v3** (for unit testing)

---

## Installing Toolchains by Platform

### Linux (Debian/Ubuntu)
```bash
sudo apt update
sudo apt install build-essential cmake
```

### macOS (Homebrew)
```bash
brew install cmake
```

### Windows
1. **Compiler**: Install Visual Studio (2019 or 2022) with the "Desktop development with C++" workload.
2. **CMake**: Install via the Visual Studio Installer or download from [cmake.org](https://cmake.org/).

---

## Configuring and Building (Using CMake Presets)

AsioScan ships with a unified `CMakePresets.json` file. This is the recommended way to build the project, as it automatically aligns generators, build types, and output behaviors across all operating systems.

**1. Clone the repository:**
```bash
git clone https://github.com/trystanhenriques/AsioScan.git
cd AsioScan
```

**2. Configure the project:**
Choose the preset that matches your operating system:
* `ubuntu-latest` (Uses Unix Makefiles)
* `macos-latest` (Uses Unix Makefiles)
* `windows-latest` (Uses Visual Studio 17 2022)

```bash
# Example for Linux
cmake --preset ubuntu-latest
```

**3. Compile the project:**
This will automatically launch the build process targeting a `Release` configuration.
```bash
# Example for Linux
cmake --build --preset ubuntu-latest
```

---

## Running the Executable

Once the build completes, the executable will be deposited in the `build/` directory structure. 

**Linux / macOS:**
```bash
./build/asioscan --help
```

**Windows:**
Because Windows uses a multi-config generator (Visual Studio), the binary is placed inside a `Release` subdirectory:
```powershell
.\build\Release\asioscan.exe --help
```

---

## Running the Tests

AsioScan includes a thorough suite of unit tests built using **Catch2**. 

You can run the entire test suite directly through CMake's testing tool, `ctest`, using the same preset:

```bash
ctest --preset ubuntu-latest
```
*(Note: Output on failure is automatically enabled via the preset).*

Alternatively, you can invoke the Catch2 test runner directly for more granular control (like running specific tags):

**Linux / macOS:**
```bash
./build/tests/asioscan_tests [cli]
```

**Windows:**
```powershell
.\build\tests\Release\asioscan_tests.exe [xml]
```

---

## Custom / Manual Builds

If you prefer not to use presets, you can always build the project manually using standard CMake commands:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
ctest --output-on-failure -C Release