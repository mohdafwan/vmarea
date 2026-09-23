# VMArea Architecture Specification

This document details the architectural design, execution model, component breakdown, memory layout, and virtualization lifecycle of the **VMArea** hypervisor.

---

## 1. System Overview

VMArea is a user-mode hypervisor built on top of the **Windows Hypervisor Platform (WHP / WHPX)**. It operates in Windows user space and uses native Windows hypervisor APIs to manage hardware-assisted virtualization (Intel VT-x or AMD-V) through the Windows hypervisor root partition.

```
+-----------------------------------------------------------------------------------+
|                                   HOST SYSTEM                                     |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  |                             VMArea Host Process                             |  |
|  |                                                                             |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |  |                       Vmm Orchestrator (vmm.cpp)                      |  |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |          |                                   |                    |         |  |
|  |          v                                   v                    v         |  |
|  |  +-------------------+              +------------------+  +--------------+  |  |
|  |  |   GuestMemory     |              |    VirtualCpu    |  | SerialConsole|  |  |
|  |  | - VirtualAlloc    |              | - Processor Mgmt |  | (0x3F8 COM1) |  |  |
|  |  | - WHvMapGpaRange  |              | - Register Setup |  | -> stdout    |  |  |
|  |  | - Binary Loader   |              | - Run Loop       |  |              |  |  |
|  |  +-------------------+              +------------------+  +--------------+  |  |
|  +----------------------------------------------|--------------------|---------+  |
|                                                 |                    |            |
|                                   WHP API Calls |                    | Exit Route |
|                                                 v                    |            |
|  +-------------------------------------------------------------------|---------+  |
|  |                Windows Hypervisor Platform User APIs (WinHvPlatform)|          |
|  |                   (WinHvPlatform.dll / WinHvEmulation.dll)          |          |
|  +-------------------------------------------------------------------|---------+  |
+-------------------------------------------------|--------------------|------------+
|                                                 v                    |            |
|  +-------------------------------------------------------------------|---------+  |
|  |                    Hyper-V Hypervisor Kernel Layer                |            |
|  +-------------------------------------------------------------------|---------+  |
|                                                 |                    |            |
|  +----------------------------------------------|--------------------|---------+  |
|  |               Hardware Virtualization Layer (Intel VT-x / AMD-V)  |            |
|  +----------------------------------------------|--------------------|---------+  |
+-------------------------------------------------|--------------------|------------+
                                                  |                    |
                                                  v                    | (VM Exit)
+----------------------------------------------------------------------|------------+
|                               GUEST VM PARTITION                     |            |
|                                                                      |            |
|  GPA Space (1 MB):                                                   |            |
|  [0x00000 - 0x0003F] Guest Kernel Flat Binary (kernel.asm)           |            |
|                      - Sets DS/SS/SP                                 |            |
|                      - Loops over string, writing chars to 0x3F8 ----+            |
|                      - Issues HLT instruction                                     |
|  [0x07C00 - 0x07FFF] Real-Mode Stack Region (SP = 0x7C00)                         |
|  [0x08000 - 0xFFFFF] Free Unused Space                                            |
+-----------------------------------------------------------------------------------+
```

---

## 2. Component Descriptions

VMArea is organized into distinct, modular components with clear separation of responsibilities:

### 2.1 Vmm (`host/vmm/vmm.h`, `host/vmm/vmm.cpp`)
The `Vmm` class serves as the top-level coordinator. It:
- Validates platform virtualization capabilities via `WHvGetCapability`.
- Allocates and configures the `WHV_PARTITION_HANDLE`.
- Coordinates `GuestMemory` and `VirtualCpu` creation and lifecycle.
- Hosts the virtual dispatch loop for VM exits.
- Ensures deterministic teardown and resource release upon completion.

### 2.2 GuestMemory (`host/memory/guest_memory.h`, `host/memory/guest_memory.cpp`)
The `GuestMemory` class manages guest physical address (GPA) space:
- Allocates page-aligned contiguous host virtual memory using `VirtualAlloc(..., MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)`.
- Maps this backing buffer to GPA `0x0` through `WHvMapGpaRange` with read, write, and execute permissions.
- Loads raw flat guest binaries from disk directly into the mapped backing memory.
- Unmaps the GPA range (`WHvUnmapGpaRange`) and frees host virtual memory (`VirtualFree`) upon destruction.

### 2.3 VirtualCpu (`host/cpu/vcpu.h`, `host/cpu/vcpu.cpp`)
The `VirtualCpu` class encapsulates execution of a single virtual processor:
- Creates a virtual processor (`WHvCreateVirtualProcessor`) with index 0.
- Initializes all 16-bit real-mode x86 register contexts via `WHvSetVirtualProcessorRegisters`.
- Executes `WHvRunVirtualProcessor` inside an event loop.
- Decodes the `WHV_RUN_VP_EXIT_CONTEXT` struct to inspect exit reasons and triggers appropriate handlers.
- Tears down the processor using `WHvDeleteVirtualProcessor`.

### 2.4 SerialConsole (`host/devices/serial_console.h`, `host/devices/serial_console.cpp`)
The `SerialConsole` class emulates a standard 8250/16550-compatible PC serial port (COM1):
- Listens to port address range `0x3F8`–`0x3FF`.
- Emulates the Transmitter Holding Register (THR) at base offset `0x3F8`.
- Emulates the Line Status Register (LSR) at offset `0x3FD`, returning bit 5 (Transmitter Holding Register Empty) set so guest polling succeeds immediately.
- Flushes written characters directly to the host console (`stdout`).

### 2.5 Guest Kernel (`guest/kernel/kernel.asm`)
The guest payload is a minimal 16-bit real-mode x86 flat binary assembled with NASM:
- Assembled with `[org 0x0000]` and `[bits 16]`.
- Sets segment registers (`ds = 0`, `es = 0`, `ss = 0`) and stack pointer (`sp = 0x7C00`).
- Loops over a null-terminated string and writes each byte to COM1 Data Port (`0x3F8`) using the `out dx, al` instruction.
- Halts execution with an explicit `hlt` instruction.

---

## 3. WHP API Virtualization Lifecycle

The VMM interacts with the Windows Hypervisor Platform through a strict sequence of API calls:

```
+-------------------------------------------------------------------+
| 1. Query Hypervisor Capability (WHvGetCapability)                 |
|    - Verify WHvCapabilityCodeHypervisorPresent                    |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 2. Create Partition (WHvCreatePartition)                          |
|    - Allocates WHV_PARTITION_HANDLE                               |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 3. Configure Partition (WHvSetPartitionProperty)                  |
|    - Set WHvPartitionPropertyCodeProcessorCount = 1               |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 4. Setup Partition (WHvSetupPartition)                            |
|    - Commits configuration; locks partition properties            |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 5. Map Guest Physical Memory (WHvMapGpaRange)                     |
|    - Map host VirtualAlloc buffer to GPA 0x0 (1MB, RWX)           |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 6. Create Virtual Processor (WHvCreateVirtualProcessor)           |
|    - VP index 0                                                   |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 7. Set Registers (WHvSetVirtualProcessorRegisters)                |
|    - CS:IP = 0x0000:0x0000, CR0, RFLAGS, Segment Descriptors     |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 8. Execution Run Loop (WHvRunVirtualProcessor)                    |<---+
|    - Execute instructions until VM Exit occurs                    |    |
|    - Handle Exit:                                                 |    |
|      * WHvRunVpExitReasonX64IoPortAccess -> Route to SerialConsole|----+
|      * WHvRunVpExitReasonHalt            -> Normal Shutdown       |
|      * Other / Error                     -> Abort Loop            |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 9. Teardown Virtual Processor (WHvDeleteVirtualProcessor)         |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 10. Unmap Guest Memory (WHvUnmapGpaRange) & Free Virtual Memory    |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 11. Delete Partition (WHvDeletePartition)                         |
+-------------------------------------------------------------------+
```

---

## 4. Guest Execution Model

- **Architecture Mode**: 16-bit x86 Real Address Mode.
- **Entry Point**: `CS:IP = 0x0000:0x0000` (physical address `0x00000000`).
- **Addressing Mode**: Segmented addressing where physical address = `Segment * 16 + Offset`.
- **I/O Mechanism**: Port-mapped I/O (`in` and `out` assembly instructions).
- **Console Transport**: Standard PC COM1 UART at I/O port `0x3F8`. The guest writes characters sequentially to this port.
- **Termination**: The guest issues a `hlt` instruction. When interrupts are disabled (`cli`) or unconfigured, `hlt` suspends CPU execution indefinitely, triggering a clean VM exit to the hypervisor host.

---

## 5. VM Exit Handling

During `WHvRunVirtualProcessor`, hardware virtualization traps certain operations back to the host root partition. VMArea handles the following exit reasons:

### 5.1 `WHvRunVpExitReasonX64IoPortAccess`
Occurs when the guest executes `in` or `out` instructions:
- **Port Matching**: Checked against `0x3F8`–`0x3FF` via `SerialConsole::ownsPort(port)`.
- **Write Operations (`AccessInfo.IsWrite == 1`)**:
  - If `Port == 0x3F8`, the transmitted character is extracted from `ExitContext.IoPortAccess.Data` and forwarded to `SerialConsole::handleWrite()`, which writes it to host `stdout`.
- **Read Operations (`AccessInfo.IsWrite == 0`)**:
  - If `Port == 0x3FD` (Line Status Register), VMM returns `0x60` (Transmitter Empty and Transmitter Holding Register Empty) to satisfy polling loops.
- **Instruction Pointer Advance**: WHP provides `ExitContext.VpContext.InstructionLength`. The VMM increments `RIP` by this length using `WHvSetVirtualProcessorRegisters` to resume past the I/O instruction.

### 5.2 `WHvRunVpExitReasonHalt`
Occurs when the guest executes `hlt`:
- Signifies clean completion of guest kernel tasks in Phase 1.
- The VMM logs successful shutdown and exits the execution run loop with a success status.

### 5.3 Memory Access Faults & Unexpected Exits
- `WHvRunVpExitReasonMemoryAccess`: Indicates unmapped GPA access or violation of page protection permissions. VMM logs GPA, fault flags, and aborts.
- `WHvRunVpExitReasonUnrecoverableException` / `WHvRunVpExitReasonInvalidVpRegisterValue`: Indicates architectural fault or invalid CPU state; VMM dumps register state and exits.

---

## 6. Memory Layout

VMArea allocates a contiguous 1 MB block of guest physical memory (GPA `0x00000000` through `0x000FFFFF`):

```
+------------------+ 0x00100000 (1 MB)
|                  |
|  Unused Guest    |
|  Memory          |
|                  |
+------------------+ 0x00008000
| Real-Mode Stack  | (grows downward from 0x7C00)
+------------------+ 0x00007C00
| Free Buffer      |
+------------------+ 0x00000040 (~64 bytes)
| Guest Kernel     | Flat binary loaded at GPA 0x0
| (kernel.asm)     | Entry point at 0x0000:0x0000
+------------------+ 0x00000000
```

- **GPA 0x00000000**: Guest kernel origin. The binary payload is copied directly here.
- **GPA 0x00007C00**: Initial top of stack (`SS = 0x0000`, `SP = 0x7C00`). Standard PC BIOS boot sector convention.
- **Total Mapped Size**: `0x100000` bytes (1,048,576 bytes / 1 MB), aligned to a 4 KB host page boundary.

---

## 7. vCPU Register Initialization

Setting up x86 real mode within a hardware-virtualized partition (VMX/SVM) requires specific register attributes and hidden segment descriptor cache fields:

| Register | Value | Description |
|---|---|---|
| `RIP` | `0x00000000` | Instruction Pointer at origin |
| `RFLAGS` | `0x00000002` | Bit 1 reserved (must be 1); IF=0, TF=0 |
| `CR0` | `0x00000020` | Real-mode (PE=0, NE=1) |
| `CR4` | `0x00000000` | Baseline extensions disabled |
| `CS` | Selector: `0x0000`, Base: `0x0`, Limit: `0xFFFF`, Attr: `0x009B` (Code, Read/Exec) | Real-mode 64KB code segment descriptor |
| `DS` | Selector: `0x0000`, Base: `0x0`, Limit: `0xFFFF`, Attr: `0x0093` (Data, Read/Write) | Real-mode 64KB data segment descriptor |
| `ES` | Selector: `0x0000`, Base: `0x0`, Limit: `0xFFFF`, Attr: `0x0093` (Data, Read/Write) | Real-mode 64KB extra segment descriptor |
| `FS` | Selector: `0x0000`, Base: `0x0`, Limit: `0xFFFF`, Attr: `0x0093` (Data, Read/Write) | Real-mode 64KB segment descriptor |
| `GS` | Selector: `0x0000`, Base: `0x0`, Limit: `0xFFFF`, Attr: `0x0093` (Data, Read/Write) | Real-mode 64KB segment descriptor |
| `SS` | Selector: `0x0000`, Base: `0x0`, Limit: `0xFFFF`, Attr: `0x0093` (Data, Read/Write) | Real-mode 64KB stack segment descriptor |
| `RSP` | `0x00007C00` | Stack pointer top |
| `GDTR` | Base: `0x0`, Limit: `0xFFFF` | Flat limit |
| `IDTR` | Base: `0x0`, Limit: `0x03FF` | Real-mode IVT limit (1024 bytes) |

---

## 8. Design Decisions & Rationale

- **Why 16-Bit Real Mode for Phase 1?**
  Real mode minimizes execution complexity. It does not require building paging tables (PML4/PDPT/PD/PT), loading Global Descriptor Tables (GDT), or managing privilege transitions. It provides the fastest path to verify hardware virtualization and hypervisor communication.
- **Why COM1 Serial Console?**
  Serial port I/O via `out dx, al` requires no virtual video memory, framebuffer rendering, or font glyph blitting. Port I/O causes synchronous VM exits that are simple to intercept and route to standard host output.
- **Why Flat Binary?**
  A raw flat binary (`.bin`) has no file headers, relocation tables, or format overhead (unlike ELF or PE/COFF). The hypervisor can read the bytes and copy them 1:1 into the base of guest memory.
- **Why Windows Hypervisor Platform (WHP)?**
  WHP is the official, supported hypervisor API for Windows (coexisting with WSL2, Windows Sandbox, and Hyper-V). It does not require custom kernel-mode drivers or disabling Hyper-V.

---

## 9. Known Limitations (Phase 1)

1. **Single vCPU**: Exactly one virtual processor (index 0) is created and scheduled.
2. **No Hardware Interrupts**: Virtual 8259 PIC or APIC is not emulated; interrupts remain masked (`cli`).
3. **No Protected or Long Mode**: Paging, segmentation descriptors beyond flat 16-bit, and 64-bit page translation are not implemented.
4. **No Block or Storage Devices**: The guest runs entirely out of initial RAM loaded at start; no virtual disk or NVMe controller is attached.
5. **Synchronous Run Loop**: The VMM execution thread blocks on `WHvRunVirtualProcessor` until a VM exit occurs.

---

## 10. Security Considerations

- **Hardware Isolation**: The guest executes in VMX non-root mode (or AMD-V guest mode). CPU isolation and memory boundaries are enforced by hardware CPU extensions.
- **Memory Containment**: Guest code cannot access host virtual memory outside the explicit range registered with `WHvMapGpaRange`.
- **Privilege Separation**: Even though the guest kernel operates at ring 0 within the virtual machine, it has zero host privileges and cannot execute privileged root-mode hypervisor instructions.
- **Input Sanitization**: Serial console handlers only consume byte-level character writes and do not perform unchecked pointer dereferences.
