# VMArea Build & Installation Guide

This guide describes how to configure your Windows development environment, install all required dependencies, enable the Windows Hypervisor Platform, compile the host hypervisor and guest kernel, and run tests.

---

## 1. System Requirements & Prerequisites

To build and run VMArea, your system must satisfy the following hardware and software requirements:

| Component | Minimum Requirement | Recommended |
|---|---|---|
| **Operating System** | Windows 10 Version 1803 (Build 17134)+ | Windows 11 (64-bit) |
| **CPU Virtualization** | Intel VT-x with EPT or AMD-V with NPT | Hardware virtualization enabled in BIOS/UEFI |
| **Compiler / IDE** | Visual Studio 2019 (v16.0+) | Visual Studio 2022 Community / Professional |
| **Windows SDK** | Windows 10 SDK 10.0.17134.0+ | Windows 11 SDK 10.0.22621.0+ |
| **Build System** | CMake 3.16+ | CMake 3.25+ |
| **Assembler** | NASM 2.15+ | NASM 2.16+ (available on `PATH`) |
| **Windows Features** | Windows Hypervisor Platform | Hyper-V / Windows Hypervisor Platform enabled |

---

## 2. Step-by-Step Prerequisite Installation

### Step 2.1: Enable Hardware Virtualization in BIOS/UEFI
1. Reboot your PC and enter firmware settings (press `F2`, `F10`, `Del`, or `Esc` during boot).
2. Locate CPU Virtualization Settings:
   - For Intel CPUs: Enable **Intel Virtualization Technology (VT-x)** and **Intel VT-d** if available.
   - For AMD CPUs: Enable **SVM Mode** or **AMD-V**.
3. Save settings and boot into Windows.
4. Verify in **Task Manager** -> **Performance** -> **CPU**: ensure **Virtualization: Enabled** is shown.

### Step 2.2: Enable Windows Hypervisor Platform
WHP provides the user-mode APIs (`WinHvPlatform.dll`) required to create and manage virtual partitions.

#### Option A: Using Windows PowerShell (Administrator)
Open PowerShell as Administrator and run:
```powershell
Enable-WindowsOptionalFeature -Online -FeatureName HypervisorPlatform -All
```
*Note: A system reboot is typically required.*

#### Option B: Using Windows Features GUI
1. Press `Win + R`, type `optionalfeatures.exe`, and press **Enter**.
2. Scroll down and check **Windows Hypervisor Platform**.
3. (Optional) Check **Virtual Machine Platform** if you also use WSL2.
4. Click **OK** and restart your computer when prompted.

### Step 2.3: Install Visual Studio & Windows SDK
1. Download the [Visual Studio Installer](https://visualstudio.microsoft.com/).
2. Run the installer and select **Desktop development with C++**.
3. Ensure the following optional components are selected:
   - **MSVC v142** or **v143 - VS 2019/2022 C++ x64/x86 build tools**
   - **Windows 10/11 SDK** (version 10.0.17134.0 or higher)
   - **C++ CMake tools for Windows**

### Step 2.4: Install NASM (Netwide Assembler)
The guest kernel binary is written in x86 assembly and assembled using NASM.
1. Download the latest Windows installer or zip archive from [NASM Official Site](https://www.nasm.us/).
2. Install NASM (e.g., to `C:\Program Files\NASM` or `C:\Users\<user>\AppData\Local\bin\NASM`).
3. Add the NASM directory to your system or user `PATH` environment variable:
   ```cmd
   setx PATH "%PATH%;C:\Program Files\NASM"
   ```
4. Verify by opening a new Command Prompt:
   ```cmd
   nasm --version
   ```

### Step 2.5: Install CMake
If not already installed via Visual Studio:
1. Download the Windows x64 installer from [cmake.org](https://cmake.org/download/).
2. Check the box to add CMake to system `PATH`.
3. Verify:
   ```cmd
   cmake --version
   ```

---

## 3. Quick Build

VMArea provides automated batch scripts for quick setup and compilation.

Open an **x64 Native Tools Command Prompt for VS 2019/2022**:

```cmd
cd /d C:\path\to\vmarea
build.cmd
```

This script performs the following tasks:
1. Validates that `cmake` and `nasm` are present in `PATH`.
2. Assembles `guest/kernel/kernel.asm` into `build\guest_kernel.bin`.
3. Invokes CMake to configure the solution with the Visual Studio MSVC generator.
4. Compiles the host binaries in **Release** mode.

Artifacts generated:
- `build\guest_kernel.bin` — Raw 16-bit real-mode guest binary.
- `build\bin\Release\run-vm.exe` — Virtual machine launcher executable.
- `build\bin\Release\vmarea-tests.exe` — Windows WHP integration test runner.
- `build\bin\vmarea-sim-tests.exe` — Cross-platform serial-console and guest simulation test runner.

---

## 4. Manual Build Steps

If you prefer building manually or integrating with custom CI pipelines:

### 4.1 Assemble the Guest Kernel
```cmd
mkdir build
nasm -f bin guest/kernel/kernel.asm -o build/guest_kernel.bin
```

### 4.2 Configure the CMake Project
```cmd
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
```
*(Use `-G "Visual Studio 16 2019"` if using VS 2019).*

### 4.3 Build Release Configuration
```cmd
cmake --build build --config Release
```

### 4.4 Build Debug Configuration
For debugging symbols and internal diagnostics:
```cmd
cmake --build build --config Debug
```
Debug binaries will be placed in `build\bin\Debug\`.

---

## 5. Running the Virtual Machine

### Quick Run
```cmd
run-vm.cmd
```

### Manual Run
Run the executable directly, passing the path to the guest binary:

**Release Mode:**
```cmd
build\bin\Release\run-vm.exe build\guest_kernel.bin
```

**Debug Mode:**
```cmd
build\bin\Debug\run-vm.exe build\guest_kernel.bin
```

---

## 6. Running Tests

Run the test suite via the test batch script:
```cmd
test.cmd
```

Or execute the test binary manually:
```cmd
build\bin\Release\vmarea-tests.exe build\guest_kernel.bin
```

The test runner exercises:
- Serial console buffer handling and I/O port address decoding.
- Guest-memory allocation and GPA mapping through the native WHP lifecycle tests.
- A complete WHP partition lifecycle and native guest boot when WHP is available. WHP-dependent tests report **SKIP** (rather than pass) when the Windows host or hypervisor is unavailable.

---

## 7. Troubleshooting

### Problem: `WHvGetCapability failed: 0x80070032` (or "Hypervisor not present")
- **Cause**: Windows Hypervisor Platform feature is not enabled, or hardware virtualization is disabled in BIOS.
- **Solution**:
  1. Check BIOS/UEFI settings and enable Intel VT-x / AMD-V.
  2. Run `Get-WindowsOptionalFeature -Online -FeatureName HypervisorPlatform` in PowerShell to confirm state is `Enabled`.
  3. Ensure Hyper-V or Windows Hypervisor Platform is not blocked by third-party antivirus or legacy virtualization software.

### Problem: `'nasm' is not recognized as an internal or external command`
- **Cause**: NASM is not installed or not in the current session's `PATH`.
- **Solution**:
  1. Verify NASM installation directory.
  2. Add the directory containing `nasm.exe` to `PATH`.
  3. Re-open the Command Prompt.

### Problem: `LINK : fatal error LNK1104: cannot open file 'WinHvPlatform.lib'`
- **Cause**: The Windows SDK version installed does not include WHP headers and libraries.
- **Solution**:
  1. Open Visual Studio Installer.
  2. Modify your installation and ensure **Windows 10/11 SDK (10.0.17134.0 or higher)** is installed.
  3. Check that the SDK `Lib` directory contains `WinHvPlatform.Lib` (typically located in `C:\Program Files (x86)\Windows Kits\10\Lib\<sdk_version>\um\x64`).

### Problem: Access Denied when running `run-vm.cmd`
- **Cause**: Creating WHP partitions requires appropriate user privileges on some Windows configurations.
- **Solution**: Run your Command Prompt or terminal as **Administrator**.
