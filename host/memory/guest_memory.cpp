#include "guest_memory.h"
#include <cstdio>
#include <cstring>
#include <limits>

namespace vmarea {

GuestMemory::GuestMemory() = default;

GuestMemory::~GuestMemory() {
    cleanup();
}

bool GuestMemory::initialize(WHV_PARTITION_HANDLE partition, uint64_t gpa, size_t sizeBytes) {
    if (initialized_) {
        fprintf(stderr, "GuestMemory: Already initialized.\n");
        return false;
    }
    if (!partition || sizeBytes == 0) {
        fprintf(stderr, "GuestMemory: Invalid parameters.\n");
        return false;
    }

    // Round up to 4 KB page boundary
    constexpr size_t PAGE_SIZE = 4096;
    if ((gpa % PAGE_SIZE) != 0 || sizeBytes > std::numeric_limits<size_t>::max() - (PAGE_SIZE - 1)) {
        fprintf(stderr, "GuestMemory: GPA must be page aligned and size must be representable.\n");
        return false;
    }
    size_ = (sizeBytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (gpa > std::numeric_limits<uint64_t>::max() - size_) {
        fprintf(stderr, "GuestMemory: GPA range overflows.\n");
        size_ = 0;
        return false;
    }

    partition_ = partition;
    gpa_ = gpa;

    // Allocate page-aligned host memory
    hostMemory_ = VirtualAlloc(nullptr, size_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!hostMemory_) {
        fprintf(stderr, "GuestMemory: VirtualAlloc failed (error %lu).\n", GetLastError());
        return false;
    }

    // Zero-initialize
    memset(hostMemory_, 0, size_);

    // Map host memory into guest physical address space
    HRESULT hr = WHvMapGpaRange(
        partition_, hostMemory_, gpa_, size_,
        WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute
    );
    if (FAILED(hr)) {
        fprintf(stderr, "GuestMemory: WHvMapGpaRange failed (0x%08lX).\n", hr);
        VirtualFree(hostMemory_, 0, MEM_RELEASE);
        hostMemory_ = nullptr;
        partition_ = nullptr;
        gpa_ = 0;
        size_ = 0;
        return false;
    }

    mapped_ = true;
    initialized_ = true;
    return true;
}

void GuestMemory::cleanup() {
    if (mapped_ && partition_) {
        WHvUnmapGpaRange(partition_, gpa_, size_);
        mapped_ = false;
    }
    if (hostMemory_) {
        VirtualFree(hostMemory_, 0, MEM_RELEASE);
        hostMemory_ = nullptr;
    }
    initialized_ = false;
    partition_ = nullptr;
    gpa_ = 0;
    size_ = 0;
}

bool GuestMemory::write(uint64_t offset, const void* data, size_t length) const {
    if (!initialized_ || (!data && length != 0) || offset > size_ || length > size_ - offset) {
        fprintf(stderr, "GuestMemory: Write out of bounds (offset=0x%llX, len=%zu, size=%zu).\n",
                (unsigned long long)offset, length, size_);
        return false;
    }
    memcpy(static_cast<uint8_t*>(hostMemory_) + offset, data, length);
    return true;
}

bool GuestMemory::read(uint64_t offset, void* buffer, size_t length) const {
    if (!initialized_ || (!buffer && length != 0) || offset > size_ || length > size_ - offset) {
        fprintf(stderr, "GuestMemory: Read out of bounds (offset=0x%llX, len=%zu, size=%zu).\n",
                (unsigned long long)offset, length, size_);
        return false;
    }
    memcpy(buffer, static_cast<const uint8_t*>(hostMemory_) + offset, length);
    return true;
}

} // namespace vmarea
