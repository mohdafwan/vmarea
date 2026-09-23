@echo off
setlocal

:: Locate the test executable
set "EXE="
if exist "build\bin\Release\vmarea-tests.exe" set "EXE=build\bin\Release\vmarea-tests.exe"
if exist "build\bin\Debug\vmarea-tests.exe"   set "EXE=build\bin\Debug\vmarea-tests.exe"
if exist "build\bin\vmarea-tests.exe"         set "EXE=build\bin\vmarea-tests.exe"

if "%EXE%"=="" (
    echo Error: vmarea-tests.exe not found. Run build.cmd first.
    exit /b 1
)

echo.
"%EXE%" %*
echo.

endlocal
