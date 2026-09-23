# VMArea Multi-Phase Project Roadmap

This document outlines the sequential development phases of the **VMArea** hypervisor. Each phase establishes a stable foundation required by subsequent stages. 

> [!IMPORTANT]
> Each phase must reach full completion and verification before work on any subsequent phase begins. Phases 3 through 5 represent speculative research directions, not binding architectural commitments.

---

## Roadmap Overview

```
+--------------------------------------------------------------------------+
| Phase 1: Minimal Proof-of-Concept [STATUS: COMPLETE]                     |
| - Windows Hypervisor Platform (WHP) VM Partition                         |
| - 16-Bit Real-Mode Guest Kernel                                          |
| - COM1 Serial Console Emulation (0x3F8)                                  |
| - Clean Boot & Halt Banner Output                                        |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
| Phase 2: Protected Mode & Core Platform Services [PROPOSED / UNIMPLEMENTED|
| - 32-Bit Protected Mode Execution (GDT / IDT Setup)                      |
| - Interrupt Subsystem (8259 PIC emulation)                               |
| - Two-Level Paging (CR3, Page Tables)                                    |
| - Structured Multiboot-Style Boot Protocol                               |
| - Basic Devices: PIT (8254 Timer), Keyboard (8042 PS/2)                  |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
| Phase 3: 64-Bit Long Mode & System Architecture [FUTURE CONCEPT]         |
| - 64-Bit Long Mode Transition (4-Level Paging PML4)                      |
| - UEFI-Style Boot Sequence / Payload Hand-off                            |
| - Local APIC & IOAPIC Emulation                                          |
| - Extensible VirtIO Device Framework (virtio-blk, virtio-net)            |
| - Pre-emptive Multitasking & Kernel Primitives                           |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
| Phase 4: Darwin / XNU Compatibility Research [SPECULATIVE RESEARCH]     |
| - Research Mach-Compatible IPC & Port Abstractions                       |
| - Investigate Minimal XNU Kernel Interfaces                              |
| - Basic BSD-Layer POSIX Compatibility & Syscall Translation              |
| - Mach-O 64-Bit Binary Loader Feasibility                                |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
| Phase 5: Developer Tool Integration Research [SPECULATIVE RESEARCH]      |
| - Investigate Xcode / iOS Simulator Toolchain Interoperability           |
| - Simulator Runtime Wrapper Protocols                                    |
| - Headless Debugging & GDB / LLDB Remote Stub Integration                |
+--------------------------------------------------------------------------+
```

---

## Detailed Phase Breakdown

### Phase 1: Minimal Proof-of-Concept
*Status: **Complete***

The objective of Phase 1 is to validate the Windows Hypervisor Platform API integration, allocate guest physical memory, launch a virtual CPU, and run a minimal 16-bit real-mode guest that communicates over an emulated serial port.

- **Entry Criteria**: Empty project or prototyping branch on Windows 10/11 with WHP enabled.
- **Deliverables**:
  - `host/vmm`: WHP partition lifecycle manager (`WHvCreatePartition`, `WHvSetupPartition`, `WHvRunVirtualProcessor`).
  - `host/memory`: 1 MB guest physical address space mapped via `VirtualAlloc` and `WHvMapGpaRange`.
  - `host/cpu`: vCPU initialization with 16-bit real-mode registers and segment attributes.
  - `host/devices`: COM1 serial port emulation (`0x3F8`–`0x3FD`) intercepting I/O exits to host `stdout`.
  - `guest/kernel`: NASM flat binary outputting `"Darwin-like environment booted successfully.\r\n"` and issuing `hlt`.
  - Automated build scripts (`build.cmd`, `run-vm.cmd`, `test.cmd`) and CMake configuration.
- **Verification**: `run-vm.cmd` boots guest, prints the banner cleanly to terminal, and shuts down without memory leaks or crashes.

---

### Phase 2: Protected Mode & Core Platform Services
*Status: **Proposed / Not Implemented***

Phase 2 transitions the guest from 16-bit real mode into 32-bit protected mode, establishes memory paging, and emulates core PC interrupt and timing infrastructure.

- **Entry Criteria**: Phase 1 fully passing all test suites and functioning deterministically across host environments.
- **Planned Deliverables**:
  - **32-Bit Protected Mode**: Global Descriptor Table (GDT), Interrupt Descriptor Table (IDT), Task State Segment (TSS).
  - **Memory Management**: 32-bit two-level paging enabled via `CR0.PG` and `CR3` directory tables; basic virtual memory page frame allocator.
  - **Structured Boot Protocol**: Bootloader handover protocol passing memory maps, framebuffer pointers, and boot parameters.
  - **Interrupt Architecture**: Dual 8259 Programmable Interrupt Controller (PIC) emulation handling IRQ line routing.
  - **Timing & Input**: 8254 Programmable Interval Timer (PIT) providing periodic ticks; 8042 PS/2 controller for keyboard input emulation.
- **Verification**: A 32-bit protected-mode guest kernel receives timer interrupts, handles page faults cleanly, and manages memory dynamically.

---

### Phase 3: 64-Bit Long Mode & System Architecture
*Status: **Future Concept***

Phase 3 transitions the platform to x86-64 long mode, introduces modern firmware/UEFI boot semantics, and supports modern high-performance virtual devices.

- **Entry Criteria**: Phase 2 protected-mode kernel, interrupt subsystem, and device framework fully stabilized.
- **Planned Deliverables**:
  - **x86-64 Long Mode**: 4-level paging (PML4, PDPT, PD, PT) with `CR4.PAE` and `EFER.LME/LMA`.
  - **Virtual APIC**: Emulation of Local APIC and I/O APIC replacing legacy 8259 PIC.
  - **UEFI Boot Flow**: Adherence to UEFI specification or modern 64-bit boot protocols (e.g., Limine/Stivale2/Multiboot2).
  - **VirtIO Device Subsystem**: Support for VirtIO MMIO or PCI transports (virtio-console, virtio-blk, virtio-net).
  - **SMP Virtualization**: Support for multiple virtual CPUs with Inter-Processor Interrupts (IPI).
- **Verification**: 64-bit guest kernel boots, initializes 64-bit flat address space, and executes preemptive multitasking across virtual CPUs.

---

### Phase 4: Darwin / XNU Compatibility Research
*Status: **Speculative Research Direction***

Phase 4 investigates the technical feasibility of providing Darwin/XNU kernel interfaces, Mach IPC semantics, and BSD-compatible system abstractions within the virtualized guest.

- **Entry Criteria**: Phase 3 64-bit long-mode kernel environment fully operational.
- **Research Topics & Objectives**:
  - **Mach IPC Primitives**: Investigate message passing, ports (`mach_port_t`), port sets, and IPC spaces.
  - **XNU Interfaces**: Research task, thread, and virtual memory (`vm_map`) data structures compatible with Darwin expectations.
  - **Mach-O Binary Loader**: Design a loader capable of parsing 64-bit Mach-O headers (`MH_EXECUTE`), segments (`__TEXT`, `__DATA`), and section relocations.
  - **BSD Syscall Translation**: Analyze feasibility of translating POSIX/BSD system calls or hosting a lightweight BSD compatibility personality.
- **Verification**: Standalone user-mode Mach-O binary loaded and executed, interacting with guest Mach IPC primitives.

---

### Phase 5: Developer Tool Integration Research
*Status: **Speculative Research Direction***

Phase 5 examines possibilities for integrating the virtualization platform with external development tools and workflows.

- **Entry Criteria**: Phase 4 research concludes with a viable Darwin/BSD execution layer.
- **Research Topics & Objectives**:
  - **Simulator Runtime Protocols**: Investigate requirements for running simulator frameworks or containerized build artifacts.
  - **Debugging Interfaces**: Implement an in-hypervisor GDB/LLDB remote serial stub (`gdbserver` protocol) for source-level debugging of guest binaries.
  - **Host Tooling Bridges**: Explore filesystem sharing (VirtIO-FS / 9P) and socket bridges between Windows host tools and the virtual guest.
- **Verification**: Remote debugger (LLDB) attaches to a running guest process; guest executes test binaries dispatched from host tooling.

---

## Governance & Phase Discipline

1. **Sequential Progression**: No code for Phase $N+1$ may be committed until Phase $N$ has achieved its defined verification targets.
2. **Backward Compatibility**: Enhancements introduced in subsequent phases must not break the ability to run minimal real-mode payloads like the Phase 1 guest.
3. **Speculative Boundary**: Phases 4 and 5 are open-ended research investigations; their deliverables depend on feasibility assessments gathered during Phase 3.
