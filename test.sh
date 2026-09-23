#!/usr/bin/env bash
set -e

# Locate test executable
EXE=""
if [ -f "build/bin/vmarea-sim-tests" ]; then
    EXE="build/bin/vmarea-sim-tests"
elif [ -f "build/bin/Release/vmarea-tests.exe" ]; then
    EXE="build/bin/Release/vmarea-tests.exe"
fi

if [ -z "$EXE" ]; then
    echo "Error: Test executable not found. Run cmake --build build first."
    exit 1
fi

echo ""
"$EXE" "$@"
echo ""
