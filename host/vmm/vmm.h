#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <WinHvPlatform.h>

#include <cstdint>
#include <string>
#include <memory>

namespace vmarea {

class GuestMemory;
class VirtualCpu;
class SerialConsole;

/// Vmm is the main Virtual Machine Monitor for Phase 1.
/// Orchestrates VM creation, memory setup, vCPU configuration,
/// guest loading, and the execution loop.
class Vmm {
public:
    Vmm();
    ~Vmm();

    Vmm(const Vmm&) = delete;
    Vmm& operator=(const Vmm&) = delete;

    /// Check WHP availability on this system.
    bool initialize();

    /// Create the VM partition and configure it.
    bool createVm();

    /// Allocate and map guest physical memory (default 1 MB).
    bool allocateMemory(size_t sizeBytes = 1024 * 1024);

    /// Create virtual CPU #0.
    bool createVcpu();

    /// Load a flat binary guest image and initialize the vCPU.
    bool loadGuest(const std::string& imagePath);

    /// Run the guest until it halts (blocking call).
    bool startGuest();

    /// Clean shutdown of all VM resources.
    void shutdown();

    /// Get the accumulated guest console output.
    const std::string& consoleOutput() const;

private:
    void handleIoPortExit(const WHV_RUN_VP_EXIT_CONTEXT& exitContext);

    WHV_PARTITION_HANDLE partition_ = nullptr;
    std::unique_ptr<GuestMemory>  memory_;
    std::unique_ptr<VirtualCpu>   vcpu_;
    std::unique_ptr<SerialConsole> console_;
    bool initialized_ = false;
    bool vmCreated_    = false;
    bool running_      = false;
};

} // namespace vmarea
