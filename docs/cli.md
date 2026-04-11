# AsioScan CLI Reference

This document provides a detailed reference for all command-line arguments and options supported by AsioScan.

## Command Syntax

The general syntax for running AsioScan is:

```bash
asioscan [OPTIONS] [TARGETS...]
```

* **TARGETS**: One or more hostnames or IP addresses to scan. At least one target is required.
* **OPTIONS**: Flags controlling port selection, scan behavior, and output formatting.

*Example:*
```bash
asioscan 127.0.0.1 192.168.1.1 -p 22,80,443 -t 1000 --oX results.xml
```

---

## Target Specification

Targets are provided as positional arguments at the end of the command or mixed with options.

* **Syntax**: `[target1] [target2] ...`
* **Purpose**: Specifies the network hosts or IP addresses to be scanned.
* **Constraints**: 
  * At least one target must be provided.
  * Targets are resolved by the local OS resolver if provided as hostnames.

*Example:*
```bash
asioscan scanme.nmap.org 8.8.8.8 -p 443
```

---

## Port Selection

* **Flags**: `-p`, `--ports` **(Required)**
* **Purpose**: Defines which TCP ports to scan on the provided targets.
* **Syntax**: Accepts a single port, comma-separated lists, dash-separated ranges, or a mix of all three.
* **Constraints**: 
  * Valid port numbers must be between `1` and `65535`.
  * Malformed lists or ranges where the start is greater than the end will result in an error.

*Examples:*
* Single port: `-p 80`
* List of ports: `-p 22,80,443`
* Range of ports: `-p 1-1024`
* Mixed representation: `-p 22,80,1000-1010`

---

## Timeout and Concurrency

AsioScan operates asynchronously. These controls dictate how aggressively it scans.

### Timeout
* **Flags**: `-t`, `--timeout`
* **Purpose**: Sets the maximum time (in milliseconds) to wait for a connection to be established for each port.
* **Constraints**: Must be between `1` and `300,000` (5 minutes). 
* **Default**: `500` ms

*Example:*
```bash
asioscan 127.0.0.1 -p 80 -t 1000
```

### Concurrency
* **Flags**: `-c`, `--concurrency`
* **Purpose**: Limits the maximum number of concurrent in-flight connection attempts across all targets.
* **Constraints**: Must be between `1` and `10,000`.
* **Default**: `200`

*Example:*
```bash
asioscan 192.168.1.1 192.168.1.2 -p 1-1024 -c 500
```

---

## Output Control

These flags modify what information is printed to the output. The following text mode flags are **mutually exclusive** (you can only use one at a time):

* `-q`, `--quiet`: Show only open ports (minimal, uncluttered output).
* `--summary`: Show only the scan summary statistics footer.
* `--ports-only`: Show the per-host port tables, but ignore host-level metadata.
* `--hosts-only`: Show only host availability (whether the host responded), skipping individual port tables.
* `-v`, `--verbose`: Show extended detail per port during the scan.

### Additional Display Flags

* **Flags**: `-r`, `--reason`
* **Purpose**: Includes the specific connection reason in text outputs (e.g., `Connection refused`, `Timeout`).

*Example:*
```bash
asioscan 127.0.0.1 -p 80 -r
```

---

## Output Formats and Destinations

By default, AsioScan prints human-readable text output to `stdout`. You can redirect output to a file and specify structural formats. The following options are **mutually exclusive**:

### Normal Text Output
* **Flags**: `-o`, `--output`, `--oN`
* **Purpose**: Writes standard human-readable text output to the specified file instead of the console.
* **Constraints**: The provided file path must not be blank.

*Example:*
```bash
asioscan 127.0.0.1 -p 80 -o results.txt
```

### XML Output
* **Flags**: `--oX`, `--output-xml`
* **Purpose**: Writes structured XML output to the specified file for integration with other tools or automated pipelines.
* **Constraints**: The provided file path must not be blank.

*Example:*
```bash
asioscan 127.0.0.1 -p 80-100 --oX results.xml
```

---

## Help & Version

* **Flags**: `-h`, `--help`
* **Purpose**: Prints the default help message, showing all available flags and quick usage examples, then exits cleanly.