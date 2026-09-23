#pragma once

// VMArea Phase 1 — Common type definitions

#include <cstdint>
#include <cstddef>

namespace vmarea {

/// Guest physical address type
using GPA = uint64_t;

/// Default memory configuration
constexpr size_t DEFAULT_GUEST_MEMORY_SIZE = 1 * 1024 * 1024; // 1 MB
constexpr GPA GUEST_LOAD_ADDRESS = 0x0000;
constexpr uint16_t DEFAULT_STACK_POINTER = 0x7C00;

/// Serial port
constexpr uint16_t COM1_DATA_PORT = 0x3F8;
constexpr uint16_t COM1_PORT_COUNT = 8;

} // namespace vmarea
