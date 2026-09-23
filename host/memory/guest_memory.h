#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <WinHvPlatform.h>

#include <cstdint>
#include <cstddef>

namespace vmarea {

/// GuestMemory manages a contiguous region of guest physical memory.
/// Allocates page-aligned host memory and maps it into the guest's
/// physical address space via WHvMapGpaRange.
class GuestMemory {
public:
    GuestMemory();
    ~GuestMemory();

    GuestMemory(const GuestMemory&) = delete;
    GuestMemory& operator=(const GuestMemory&) = delete;

    /// Allocate host memory and map it into the guest physical address space.
    /// @param partition  WHP partition handle
    /// @param gpa        Guest physical address for the region start
    /// @param sizeBytes  Region size (rounded up to 4 KB page boundary)
    bool initialize(WHV_PARTITION_HANDLE partition, uint64_t gpa, size_t sizeBytes);

    /// Unmap and free the memory.
    void cleanup();

    /// Write data into guest memory at the given offset from base GPA.
    bool write(uint64_t offset, const void* data, size_t length) const;

    /// Read data from guest memory at the given offset from base GPA.
    bool read(uint64_t offset, void* buffer, size_t length) const;

    void*    hostPointer()          const { return hostMemory_; }
    uint64_t guestPhysicalAddress() const { return gpa_; }
    size_t   size()                 const { return size_; }
    bool     isInitialized()        const { return initialized_; }

private:
    WHV_PARTITION_HANDLE partition_ = nullptr;
    void*    hostMemory_  = nullptr;
    uint64_t gpa_         = 0;
    size_t   size_        = 0;
    bool     mapped_      = false;
    bool     initialized_ = false;
};

} // namespace vmarea
