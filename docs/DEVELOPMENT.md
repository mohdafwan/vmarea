# VMArea Developer Guide

This document outlines the codebase organization, implementation conventions, extensibility patterns, debugging practices, and project scope boundaries for developers contributing to **VMArea**.

---

## 1. Code Organization & Responsibilities

The codebase is split into host hypervisor components, guest software, shared types, and test suites:

```
vmarea/
├── host/
│   ├── vmm/
│   │   ├── vmm.h               # VMM orchestrator interface
│   │   └── vmm.cpp             # Partition lifecycle & VM exit loop
│   ├── memory/
│   │   ├── guest_memory.h      # Guest physical address management interface
│   │   └── guest_memory.cpp    # VirtualAlloc & WHvMapGpaRange operations
│   ├── cpu/
│   │   ├── vcpu.h              # Virtual processor management
│   │   └── vcpu.cpp            # Register setup & WHvRunVirtualProcessor wrapper
│   ├── devices/
│   │   ├── serial_console.h    # 8250/16550 UART emulation interface
│   │   └── serial_console.cpp  # Port 0x3F8 I/O decode and stdout dispatch
├── guest/
│   └── kernel/
│       └── kernel.asm          # Minimal 16-bit real-mode guest kernel
├── shared/
│   └── types.h                 # Common typedefs, error codes, and constants
└── tests/
    ├── test_guest_sim.cpp      # Cross-platform device and guest simulation tests
    └── test_vmm.cpp            # Windows WHP integration tests
```

### Component Responsibilities
- **`host/vmm`**: High-level manager. Initializes the partition, configures properties, initializes memory, spawns vCPUs, coordinates exit handlers, and tears down state.
- **`host/memory`**: Manages host allocations (`VirtualAlloc`) and maps them into the WHP partition via `WHvMapGpaRange`. Provides binary loading into GPA memory.
- **`host/cpu`**: Manages the single WHP virtual processor, sets its initial real-mode register context (CS, IP, CR0, RFLAGS), and exposes `run()`.
- **`host/devices`**: Implements port-mapped I/O devices that hook into the VM exit loop.
- **`guest/kernel`**: Standalone guest payload assembled into flat binary form.

---

## 2. Coding Conventions

- **Language Standard**: Modern **C++17** (`/std:c++17`).
- **Namespace**: All host hypervisor classes and functions reside in the `vmarea` namespace (e.g., `vmarea::Vmm`, `vmarea::GuestMemory`, `vmarea::VirtualCpu`, `vmarea::SerialConsole`).
- **RAII (Resource Acquisition Is Initialization)**:
  - Partition handles (`WHV_PARTITION_HANDLE`), host memory allocations (`VirtualAlloc`), and file handles must be managed via RAII wrappers or destructors.
  - Never leak partition handles or leave GPA mappings dangling on error paths.
- **Error Handling Pattern**:
  - Functions returning status return `bool` (`true` on success, `false` on failure) or `HRESULT` when interfacing directly with Windows Hypervisor Platform APIs.
  - On failure, output human-readable diagnostic messages to `stderr` via `fprintf(stderr, ...)` with an explicit module tag, e.g.:
    ```cpp
    if (FAILED(hr)) {
        fprintf(stderr, "[VMM] Failed to create partition: 0x%08X\n", hr);
        return false;
    }
    ```
- **Naming Conventions**:
  - Classes and structs: `PascalCase` (e.g., `VirtualCpu`, `GuestMemory`).
  - Methods and functions: `camelCase` (e.g., `setupRegisters()`, `ownsPort()`).
  - Member variables: `camelCase_` or `m_camelCase` (e.g., `partition_`, `vcpuHandle_`).
  - Constants and macros: `UPPER_SNAKE_CASE` (e.g., `GUEST_MEMORY_SIZE`, `COM1_PORT_BASE`).

---

## 3. How to Add a New Virtual Device

All port-mapped virtual devices follow a consistent interface pattern:

### Step 1: Define the Device Interface
Create a new header in `host/devices/` (e.g., `debug_port.h`):

```cpp
#pragma once
#include <cstdint>

namespace vmarea {

class DebugPort {
public:
    DebugPort() = default;
    ~DebugPort() = default;

    // Checks if the I/O port belongs to this device
    bool ownsPort(uint16_t port) const noexcept {
        return (port == 0x00E9); // Bochs/QEMU debug port
    }

    // Handles write operations
    void handleWrite(uint16_t port, uint32_t value, uint8_t size) {
        if (port == 0x00E9) {
            char ch = static_cast<char>(value & 0xFF);
            putchar(ch);
        }
    }

    // Handles read operations
    uint32_t handleRead(uint16_t port, uint8_t size) {
        return 0xFF;
    }
};

} // namespace vmarea
```

### Step 2: Register in VMM Exit Handler
In `host/vmm/vmm.cpp`, add the device instance and route exits in the `WHvRunVpExitReasonX64IoPortAccess` branch:

```cpp
// Within VMM run loop:
if (exitContext.ExitReason == WHvRunVpExitReasonX64IoPortAccess) {
    const auto& ioAccess = exitContext.IoPortAccess;
    uint16_t port = ioAccess.PortNumber;

    if (serialConsole_.ownsPort(port)) {
        if (ioAccess.AccessInfo.IsWrite && ioAccess.AccessInfo.AccessSize == 1) {
            serialConsole_.handleWrite(port, static_cast<uint8_t>(ioAccess.Rax));
        }
    } else if (debugPort_.ownsPort(port)) {
        if (ioAccess.AccessInfo.IsWrite) {
            debugPort_.handleWrite(port, static_cast<uint32_t>(ioAccess.Rax),
                                  ioAccess.AccessInfo.AccessSize);
        }
    }

    // WHP resumes after the I/O instruction; do not modify RIP here.
}
```

---

## 4. How to Modify the Guest Kernel

The guest kernel lives at `guest/kernel/kernel.asm`.

### Step 1: Edit `kernel.asm`
The kernel is a 16-bit real-mode flat binary. For example:

```nasm
[bits 16]
[org 0x0000]

start:
    cli                     ; Disable interrupts
    xor ax, ax
    mov ds, ax              ; Data segment = 0
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Stack grows downward from 0x7C00

    mov si, boot_message

print_loop:
    lodsb                   ; Load byte at DS:SI into AL and increment SI
    test al, al             ; Check for null terminator
    jz done
    mov dx, 0x03F8          ; COM1 Data Port
    out dx, al              ; Transmit byte to serial console
    jmp print_loop

done:
    hlt                     ; Halt CPU (triggers VM Exit to host)
    jmp done

boot_message:
    db "Darwin-like environment booted successfully.", 0x0D, 0x0A, 0
```

### Step 2: Reassemble and Test
Reassemble the kernel:
```cmd
nasm -f bin guest/kernel/kernel.asm -o build/guest_kernel.bin
```

Launch the hypervisor with the new payload:
```cmd
build\bin\Release\run-vm.exe build\guest_kernel.bin
```

---

## 5. Debugging Tips

1. **Build with Debug Configuration**:
   ```cmd
   cmake --build build --config Debug
   ```
   This generates `.pdb` symbols and enables debug asserts.

2. **Inspect VM Exit Context**:
   When an unexpected VM exit occurs, print the detailed exit context:
   ```cpp
   printf("[DEBUG] VM Exit Reason: 0x%08X\n", exitContext.ExitReason);
   printf("[DEBUG] RIP: 0x%016llX\n", exitContext.VpContext.Rip);
   printf("[DEBUG] Instruction Length: %u\n", exitContext.VpContext.InstructionLength);
   ```

3. **Check WHP HRESULT Codes**:
   WHP errors return standard Windows `HRESULT` or Win32 error codes encoded via `HRESULT_FROM_WIN32`. Common error codes:
   - `0x80070032` (`ERROR_NOT_SUPPORTED`): Hypervisor not running or platform feature disabled.
   - `0x80070057` (`E_INVALIDARG`): Incorrect register attribute or invalid partition property.
   - `0xC0350005` (`WHV_E_INSUFFICIENT_BUFFER`): Structure size mismatch in WHP API structs.

4. **Instruction Single-Stepping**:
   Set `RFLAGS.TF = 1` (Trap Flag) in initial processor registers to trigger `WHvRunVpExitReasonCanceled` or single-step exits for fine-grained debugging.

---

## 6. Testing Strategy

VMArea tests are segregated into two tiers in `tests/`:

1. **Unit Tests (No WHP Needed)**:
   - Device logic (e.g., `SerialConsole::ownsPort` and character accumulation).
   - The assembled 16-bit guest's COM1 output and HLT behavior, simulated without WHP.
   - Run in any environment (including GitHub Actions or virtualized CI runners without nested virtualization).

2. **Integration Tests (Require WHP)**:
   - Partition creation, register initialization, and real-mode execution loop.
   - Capability probe: if `WHvGetCapability` reports WHP is unavailable, tests report **SKIP**, not pass or fail. A missing guest binary is a failure because CMake makes it a test dependency.
     ```cpp
     BOOL present = FALSE;
     UINT32 bytes = 0;
     HRESULT hr = WHvGetCapability(WHvCapabilityCodeHypervisorPresent, &present, sizeof(present), &bytes);
     if (FAILED(hr) || !present) {
         printf("[SKIP] WHP not available on this host. Skipping hypervisor integration tests.\n");
         return;
     }
     ```

---

## 7. Scope Boundaries (What NOT to Implement in Phase 1)

Phase 1 has a strictly defined, minimal scope. To avoid scope creep and preserve stability, the following components are **explicitly out of scope** for Phase 1:

- **Darwin / macOS Subsystems**:
  - Mach kernel subsystem (`mach_port_t`, Mach messages, IPC, task/thread ports).
  - XNU kernel layer, BSD abstractions (vnodes, sysctl, proc).
  - Apple frameworks (Foundation, CoreFoundation, AppKit, UIKit).
  - Objective-C runtime (`objc_msgSend`) or Swift runtime.
  - Metal API emulation or GPU command buffer pass-through.
  - Xcode toolchain integration or iOS Simulator runtime wrappers.
- **CPU & Architecture**:
  - 32-bit Protected Mode (GDT, LDT, IDT setup).
  - 64-bit Long Mode (PML4 paging tables, EFER.LME).
  - Multi-vCPU synchronization or SMP scheduling.
- **Hardware & Devices**:
  - Real hardware emulation (AHCI/NVMe controllers, e1000/virtio network adapters).
  - Interrupt controllers (8259 PIC, APIC, IOAPIC).
  - Paging hardware or MMU translation emulation.
  - Real-time clocks (RTC) or programmable interval timers (PIT).
- **Filesystems & Storage**:
  - APFS or HFS+ filesystem drivers.
  - Virtual block device drivers.
