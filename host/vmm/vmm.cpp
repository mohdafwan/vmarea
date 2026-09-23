#include "vmm.h"
#include "../memory/guest_memory.h"
#include "../cpu/vcpu.h"
#include "../devices/serial_console.h"

#include <cstdio>
#include <fstream>
#include <vector>

namespace vmarea {

Vmm::Vmm() = default;

Vmm::~Vmm() {
    shutdown();
}

bool Vmm::initialize() {
    if (initialized_) {
        return true;
    }

    WHV_CAPABILITY capability = {};
    UINT32 written = 0;

    HRESULT hr = WHvGetCapability(
        WHvCapabilityCodeHypervisorPresent,
        &capability, sizeof(capability), &written
    );

    if (FAILED(hr)) {
        fprintf(stderr, "Error: WHvGetCapability failed (0x%08lX).\n", hr);
        fprintf(stderr, "  Ensure Windows Hypervisor Platform is enabled:\n");
        fprintf(stderr, "  Settings > Apps > Optional Features > More Windows Features\n");
        fprintf(stderr, "  > Windows Hypervisor Platform\n");
        return false;
    }

    if (!capability.HypervisorPresent) {
        fprintf(stderr, "Error: Hypervisor not present.\n");
        fprintf(stderr, "  Enable VT-x/AMD-V in BIOS/UEFI settings.\n");
        return false;
    }

    printf("  WHP hypervisor detected.\n");
    initialized_ = true;
    return true;
}

bool Vmm::createVm() {
    if (!initialized_) {
        fprintf(stderr, "Error: VMM not initialized.\n");
        return false;
    }
    if (vmCreated_ || partition_) {
        fprintf(stderr, "Error: VM partition already created.\n");
        return false;
    }

    HRESULT hr = WHvCreatePartition(&partition_);
    if (FAILED(hr)) {
        fprintf(stderr, "Error: WHvCreatePartition failed (0x%08lX).\n", hr);
        return false;
    }

    // Configure: 1 virtual processor
    WHV_PARTITION_PROPERTY prop = {};
    prop.ProcessorCount = 1;
    hr = WHvSetPartitionProperty(
        partition_, WHvPartitionPropertyCodeProcessorCount,
        &prop, sizeof(prop)
    );
    if (FAILED(hr)) {
        fprintf(stderr, "Error: Failed to set processor count (0x%08lX).\n", hr);
        WHvDeletePartition(partition_);
        partition_ = nullptr;
        return false;
    }

    // Finalize partition configuration
    hr = WHvSetupPartition(partition_);
    if (FAILED(hr)) {
        fprintf(stderr, "Error: WHvSetupPartition failed (0x%08lX).\n", hr);
        WHvDeletePartition(partition_);
        partition_ = nullptr;
        return false;
    }

    vmCreated_ = true;
    printf("  VM partition created.\n");
    return true;
}

bool Vmm::allocateMemory(size_t sizeBytes) {
    if (!vmCreated_) {
        fprintf(stderr, "Error: VM not created.\n");
        return false;
    }
    if (vcpu_) {
        fprintf(stderr, "Error: Guest memory cannot be replaced after vCPU creation.\n");
        return false;
    }

    memory_ = std::make_unique<GuestMemory>();
    if (!memory_->initialize(partition_, 0, sizeBytes)) {
        memory_.reset();
        return false;
    }

    printf("  Allocated %zu bytes of guest memory at GPA 0x0.\n", memory_->size());
    return true;
}

bool Vmm::createVcpu() {
    if (!vmCreated_) {
        fprintf(stderr, "Error: VM not created.\n");
        return false;
    }
    if (!memory_) {
        fprintf(stderr, "Error: Guest memory must be allocated before creating a vCPU.\n");
        return false;
    }
    if (vcpu_) {
        fprintf(stderr, "Error: Virtual CPU #0 already created.\n");
        return false;
    }

    vcpu_ = std::make_unique<VirtualCpu>();
    if (!vcpu_->create(partition_, 0)) {
        vcpu_.reset();
        return false;
    }

    printf("  Virtual CPU #0 created.\n");
    return true;
}

bool Vmm::loadGuest(const std::string& imagePath) {
    if (!memory_ || !vcpu_) {
        fprintf(stderr, "Error: Memory or vCPU not ready.\n");
        return false;
    }

    // Read the guest binary from disk
    std::ifstream file(imagePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        fprintf(stderr, "Error: Cannot open guest image: %s\n", imagePath.c_str());
        return false;
    }

    const std::streampos end = file.tellg();
    if (end <= 0) {
        fprintf(stderr, "Error: Guest image is empty or its size could not be determined.\n");
        return false;
    }
    const auto fileSize = static_cast<size_t>(end);
    file.seekg(0, std::ios::beg);

    if (fileSize > memory_->size()) {
        fprintf(stderr, "Error: Guest image (%zu bytes) exceeds guest memory (%zu bytes).\n",
                fileSize, memory_->size());
        return false;
    }

    std::vector<uint8_t> imageData(fileSize);
    file.read(reinterpret_cast<char*>(imageData.data()), fileSize);
    if (!file) {
        fprintf(stderr, "Error: Failed to read guest image.\n");
        return false;
    }
    file.close();

    // Load guest binary at GPA 0x0000
    if (!memory_->write(0, imageData.data(), fileSize)) {
        fprintf(stderr, "Error: Failed to write guest image to memory.\n");
        return false;
    }

    printf("  Loaded %zu bytes from '%s'.\n", fileSize, imagePath.c_str());

    // Initialize vCPU for real-mode execution
    //   Entry point: 0x0000 (start of guest binary)
    //   Stack:       0x7C00 (conventional location)
    if (!vcpu_->initializeRealMode(0x0000, 0x7C00)) {
        fprintf(stderr, "Error: Failed to initialize vCPU state.\n");
        return false;
    }

    // Create the serial console device
    console_ = std::make_unique<SerialConsole>();
    return true;
}

bool Vmm::startGuest() {
    if (!vcpu_ || !memory_ || !console_) {
        fprintf(stderr, "Error: VM not fully initialized.\n");
        return false;
    }

    running_ = true;
    bool prefixPrinted = false;
    uint32_t exitCount = 0;
    constexpr uint32_t MAX_EXITS = 100000;

    // WHP reports an I/O-port exit after the I/O instruction has executed.
    // VpContext.Rip already identifies the next instruction, so the VMM must
    // not advance it again.
    console_->setOutputCallback([&prefixPrinted](char c) {
        if (!prefixPrinted) {
            printf("Guest: ");
            prefixPrinted = true;
        }
        putchar(c);
        fflush(stdout);
    });

    while (running_) {
        WHV_RUN_VP_EXIT_CONTEXT exitCtx = {};

        if (!vcpu_->run(&exitCtx)) {
            running_ = false;
            return false;
        }

        if (++exitCount > MAX_EXITS) {
            fprintf(stderr, "Error: Exceeded %u VM exits — possible infinite loop.\n", MAX_EXITS);
            running_ = false;
            return false;
        }

        switch (exitCtx.ExitReason) {
        case WHvRunVpExitReasonX64IoPortAccess:
            if (!handleIoPortExit(exitCtx)) {
                running_ = false;
                return false;
            }
            break;

        case WHvRunVpExitReasonX64Halt:
            printf("Guest halted.\n");
            running_ = false;
            break;

        case WHvRunVpExitReasonMemoryAccess:
            fprintf(stderr, "Error: Memory fault at GPA 0x%llX (RIP=0x%llX).\n",
                    (unsigned long long)exitCtx.MemoryAccess.Gpa,
                    (unsigned long long)exitCtx.VpContext.Rip);
            running_ = false;
            return false;

        case WHvRunVpExitReasonCanceled:
            fprintf(stderr, "Error: Guest execution was canceled.\n");
            running_ = false;
            return false;

        default:
            fprintf(stderr, "Error: Unhandled VM exit %d (RIP=0x%llX).\n",
                    exitCtx.ExitReason,
                    (unsigned long long)exitCtx.VpContext.Rip);
            running_ = false;
            return false;
        }
    }

    return true;
}

bool Vmm::handleIoPortExit(const WHV_RUN_VP_EXIT_CONTEXT& exitCtx) {
    const auto& io = exitCtx.IoPortAccess;
    if (!io.AccessInfo.IsWrite || !console_->ownsPort(io.PortNumber)) {
        fprintf(stderr, "Error: Unsupported I/O %s at port 0x%X.\n",
                io.AccessInfo.IsWrite ? "write" : "read", io.PortNumber);
        return false;
    }
    if (io.AccessInfo.AccessSize != 1) {
        fprintf(stderr, "Error: Unsupported I/O write size %u at port 0x%X.\n",
                io.AccessInfo.AccessSize, io.PortNumber);
        return false;
    }
    console_->handleWrite(io.PortNumber, static_cast<uint8_t>(io.Rax));
    return true;
}

void Vmm::shutdown() {
    running_ = false;

    // Destroy in reverse order of creation
    vcpu_.reset();
    memory_.reset();
    console_.reset();

    if (partition_) {
        WHvDeletePartition(partition_);
        partition_ = nullptr;
    }

    vmCreated_   = false;
    initialized_ = false;
}

const std::string& Vmm::consoleOutput() const {
    static const std::string empty;
    return console_ ? console_->output() : empty;
}

} // namespace vmarea
