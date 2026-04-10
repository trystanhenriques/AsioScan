# Output Formats

AsioScan provides multiple output formats to accommodate both human-readable console use and structured data parsing for automated workflows. The tool guarantees deterministic formatting and reliable exit codes to ensure easy integration into scripts and pipelines.

## Overview

By default, AsioScan prints a standard human-readable text report to `stdout`. Using CLI flags, you can alter the verbosity of this output or switch the format entirely to XML.

All error messages, warnings, and diagnostic information are exclusively written to `stderr`. This ensures that if you redirect `stdout` or write to a file, the structure of the generated report remains clean and valid.

---

## Text Output

The default text formatter renders a structured view designed for the console. It groups results by target host.

### Normal Text Mode
The default mode includes a report header with timestamps, a section for each scanned host featuring a port state table, and a summary footer with scan statistics.

*Example excerpt:*
```text
========================================
AsioScan Scan Report
========================================

Scan start time: 2023-10-14 10:00:00
Scan end time:   2023-10-14 10:00:01
Scan duration:   1.05s

----------------------------------------
Host: 127.0.0.1
Host scan duration: 1005 ms

PORT     STATE
80       Closed      
443      Open        

========================================
Scan Summary
========================================

Hosts scanned: 1
Ports scanned: 2
Open ports:    1
Total duration: 1.05s
```

*Note: Depending on flags like `-r` (reason) and `-v` (verbose), additional columns such as `LATENCY(ms)` and `REASON` are appended to the port tables.*

### Specialized Text Modes
AsioScan provides alternative text modes to simplify the output stream:
* **Quiet (`-q`)**: Prints only open ports in a minimal format (e.g., `127.0.0.1:443 open`).
* **Summary (`--summary`)**: Prints only the `Scan Summary` footer.
* **Ports-only (`--ports-only`)**: Prints host sections followed by simple port enumerations (e.g., `Host: 127.0.0.1` and `443/tcp open`).
* **Hosts-only (`--hosts-only`)**: Prints a single line per host indicating general availability (e.g., `127.0.0.1: up (1 open port)`).

### File Text Output
You can redirect the text report to a file using `-o` or `--oN`:
```bash
asioscan 127.0.0.1 -p 80 -o results.txt
```

---

## XML Output

For machine-parseable data, AsioScan supports generating structured XML reports using the `--oX` or `--output-xml` flags. 

The XML format guarantees a stable schema encompassing scan configurations, sequential host processing, detailed port mappings, and robust escaping of XML control characters. 

*Example excerpt (`--oX results.xml`):*
```xml
<?xml version="1.0"?>
<asioscan version="0.1.0">
  <scaninfo type="connect" timeout="500" unit="ms" concurrency="200" ports="80,443" />
  <host>
    <address addr="127.0.0.1"/>
    <ports>
      <port protocol="tcp" portid="80">
        <state state="closed" reason="Connection refused"/>
      </port>
      <port protocol="tcp" portid="443">
        <state state="open" reason="Connection established"/>
      </port>
    </ports>
  </host>
  <runstats up="1" down="0" total="1" ports-scanned="2" open="1" closed="1" filtered="0" duration="1.05" unit="seconds"/>
</asioscan>
```

---

## Expected Behavior and Errors

### Stderr Delivery
AsioScan enforces strict hygiene between standard out (`stdout`) for results and standard error (`stderr`) for diagnostics. If an argument is invalid or a host cannot be resolved, an error will be printed to `stderr` and will not pollute the XML or Text file data.

### Exit Codes
AsioScan uses reliable exit codes for shell scripting:
* `0`: Success. The parsed configuration was valid, and the active scan executed and successfully wrote to the requested output destination.
* `1`: Error. The program encountered bad CLI arguments, failed to resolve an output file handle, or caught an unhandled runtime error.