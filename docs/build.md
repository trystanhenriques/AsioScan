# Building AsioScan from Source

This guide provides instructions for compiling AsioScan across Windows, macOS, and Linux. AsioScan relies on a modern CMake build system and automatically manages most of its dependencies.

## Prerequisites

To build AsioScan, your system must have the following tools installed:

* **C++ Compiler**: A compiler supporting C++20 (e.g., GCC 10+, Clang 10+, or MSVC 2019+).
* **CMake**: Version 3.16 or newer.
* **Boost Libraries**: Specifically `Boost.Asio` and `Boost.System`. This must be installed on your system prior to building.

### Dependency Management

The build system automatically fetches and builds the following internal dependencies using CMake's `FetchContent` module, so you do **not** need to install them manually:
* **CLI11** (for command-line argument parsing)
* **Catch2** (for unit testing)

---

## Installing Prerequisites by Platform

### Linux (Debian/Ubuntu)
```bash
sudo apt update
sudo apt install build-essential cmake libboost-all-dev
```

### macOS (Homebrew)
```bash
brew install cmake boost
```

### Windows
1. **Compiler**: Install Visual Studio (2019 or 2022) with the "Desktop development with C++" workload.
2. **CMake**: Install via the Visual Studio Installer or download from [cmake.org](https://cmake.org/).
3. **Boost**: The easiest way to get Boost on Windows is using [vcpkg](https://vcpkg.io/):
   ```bash
   vcpkg install boost-system:x64-windows
   ```
   *Note: Using vcpkg requires providing the toolchain file to CMake during configuration.*

---

## Configuring and Building

1. **Clone the repository:**
   ```bash
   git clone https://github.com/trystanhenriques/AsioScan.git
   cd AsioScan
   ```

2. **Generate the build files using CMake:**
   We recommend performing an out-of-source build by creating a `build` directory. 
   
   *For Linux/macOS:*
   ```bash
   mkdir build && cd build
   cmake -DCMAKE_BUILD_TYPE=Release ..
   ```

   *For Windows (using vcpkg):*
   ```powershell
   mkdir build && cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE="C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake"
   ```

3. **Compile the project:**
   ```bash
   cmake --build . --config Release
   ```

---

## Running the Executable

Once the build completes, the executable will be located in the build directory.

**Linux / macOS:**
```bash
./asioscan --help
```

**Windows:**
Depending on your generator, it may be placed inside a `Release` subdirectory:
```powershell
.\Release\asioscan.exe --help
```

---

## Running the Tests

AsioScan includes a thorough suite of unit tests built using **Catch2**. The test executable (`asioscan_tests`) is built alongside the main application. 

You can run the entire test suite heavily integrated with CMake's testing tool, `ctest`:

```bash
# From inside your build directory
ctest --output-on-failure --build-config Release
```

Alternatively, you can invoke the Catch2 test runner directly for more granular control (like running specific tags):

**Linux / macOS:**
```bash
./tests/asioscan_tests
./tests/asioscan_tests [cli]
```

**Windows:**
```powershell
.\tests\Release\asioscan_tests.exe
```