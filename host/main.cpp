// VMArea Phase 1 — Main Entry Point
// Minimal Darwin-like virtualization environment on Windows

#include <cstdio>
#include <cstdlib>
#include <string>

#include "vmm/vmm.h"

int main(int argc, char* argv[]) {
    std::string guestImage = "guest_kernel.bin";
    if (argc > 1) {
        guestImage = argv[1];
    }

    printf("=== VMArea Phase 1 — Darwin-like Virtualization Environment ===\n\n");

    vmarea::Vmm vmm;

    printf("Initializing VMM...\n");
    if (!vmm.initialize()) return 1;

    printf("Creating VM...\n");
    if (!vmm.createVm()) return 1;

    printf("Allocating guest memory...\n");
    if (!vmm.allocateMemory()) return 1;

    printf("Creating virtual CPU...\n");
    if (!vmm.createVcpu()) return 1;

    printf("Loading guest kernel...\n");
    if (!vmm.loadGuest(guestImage)) return 1;

    printf("Starting guest...\n");
    if (!vmm.startGuest()) return 1;

    vmm.shutdown();
    printf("VM stopped successfully.\n");

    return 0;
}
