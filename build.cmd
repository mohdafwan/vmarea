@echo off
setlocal

echo === VMArea Build Script ===
echo.

:: Check prerequisites
cmake --version >nul 2>&1
if errorlevel 1 (
    echo Error: CMake not found. Install CMake 3.16+ and add to PATH.
    exit /b 1
)

nasm --version >nul 2>&1
if errorlevel 1 (
    echo Error: NASM not found. Install NASM 2.15+ and add to PATH.
    exit /b 1
)

:: Create build directory
if not exist build mkdir build

:: Assemble guest kernel
echo [1/3] Assembling guest kernel...
nasm -f bin -o build\guest_kernel.bin guest\kernel\kernel.asm
if errorlevel 1 (
    echo Error: Guest kernel assembly failed.
    exit /b 1
)
echo   Output: build\guest_kernel.bin
echo.

:: Configure with CMake (Visual Studio generator)
echo [2/3] Configuring with CMake...
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo.
    echo CMake configuration failed. Try a different generator:
    echo   cmake -S . -B build -G "Visual Studio 16 2019" -A x64
    echo   cmake -S . -B build -G "NMake Makefiles"
    exit /b 1
)
echo.

:: Build Release configuration
echo [3/3] Building (Release)...
cmake --build build --config Release
if errorlevel 1 (
    echo Error: Build failed.
    exit /b 1
)

echo.
echo === Build complete ===
echo.
echo   Run:  run-vm.cmd
echo   Test: test.cmd
endlocal
