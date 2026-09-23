#include "vcpu.h"
#include <cstdio>

namespace vmarea {

VirtualCpu::VirtualCpu() = default;

VirtualCpu::~VirtualCpu() {
    destroy();
}

bool VirtualCpu::create(WHV_PARTITION_HANDLE partition, uint32_t vpIndex) {
    if (created_) {
        fprintf(stderr, "VirtualCpu: Already created.\n");
        return false;
    }

    HRESULT hr = WHvCreateVirtualProcessor(partition, vpIndex, 0);
    if (FAILED(hr)) {
        fprintf(stderr, "VirtualCpu: WHvCreateVirtualProcessor failed (0x%08lX).\n", hr);
        return false;
    }

    partition_ = partition;
    vpIndex_   = vpIndex;
    created_   = true;
    return true;
}

void VirtualCpu::destroy() {
    if (created_ && partition_) {
        WHvDeleteVirtualProcessor(partition_, vpIndex_);
        created_  = false;
        partition_ = nullptr;
    }
}

bool VirtualCpu::initializeRealMode(uint64_t entryPoint, uint64_t stackPointer) {
    if (!created_) {
        fprintf(stderr, "VirtualCpu: Not created.\n");
        return false;
    }

    // Named indices for clarity
    enum RegIdx {
        R_RIP = 0, R_RSP, R_RFLAGS,
        R_CS, R_DS, R_ES, R_FS, R_GS, R_SS,
        R_CR0, R_IDTR, R_GDTR,
        R_COUNT
    };

    WHV_REGISTER_NAME names[R_COUNT] = {
        WHvX64RegisterRip,       // 0
        WHvX64RegisterRsp,       // 1
        WHvX64RegisterRflags,    // 2
        WHvX64RegisterCs,        // 3
        WHvX64RegisterDs,        // 4
        WHvX64RegisterEs,        // 5
        WHvX64RegisterFs,        // 6
        WHvX64RegisterGs,        // 7
        WHvX64RegisterSs,        // 8
        WHvX64RegisterCr0,       // 9
        WHvX64RegisterIdtr,      // 10
        WHvX64RegisterGdtr,      // 11
    };

    WHV_REGISTER_VALUE values[R_COUNT] = {};

    // RIP — guest entry point
    values[R_RIP].Reg64 = entryPoint;

    // RSP — stack pointer
    values[R_RSP].Reg64 = stackPointer;

    // RFLAGS — bit 1 is reserved and must always be 1
    values[R_RFLAGS].Reg64 = 0x2;

    // CS — real-mode code segment
    //   Type=0xB (execute/read/accessed), S=1 (non-system), DPL=0, P=1
    values[R_CS].Segment.Base       = 0;
    values[R_CS].Segment.Limit      = 0xFFFF;
    values[R_CS].Segment.Selector   = 0;
    values[R_CS].Segment.Attributes = 0x009B;

    // DS, ES, FS, GS, SS — real-mode data segments
    //   Type=0x3 (read/write/accessed), S=1, DPL=0, P=1
    for (int i = R_DS; i <= R_SS; i++) {
        values[i].Segment.Base       = 0;
        values[i].Segment.Limit      = 0xFFFF;
        values[i].Segment.Selector   = 0;
        values[i].Segment.Attributes = 0x0093;
    }

    // CR0 — PE=0 (real mode), ET=1 (bit 4, extension type, always 1)
    values[R_CR0].Reg64 = 0x10;

    // IDTR — real-mode default: base=0, limit=0x3FF (256 IVT entries x 4 bytes)
    values[R_IDTR].Table.Base  = 0;
    values[R_IDTR].Table.Limit = 0x3FF;

    // GDTR — unused in real mode, safe defaults
    values[R_GDTR].Table.Base  = 0;
    values[R_GDTR].Table.Limit = 0;

    HRESULT hr = WHvSetVirtualProcessorRegisters(
        partition_, vpIndex_, names, R_COUNT, values
    );
    if (FAILED(hr)) {
        fprintf(stderr, "VirtualCpu: WHvSetVirtualProcessorRegisters failed (0x%08lX).\n", hr);
        return false;
    }

    return true;
}

bool VirtualCpu::run(WHV_RUN_VP_EXIT_CONTEXT* exitContext) {
    if (!created_) return false;

    HRESULT hr = WHvRunVirtualProcessor(
        partition_, vpIndex_, exitContext, sizeof(WHV_RUN_VP_EXIT_CONTEXT)
    );
    if (FAILED(hr)) {
        fprintf(stderr, "VirtualCpu: WHvRunVirtualProcessor failed (0x%08lX).\n", hr);
        return false;
    }
    return true;
}

bool VirtualCpu::advanceInstructionPointer(const WHV_VP_EXIT_CONTEXT& vpContext) {
    if (!created_) return false;

    WHV_REGISTER_NAME  name  = WHvX64RegisterRip;
    WHV_REGISTER_VALUE value = {};
    value.Reg64 = vpContext.Rip + vpContext.InstructionLength;

    HRESULT hr = WHvSetVirtualProcessorRegisters(
        partition_, vpIndex_, &name, 1, &value
    );
    if (FAILED(hr)) {
        fprintf(stderr, "VirtualCpu: Failed to advance RIP (0x%08lX).\n", hr);
        return false;
    }
    return true;
}

} // namespace vmarea
