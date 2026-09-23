# VMArea

**VMArea** is an experimental Phase 1 minimal Darwin-like virtualization environment running on Windows. It leverages the native **Windows Hypervisor Platform (WHP / WHPX)** APIs to construct a lightweight virtual machine partition, map guest physical memory, initialize a virtual CPU, execute a minimal 16-bit real-mode guest kernel, and display a boot banner over an emulated serial console.

---

## Overview & Capabilities

VMArea demonstrates bare-metal hypervisor fundamentals on Windows using native platform virtualization APIs:

- **Partition & Hypervisor Management**: Initializes a WHP partition, queries hypervisor capabilities, configures processor counts, and handles partition setup.
- **Guest Physical Memory**: Allocates host virtual memory via `VirtualAlloc` and maps it into Guest Physical Address (GPA) space using `WHvMapGpaRange`.
- **vCPU Initialization & Execution**: Instantiates a virtual processor, sets up initial real-mode registers (CS, IP, DS, SS, SP, CR0, RFLAGS), and drives the execution loop (`WHvRunVirtualProcessor`).
- **Device Emulation**: Intercepts guest I/O port exits to emulate a standard PC COM1 UART serial port (`0x3F8`–`0x3FF`), routing guest console output directly to host `stdout`.
- **Minimal Guest Kernel**: A 16-bit real-mode NASM flat binary that transmits `"Darwin-like environment booted successfully.\r\n"` over COM1 and halts (`hlt`).

---

## Architecture Diagram

```
+-----------------------------------------------------------------------+
|                             Windows Host                              |
|                                                                       |
|  +-----------------------------------------------------------------+  |
|  |                     VMM Orchestrator (vmm.cpp)                  |  |
|  +-----------------------------------------------------------------+  |
|         |                                          |                  |
|         v                                          v                  |
|  +---------------------+            +------------------------------+  |
|  | GuestMemory         |            | VirtualCpu                   |  |
|  | - VirtualAlloc      |            | - WHvCreateVirtualProcessor  |  |
|  | - WHvMapGpaRange    |            | - WHvRunVirtualProcessor     |  |
|  | (1 MB GPA @ 0x0)    |            +------------------------------+  |
|  +---------------------+                           |                  |
|                                                    v (VM Exit)        |
|                                     +------------------------------+  |
|                                     | SerialConsole (0x3F8 COM1)   |  |
|                                     | -> Host stdout               |  |
|                                     +------------------------------+  |
|                                                    ^                  |
+----------------------------------------------------|------------------+
| Windows Hypervisor Platform (WHPX / Hyper-V Root)  |                  |
+----------------------------------------------------|------------------+
                                                     | (I/O Port Exit)
+----------------------------------------------------|------------------+
| Guest VM Partition (GPA Space)                     |                  |
|                                                    |                  |
|  0x0000:0x0000 +-----------------------------------+---------------+  |
|                | Guest Kernel (kernel.asm)                         |  |
|                | - Outputs string to port 0x3F8                    |  |
|                | - Executes HLT                                    |  |
|  0x0000:0x7C00 +---------------------------------------------------+  |
|                | Real-Mode Stack Top                               |  |
|  0x0000:0xFFFF +---------------------------------------------------+  |
|  ...           | Free Memory (1 MB Total)                          |  |
|  0x000F:0xFFFF +---------------------------------------------------+  |
+-----------------------------------------------------------------------+
```

---

## Project Structure

```
vmarea/
├── docs/                     # Detailed technical documentation
│   ├── ARCHITECTURE.md       # Architecture specification and design rationale
│   ├── BUILD.md              # Build instructions and environment setup
│   ├── DEVELOPMENT.md        # Developer guide and coding conventions
│   └── ROADMAP.md            # Multi-phase project roadmap
├── host/
│   ├── vmm/                  # VMM orchestrator (vmm.h, vmm.cpp)
│   ├── memory/               # Guest physical memory (guest_memory.h, guest_memory.cpp)
│   ├── cpu/                  # Virtual CPU management (vcpu.h, vcpu.cpp)
│   ├── devices/              # Virtual I/O devices (serial_console.h, serial_console.cpp)
│   └── loader/               # Guest binary/payload loader
├── guest/
│   ├── kernel/               # 16-bit real-mode guest kernel (kernel.asm)
│   └── boot/                 # Boot protocol definitions
├── shared/                   # Common types and definitions (types.h)
├── tests/                    # Unit and integration tests (test_vmm.cpp)
├── tools/                    # Utility scripts and tooling
├── CMakeLists.txt            # Root CMake build configuration
├── build.cmd                 # Automated Windows build script
├── run-vm.cmd                # Launcher script for the virtual machine
└── test.cmd                  # Test runner script
```

---

## Quick Start

### Prerequisites

Ensure your system meets the following requirements:
- **Operating System**: Windows 10 (version 1803+) or Windows 11 (64-bit)
- **Virtualization**: Hardware virtualization (Intel VT-x or AMD-V) enabled in BIOS/UEFI
- **Windows Feature**: "Windows Hypervisor Platform" enabled
- **Compiler**: Visual Studio 2019 or newer with the "Desktop development with C++" workload
- **Windows SDK**: Version 10.0.17134.0 or higher
- **Build System**: CMake 3.16 or newer
- **Assembler**: NASM 2.15 or newer (available on `PATH`)

### Build, Run, and Test

Open an **x64 Native Tools Command Prompt for VS** (or Command Prompt with build tools in PATH) as Administrator:

1. **Build the Project**
   ```cmd
   build.cmd
   ```
   This assembles the guest kernel (`guest_kernel.bin`) and compiles the host VMM executables (`run-vm.exe` and `test_vmm.exe`) into `build\bin\Release\`.

2. **Run the Virtual Machine**
   ```cmd
   run-vm.cmd
   ```

3. **Run the Test Suite**
   ```cmd
   test.cmd
   ```

---

## Expected Output

When running `run-vm.cmd`, the VMM boots the guest payload in the WHP partition, captures serial output from COM1, and halts upon receiving the guest `hlt` instruction:

```text
[VMM] Initializing Windows Hypervisor Platform...
[VMM] Partition created and configured.
[VMM] Mapped 1048576 bytes of guest memory at GPA 0x0.
[VMM] Loaded guest binary (size: 64 bytes) at GPA 0x0.
[VMM] Virtual processor 0 created and registers initialized.
[VMM] Starting guest execution...
Darwin-like environment booted successfully.
[VMM] Guest execution halted cleanly (Exit Reason: WHvRunVpExitReasonHalt).
[VMM] Cleaning up VM partition...
[VMM] VM execution completed.
```

---

## Documentation

For in-depth technical details, refer to the documents in `docs/`:

- [**Architecture Specification**](file:///home/af3an/xstudio/vmarea/docs/ARCHITECTURE.md): System design, WHP API flow, memory layout, register initialization, exit handling, and security model.
- [**Build Guide**](file:///home/af3an/xstudio/vmarea/docs/BUILD.md): Detailed installation instructions, feature configuration, manual build steps, and troubleshooting.
- [**Development Guide**](file:///home/af3an/xstudio/vmarea/docs/DEVELOPMENT.md): Adding virtual devices, modifying the guest kernel, coding conventions, debugging techniques, and non-goals.
- [**Project Roadmap**](file:///home/af3an/xstudio/vmarea/docs/ROADMAP.md): Evolutionary milestones from Phase 1 proof-of-concept through prospective long-mode and Darwin/XNU research.

---

## License & Project Status

This repository is an **experimental and educational research project**. It serves as a study in hypervisor construction on Windows using WHP/WHPX and explores foundational virtualization concepts. It is not affiliated with, endorsed by, or derived from Apple Inc. or Microsoft Corporation.
