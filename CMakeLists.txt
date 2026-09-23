cmake_minimum_required(VERSION 3.16)
project(vmarea VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

# ===========================================================================
# Target: Assembled Guest Kernel (if NASM is available)
# ===========================================================================
find_program(NASM_EXECUTABLE nasm)
if(NASM_EXECUTABLE)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/guest_kernel.bin"
        COMMAND "${NASM_EXECUTABLE}" -f bin -o "${CMAKE_BINARY_DIR}/guest_kernel.bin" "${CMAKE_SOURCE_DIR}/guest/kernel/kernel.asm"
        DEPENDS "${CMAKE_SOURCE_DIR}/guest/kernel/kernel.asm"
        COMMENT "Assembling guest kernel with NASM"
    )
    add_custom_target(guest_kernel ALL DEPENDS "${CMAKE_BINARY_DIR}/guest_kernel.bin")
endif()

# ===========================================================================
# Windows-specific targets: run-vm & vmarea-tests (Windows Hypervisor Platform)
# ===========================================================================
if(WIN32)
    # Main executable: run-vm
    add_executable(run-vm
        host/main.cpp
        host/vmm/vmm.cpp
        host/memory/guest_memory.cpp
        host/cpu/vcpu.cpp
        host/devices/serial_console.cpp
    )

    target_include_directories(run-vm PRIVATE
        ${CMAKE_SOURCE_DIR}/host
        ${CMAKE_SOURCE_DIR}/shared
    )

    target_link_libraries(run-vm PRIVATE WinHvPlatform)

    set_target_properties(run-vm PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    # Test executable: vmarea-tests (WHP integration suite)
    option(VMAREA_BUILD_TESTS "Build the test suite" ON)

    if(VMAREA_BUILD_TESTS)
        add_executable(vmarea-tests
            tests/test_vmm.cpp
            host/vmm/vmm.cpp
            host/memory/guest_memory.cpp
            host/cpu/vcpu.cpp
            host/devices/serial_console.cpp
        )

        target_include_directories(vmarea-tests PRIVATE
            ${CMAKE_SOURCE_DIR}/host
            ${CMAKE_SOURCE_DIR}/shared
        )

        target_link_libraries(vmarea-tests PRIVATE WinHvPlatform)

        set_target_properties(vmarea-tests PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )

        enable_testing()
        add_test(NAME VMAreaTests COMMAND vmarea-tests)
    endif()
else()
    message(STATUS "Note: Windows Hypervisor Platform (WHP) VMM targets require Windows.")
    message(STATUS "Building cross-platform device tests and guest simulation suite.")
endif()

# ===========================================================================
# Cross-platform verification test suite: vmarea-sim-tests
# ===========================================================================
add_executable(vmarea-sim-tests
    tests/test_guest_sim.cpp
    host/devices/serial_console.cpp
)

target_include_directories(vmarea-sim-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/host
    ${CMAKE_SOURCE_DIR}/shared
)

set_target_properties(vmarea-sim-tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(TARGET guest_kernel)
    add_dependencies(vmarea-sim-tests guest_kernel)
endif()

enable_testing()
add_test(NAME GuestSimulationTests COMMAND vmarea-sim-tests)
