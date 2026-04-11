# Release & Distribution Strategy

> **Note:** AsioScan is currently in active development. There are **no official binary releases** published yet. This document outlines the planned distribution strategy for when the project reaches its first stable milestone.

## Release Goals

The primary distribution goal for AsioScan is to provide a frictionless experience for both end-users and developers. 
To achieve this, the release process will focus on:
* **Semantic Versioning**: Strict adherence to SemVer (vMAJOR.MINOR.PATCH) for predictable updates.
* **Standalone Executables**: Providing cleanly compiled, ready-to-run binaries that do not require users to install heavy C++ toolchains or Boost libraries on their system.
* **Automation**: Utilizing CI/CD pipelines to ensure tests run and binaries are deterministically built for every tag.

## Supported Target Platforms

When binary releases begin, they will target the following primary architectures natively:
* **Windows**: x64 (built via MSVC)
* **Linux**: x86_64 (built via GCC/Clang)
* **macOS**: Universal binaries supporting both Apple Silicon (ARM64) and Intel (x86_64)

## Source vs. Binary Distribution

### Source Distribution
Source distribution remains the foundation of the project. Developers and system packagers will always be able to clone the repository and build AsioScan using standard CMake workflows. The CMake setup is designed to automatically fetch testing frameworks and CLI dependencies smoothly.

### Binary Distribution
For network administrators and security professionals who just need the tool, pre-compiled binaries will be uploaded directly to GitHub Releases. 

## Planned Release Artifact Layout

Release artifacts will likely be distributed as simple `.zip` (Windows) and `.tar.gz` (Linux/macOS) archives containing the minimal footprint required to use the tool:

```text
asioscan-v0.1.0-linux-x64.tar.gz
??? asioscan        (The executable binary)
??? README.md       (Basic usage instructions)
??? LICENSE         (License information)
```

We do not currently plan to provide native installer packages (like `.msi` or `.deb`) in the initial release phases.

## Roadmap to First Release (v0.1.0)

Before the first official release is tagged and binaries are published, the following milestones need to be completed:

1. **Feature Stabilization:** Solidify the TCP connect scanning architecture and ensure the connection state logic is rock-solid across all target platforms.
2. **Output Format Finalization:** Ensure that the text and XML output schemas are stable, as automation scripts will depend on this structure not breaking between minor versions.
3. **Documentation:** Complete the initial user and developer documentation guides (currently in progress).
4. **CI/CD Pipeline:** Set up automated workflows (e.g., GitHub Actions) to compile the project, run Catch2 tests, and automatically bundle release artifacts on all three target operating systems.
