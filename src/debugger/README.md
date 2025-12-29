# openMSX Debug Server

HTTP and Telnet-based debugging interface for openMSX emulator.

## Features

- **Pure C++17 implementation** - No external dependencies
- **GPL-2.0 License** - Compatible with openMSX
- **Cross-platform** - Works on Linux, macOS, and Windows
- **4 HTTP ports** for pull-based debug information
- **1 Stream port** for push-based real-time streaming
- **JSON API** - Easy integration with external tools
- **HTML Dashboard** - Real-time browser-based monitoring
- **JSON Lines Streaming** - Real-time event streaming via Telnet

## Server Ports

### HTTP Servers (Pull-based)

| Port  | Type     | Description                              |
|-------|----------|------------------------------------------|
| 65501 | Machine  | Machine info, slots, extensions          |
| 65502 | I/O      | I/O ports, slot mapping                  |
| 65503 | CPU      | Z80/R800 registers, flags, interrupts    |
| 65504 | Memory   | Memory dumps, slot information           |

### Stream Server (Push-based)

| Port  | Protocol | Description                              |
|-------|----------|------------------------------------------|
| 65505 | Telnet   | Real-time JSON Lines debug streaming     |

The stream server outputs data according to the **OUTPUT_SPEC_V01** specification with `"emu":"msx"` identifier.

## Settings

openMSX settings can be configured via the console or settings file:

### HTTP Server Settings

```tcl
# Enable/disable HTTP debug server
set debug_http_enable true

# Port configuration (default values shown)
set debug_http_machine_port 65501
set debug_http_io_port 65502
set debug_http_cpu_port 65503
set debug_http_memory_port 65504
```

### Stream Server Settings

```tcl
# Enable/disable stream server
set debug_stream_enable true

# Stream port configuration (default: 65505)
set debug_stream_port 65505
```

## HTTP API Reference

### Machine Info (Port 65501)

```
GET /           - HTML Dashboard
GET /api/info   - Machine information (JSON)
```

Example response:
```json
{
  "timestamp": 1704067200000,
  "status": "running",
  "machine_id": "Panasonic_FS-A1F",
  "machine_name": "Panasonic FS-A1F",
  "machine_type": "MSXturboR",
  "slots": { ... },
  "extensions": ["fmpac", "ide"],
  "cpu_type": "R800"
}
```

### CPU Info (Port 65503)

```
GET /           - HTML Dashboard
GET /api/info   - CPU registers and flags (JSON)
```

Example response:
```json
{
  "timestamp": 1704067200000,
  "status": "running",
  "registers": {
    "af": "F3A0", "bc": "0100", "de": "0000", "hl": "C000",
    "af2": "0000", "bc2": "0000", "de2": "0000", "hl2": "0000",
    "ix": "0000", "iy": "0000", "sp": "F380", "pc": "0000",
    "i": "00", "r": "00"
  },
  "flags": { "s": false, "z": true, "h": false, "pv": false, "n": false, "c": false },
  "interrupts": { "iff1": true, "iff2": true, "im": 1, "halted": false },
  "cpu_type": "Z80"
}
```

### Memory Info (Port 65504)

```
GET /                               - HTML Dashboard
GET /api/info?start=0x0000&size=256 - Memory dump (JSON)
```

## Stream Server (Port 65505)

The stream server provides real-time push-based debug information via Telnet protocol.

### Connection

```bash
telnet 127.0.0.1 65505
# or
nc 127.0.0.1 65505
```

### Output Format (JSON Lines)

Each line is a complete JSON object:

```json
{"emu":"msx","cat":"sys","sec":"conn","fld":"hello","val":"1.0","ts":1704067200000}
{"emu":"msx","cat":"mach","sec":"info","fld":"id","val":"Panasonic_FS-A1F"}
{"emu":"msx","cat":"mach","sec":"info","fld":"name","val":"Panasonic FS-A1F"}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"af","val":"F3A0"}
{"emu":"msx","cat":"cpu","sec":"reg","fld":"pc","val":"0000"}
{"emu":"msx","cat":"cpu","sec":"flags","fld":"all","val":"SZ-H-PNC","raw":"F3"}
{"emu":"msx","cat":"mem","sec":"slot","fld":"page0","val":"0","addr":"0000"}
{"emu":"msx","cat":"dbg","sec":"bp","fld":"hit","val":"0","addr":"1234","ts":...}
```

### Categories

| cat    | Description      | Sections                    |
|--------|------------------|-----------------------------|
| `sys`  | System messages  | `conn` (hello/goodbye)      |
| `mach` | Machine info     | `info`, `status`            |
| `cpu`  | CPU state        | `reg`, `flags`, `state`, `int` |
| `mem`  | Memory access    | `access`, `slot`            |
| `dbg`  | Debug events     | `bp`, `trace`               |

### Initial Snapshot

Upon connection, the stream server sends a complete state snapshot:
1. Hello message with version
2. Machine information
3. All CPU registers
4. CPU flags
5. Interrupt state
6. Slot mapping for all pages

## File Structure

```
src/debugger/
├── Debugger.cc/hh             - Main debugger class
├── DebugHttpServer.cc/hh      - HTTP server manager
├── DebugHttpServerPort.cc/hh  - Individual HTTP port handler
├── DebugHttpConnection.cc/hh  - HTTP connection handler
├── DebugInfoProvider.cc/hh    - HTTP JSON generator
├── HtmlGenerator.cc/hh        - HTML dashboard generator
├── DebugTelnetServer.cc/hh    - Telnet stream server (port 65505)
├── DebugTelnetConnection.cc/hh - Telnet connection handler
├── DebugStreamFormatter.cc/hh - JSON Lines formatter (OUTPUT_SPEC_V01)
├── DasmTables.cc/hh           - Disassembly tables
├── Probe.cc/hh                - Debug probes
├── ProbeBreakPoint.cc/hh      - Breakpoint handling
├── SimpleDebuggable.cc/hh     - Base debuggable class
├── SymbolManager.cc/hh        - Symbol management
└── README.md                  - This file
```

## C++ API

### Accessing the Stream Server

```cpp
#include "DebugHttpServer.hh"

// Get the debug server instance
DebugHttpServer& debugServer = reactor.getDebugHttpServer();

// Check if streaming is active
if (debugServer.isStreamingActive()) {
    // Get the formatter
    DebugStreamFormatter* formatter = debugServer.getStreamFormatter();

    // Broadcast CPU registers
    debugServer.broadcastStreamData(formatter->getCPURegisters());

    // Broadcast breakpoint hit
    debugServer.broadcastStreamData(formatter->getBreakpointHit(0, 0x1234));
}
```

### DebugStreamFormatter Methods

| Method | Description |
|--------|-------------|
| `getHelloMessage()` | System hello message |
| `getGoodbyeMessage()` | System goodbye message |
| `getFullSnapshot()` | Complete state snapshot (vector of lines) |
| `getCPURegisters()` | All registers in one line |
| `getCPUFlags()` | Flag status |
| `getCPUState()` | Interrupt and CPU type info |
| `getMemoryRead(addr, val)` | Memory read event |
| `getMemoryWrite(addr, val)` | Memory write event |
| `getMemoryBank()` | Current slot mapping |
| `getMachineInfo()` | Machine identification |
| `getMachineStatus(mode)` | Current execution mode |
| `getBreakpointHit(idx, addr)` | Breakpoint hit event |
| `getTraceExec(addr, disasm)` | Instruction trace |

## Security Notes

- Default bind address is localhost only (`127.0.0.1`)
- No authentication is implemented
- Do not expose to public networks without additional security measures
- Consider using a reverse proxy with authentication for remote access

## Building

The debugger files are included in the main meson.build:

```meson
sources = files(
    # ... other files ...
    'debugger/DebugHttpServer.cc',
    'debugger/DebugHttpServerPort.cc',
    'debugger/DebugHttpConnection.cc',
    'debugger/DebugInfoProvider.cc',
    'debugger/DebugTelnetServer.cc',
    'debugger/DebugTelnetConnection.cc',
    'debugger/DebugStreamFormatter.cc',
    'debugger/HtmlGenerator.cc',
    # ... other files ...
)
```

## Related Documentation

- [OUTPUT_SPEC_V01.md](../../../RetroDeveloperEnvironmentProject_OUTPUT_SPEC_V01.md) - JSON Lines format specification
- [openMSX Documentation](https://openmsx.org/manual/)

## License

GPL-2.0 - Same as openMSX
