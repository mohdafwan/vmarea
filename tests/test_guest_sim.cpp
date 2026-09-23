#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include "devices/serial_console.h"

// ============================================================================
// VMArea Phase 1 — Device Unit Tests & Guest Execution Simulation
// ============================================================================

bool test_SerialConsole_Write() {
    vmarea::SerialConsole console;
    console.handleWrite(0x3F8, 'H');
    console.handleWrite(0x3F8, 'i');
    return console.output() == "Hi";
}

bool test_SerialConsole_OwnsPort() {
    vmarea::SerialConsole console;
    return console.ownsPort(0x3F8)
        && console.ownsPort(0x3FF)
        && !console.ownsPort(0x3F7)
        && !console.ownsPort(0x400);
}

bool test_SerialConsole_Callback() {
    vmarea::SerialConsole console;
    std::string received;
    console.setOutputCallback([&](char c) { received += c; });
    console.handleWrite(0x3F8, 'A');
    console.handleWrite(0x3F8, 'B');
    return received == "AB" && console.output() == "AB";
}

bool test_SerialConsole_Clear() {
    vmarea::SerialConsole console;
    console.handleWrite(0x3F8, 'X');
    console.clear();
    return console.output().empty();
}

bool test_SerialConsole_IgnoreNonDataPort() {
    vmarea::SerialConsole console;
    console.handleWrite(0x3F9, 'Z');
    return console.output().empty();
}

bool test_GuestKernel_Execution() {
    // Try multiple possible paths for the assembled binary
    const std::vector<std::string> paths = {
        "build/guest_kernel.bin",
        "guest_kernel.bin",
        "../build/guest_kernel.bin"
    };

    std::ifstream file;
    std::string resolvedPath;
    for (const auto& p : paths) {
        file.open(p, std::ios::binary);
        if (file.is_open()) {
            resolvedPath = p;
            break;
        }
    }

    if (!file.is_open()) {
        printf("    [FAIL] Cannot locate guest_kernel.bin in build directories.\n");
        return false;
    }

    std::vector<uint8_t> memory(1024 * 1024, 0); // 1 MB guest physical memory
    file.read(reinterpret_cast<char*>(memory.data()), 512);
    size_t bytesRead = file.gcount();
    file.close();

    if (bytesRead < 40) {
        printf("    [FAIL] Binary too small: %zu bytes\n", bytesRead);
        return false;
    }

    // Initialize vCPU state matching VirtualCpu::initializeRealMode
    uint16_t ax = 0, dx = 0, si = 0;
    uint16_t ds = 0;
    uint16_t ip = 0;
    bool zf = false;
    bool halted = false;

    vmarea::SerialConsole console;
    console.setOutputCallback([](char c) {
        putchar(c);
        fflush(stdout);
    });

    uint32_t stepCount = 0;
    constexpr uint32_t MAX_STEPS = 10000;

    printf("\n    Simulating vCPU execution of guest_kernel.bin:\n    Guest: ");

    while (!halted && stepCount++ < MAX_STEPS) {
        uint8_t op = memory[ip];
        if (op == 0x31 && memory[ip + 1] == 0xC0) {
            // xor ax, ax
            ax = 0;
            ip += 2;
        } else if (op == 0x8E && memory[ip + 1] == 0xD8) {
            // mov ds, ax
            ds = ax;
            ip += 2;
        } else if (op == 0x8E && memory[ip + 1] == 0xC0) {
            // mov es, ax
            ip += 2;
        } else if (op == 0x8E && memory[ip + 1] == 0xD0) {
            // mov ss, ax
            ip += 2;
        } else if (op == 0xBC) {
            // mov sp, imm16
            ip += 3;
        } else if (op == 0xBE) {
            // mov si, imm16
            si = memory[ip + 1] | (memory[ip + 2] << 8);
            ip += 3;
        } else if (op == 0xAC) {
            // lodsb: AL = [DS:SI], SI++
            uint32_t addr = (ds * 16) + si;
            ax = (ax & 0xFF00) | memory[addr];
            si++;
            ip += 1;
        } else if (op == 0x84 && memory[ip + 1] == 0xC0) {
            // test al, al
            zf = ((ax & 0xFF) == 0);
            ip += 2;
        } else if (op == 0x74) {
            // jz rel8
            int8_t rel = static_cast<int8_t>(memory[ip + 1]);
            ip += 2;
            if (zf) {
                ip += rel;
            }
        } else if (op == 0xBA) {
            // mov dx, imm16
            dx = memory[ip + 1] | (memory[ip + 2] << 8);
            ip += 3;
        } else if (op == 0xEE) {
            // out dx, al (I/O port VM exit to SerialConsole)
            console.handleWrite(dx, static_cast<uint8_t>(ax & 0xFF));
            ip += 1;
        } else if (op == 0xEB) {
            // jmp rel8
            int8_t rel = static_cast<int8_t>(memory[ip + 1]);
            ip += 2 + rel;
        } else if (op == 0xF4) {
            // hlt
            halted = true;
            ip += 1;
        } else {
            printf("\n    [FAIL] Unknown opcode: 0x%02X at IP=0x%04X\n", op, ip);
            return false;
        }
    }

    if (!halted) {
        printf("\n    [FAIL] Guest did not halt within %u steps\n", MAX_STEPS);
        return false;
    }

    printf("    Guest halted successfully at IP=0x%04X\n", ip);

    const std::string& out = console.output();
    const std::string expected = "Darwin-like environment booted successfully.\r\n";
    if (out != expected) {
        printf("    [FAIL] Output mismatch!\n      Expected: '%s'\n      Actual:   '%s'\n",
               expected.c_str(), out.c_str());
        return false;
    }

    return true;
}

int main() {
    printf("=================================================================\n");
    printf("         VMArea Phase 1 — Comprehensive Verification Suite       \n");
    printf("=================================================================\n\n");

    struct TestEntry {
        const char* name;
        bool (*fn)();
    } tests[] = {
        {"SerialConsole_Write", test_SerialConsole_Write},
        {"SerialConsole_OwnsPort", test_SerialConsole_OwnsPort},
        {"SerialConsole_Callback", test_SerialConsole_Callback},
        {"SerialConsole_Clear", test_SerialConsole_Clear},
        {"SerialConsole_IgnoreNonDataPort", test_SerialConsole_IgnoreNonDataPort},
        {"GuestKernel_ExecutionAndOutput", test_GuestKernel_Execution},
    };

    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    for (const auto& t : tests) {
        printf("[RUN ] %s...", t.name);
        bool ok = t.fn();
        if (ok) {
            printf(" [PASS]\n");
            passed++;
        } else {
            printf(" [FAIL]\n");
        }
    }

    printf("\n=================================================================\n");
    printf("Results: %d/%d tests passed (%d failed)\n", passed, total, total - passed);
    printf("=================================================================\n");

    return (passed == total) ? 0 : 1;
}
