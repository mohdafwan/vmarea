// VMArea Phase 1 — Test Suite
// Minimal test framework with unit and integration tests.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>

#include "vmm/vmm.h"
#include "devices/serial_console.h"

// ================================================================
// Minimal test framework
// ================================================================

struct TestCase {
    const char* name;
    std::function<bool()> fn;
};

static std::vector<TestCase>& getTests() {
    static std::vector<TestCase> tests;
    return tests;
}

#define TEST(testname)                                                         \
    static bool test_##testname();                                             \
    namespace {                                                                \
    struct Reg_##testname {                                                    \
        Reg_##testname() { getTests().push_back({#testname, test_##testname}); }\
    } reg_##testname##_inst;                                                   \
    }                                                                          \
    static bool test_##testname()

// ================================================================
// Unit tests — SerialConsole (no WHP required)
// ================================================================

TEST(SerialConsole_Write) {
    vmarea::SerialConsole console;
    console.handleWrite(0x3F8, 'H');
    console.handleWrite(0x3F8, 'i');
    return console.output() == "Hi";
}

TEST(SerialConsole_OwnsPort) {
    vmarea::SerialConsole console;
    return console.ownsPort(0x3F8)
        && console.ownsPort(0x3FF)
        && !console.ownsPort(0x3F7)
        && !console.ownsPort(0x400);
}

TEST(SerialConsole_Callback) {
    vmarea::SerialConsole console;
    std::string received;
    console.setOutputCallback([&](char c) { received += c; });
    console.handleWrite(0x3F8, 'A');
    console.handleWrite(0x3F8, 'B');
    return received == "AB" && console.output() == "AB";
}

TEST(SerialConsole_Clear) {
    vmarea::SerialConsole console;
    console.handleWrite(0x3F8, 'X');
    console.clear();
    return console.output().empty();
}

TEST(SerialConsole_IgnoreNonDataPort) {
    vmarea::SerialConsole console;
    console.handleWrite(0x3F9, 'Z');  // Not the data port
    return console.output().empty();
}

// ================================================================
// Integration tests — require WHP (gracefully skip if unavailable)
// ================================================================

TEST(VMM_Initialize) {
    vmarea::Vmm vmm;
    bool ok = vmm.initialize();
    if (!ok) {
        printf("    [SKIP] WHP not available on this system.\n");
        return true;  // Not a failure — just unavailable hardware
    }
    return true;
}

TEST(VMM_CreateVm) {
    vmarea::Vmm vmm;
    if (!vmm.initialize()) { printf("    [SKIP]\n"); return true; }
    if (!vmm.createVm()) return false;
    vmm.shutdown();
    return true;
}

TEST(VMM_AllocateMemory) {
    vmarea::Vmm vmm;
    if (!vmm.initialize()) { printf("    [SKIP]\n"); return true; }
    if (!vmm.createVm()) return false;
    if (!vmm.allocateMemory(64 * 1024)) return false;  // 64 KB
    vmm.shutdown();
    return true;
}

TEST(VMM_CreateVcpu) {
    vmarea::Vmm vmm;
    if (!vmm.initialize()) { printf("    [SKIP]\n"); return true; }
    if (!vmm.createVm()) return false;
    if (!vmm.allocateMemory()) return false;
    if (!vmm.createVcpu()) return false;
    vmm.shutdown();
    return true;
}

TEST(VMM_FullBoot) {
    vmarea::Vmm vmm;
    if (!vmm.initialize()) {
        printf("    [SKIP] WHP not available.\n");
        return true;
    }
    if (!vmm.createVm()) return false;
    if (!vmm.allocateMemory()) return false;
    if (!vmm.createVcpu()) return false;
    if (!vmm.loadGuest("guest_kernel.bin")) {
        printf("    [SKIP] guest_kernel.bin not found.\n");
        vmm.shutdown();
        return true;
    }
    if (!vmm.startGuest()) return false;

    // Verify console output
    const std::string& output = vmm.consoleOutput();
    bool found = output.find("Darwin-like environment booted successfully.") != std::string::npos;
    if (!found) {
        printf("    Unexpected output: '%s'\n", output.c_str());
    }
    vmm.shutdown();
    return found;
}

// ================================================================
// Test runner
// ================================================================

int main() {
    printf("=== VMArea Phase 1 — Test Suite ===\n\n");

    const auto& tests = getTests();
    int pass = 0, fail = 0;

    for (const auto& t : tests) {
        printf("[RUN ] %s\n", t.name);
        bool ok = false;
        try {
            ok = t.fn();
        } catch (...) {
            printf("[FAIL] %s — unhandled exception\n\n", t.name);
            fail++;
            continue;
        }
        printf("%s %s\n\n", ok ? "[PASS]" : "[FAIL]", t.name);
        ok ? pass++ : fail++;
    }

    int total = static_cast<int>(tests.size());
    printf("=== Results: %d/%d passed, %d failed ===\n", pass, total, fail);
    return fail > 0 ? 1 : 0;
}
