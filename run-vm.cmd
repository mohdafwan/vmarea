@echo off
setlocal

:: Locate the executable
set "EXE="
if exist "build\bin\Release\run-vm.exe" set "EXE=build\bin\Release\run-vm.exe"
if exist "build\bin\Debug\run-vm.exe"   set "EXE=build\bin\Debug\run-vm.exe"
if exist "build\bin\run-vm.exe"         set "EXE=build\bin\run-vm.exe"

if "%EXE%"=="" (
    echo Error: run-vm.exe not found. Run build.cmd first.
    exit /b 1
)

:: Locate the guest kernel
set "GUEST=build\guest_kernel.bin"
if not exist "%GUEST%" (
    echo Error: guest_kernel.bin not found. Run build.cmd first.
    exit /b 1
)

echo.
"%EXE%" "%GUEST%" %*
echo.

endlocal
