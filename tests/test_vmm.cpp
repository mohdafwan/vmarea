// VMArea Phase 1 -- WHP integration tests.
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "devices/serial_console.h"
#include "vmm/vmm.h"

enum class TestResult { Pass, Fail, Skip };

struct TestCase {
    const char* name;
    std::function<TestResult()> run;
};

static bool whpAvailable() {
    vmarea::Vmm vmm;
    return vmm.initialize();
}

static TestResult requireWhp() {
    if (!whpAvailable()) {
        printf("    WHP is unavailable on this host.\n");
        return TestResult::Skip;
    }
    return TestResult::Pass;
}

static TestResult testSerialConsole() {
    vmarea::SerialConsole console;
    std::string callbackOutput;
    console.setOutputCallback([&callbackOutput](char value) { callbackOutput += value; });
    console.handleWrite(0x3F8, 'H');
    console.handleWrite(0x3F8, 'i');
    console.handleWrite(0x3F9, '!');
    return console.output() == "Hi" && callbackOutput == "Hi" &&
           console.ownsPort(0x3F8) && console.ownsPort(0x3FF) &&
           !console.ownsPort(0x3F7) && !console.ownsPort(0x400)
        ? TestResult::Pass : TestResult::Fail;
}

static TestResult testPartitionLifecycle() {
    if (requireWhp() == TestResult::Skip) return TestResult::Skip;
    vmarea::Vmm vmm;
    return vmm.initialize() && vmm.createVm() && vmm.allocateMemory(64 * 1024) &&
           vmm.createVcpu() ? TestResult::Pass : TestResult::Fail;
}

static TestResult testFullBoot(const std::string& guestImage) {
    if (requireWhp() == TestResult::Skip) return TestResult::Skip;
    vmarea::Vmm vmm;
    if (!vmm.initialize() || !vmm.createVm() || !vmm.allocateMemory() ||
        !vmm.createVcpu() || !vmm.loadGuest(guestImage) || !vmm.startGuest()) {
        return TestResult::Fail;
    }
    return vmm.consoleOutput() == "Darwin-like environment booted successfully.\r\n"
        ? TestResult::Pass : TestResult::Fail;
}

int main(int argc, char* argv[]) {
    const std::string guestImage = argc > 1 ? argv[1] : "guest_kernel.bin";
    const std::vector<TestCase> tests = {
        {"SerialConsole", testSerialConsole},
        {"PartitionLifecycle", testPartitionLifecycle},
        {"FullBootAndCleanHalt", [&guestImage] { return testFullBoot(guestImage); }},
    };

    int passed = 0;
    int failed = 0;
    int skipped = 0;
    for (const TestCase& test : tests) {
        printf("[RUN ] %s\n", test.name);
        const TestResult result = test.run();
        if (result == TestResult::Pass) {
            ++passed;
            printf("[PASS] %s\n\n", test.name);
        } else if (result == TestResult::Skip) {
            ++skipped;
            printf("[SKIP] %s\n\n", test.name);
        } else {
            ++failed;
            printf("[FAIL] %s\n\n", test.name);
        }
    }
    printf("=== Results: %d passed, %d failed, %d skipped ===\n", passed, failed, skipped);
    return failed == 0 ? 0 : 1;
}
