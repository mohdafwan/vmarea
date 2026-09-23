#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <WinHvPlatform.h>

#include <cstdint>

namespace vmarea {

/// VirtualCpu manages a single virtual processor in a WHP partition.
class VirtualCpu {
public:
    VirtualCpu();
    ~VirtualCpu();

    VirtualCpu(const VirtualCpu&) = delete;
    VirtualCpu& operator=(const VirtualCpu&) = delete;

    /// Create the virtual processor in the given partition.
    bool create(WHV_PARTITION_HANDLE partition, uint32_t vpIndex);

    /// Destroy the virtual processor.
    void destroy();

    /// Initialize the vCPU for 16-bit real-mode execution.
    /// @param entryPoint    RIP value where guest execution begins
    /// @param stackPointer  Initial RSP value
    bool initializeRealMode(uint64_t entryPoint, uint64_t stackPointer);

    /// Run the vCPU until a VM exit occurs.
    bool run(WHV_RUN_VP_EXIT_CONTEXT* exitContext);

    /// Advance the instruction pointer past the current instruction.
    bool advanceInstructionPointer(const WHV_VP_EXIT_CONTEXT& vpContext);

    bool     isCreated() const { return created_; }
    uint32_t index()     const { return vpIndex_; }

private:
    WHV_PARTITION_HANDLE partition_ = nullptr;
    uint32_t vpIndex_ = 0;
    bool     created_ = false;
};

} // namespace vmarea
