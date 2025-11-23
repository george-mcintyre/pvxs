#!/bin/sh
# Find Python executable on Windows (for Unix-like shells like Git Bash)

# Try py.exe first (Windows Python launcher)
if command -v py.exe >/dev/null 2>&1; then
    echo "py.exe"
    exit 0
fi

# Try python.exe
if command -v python.exe >/dev/null 2>&1; then
    echo "python.exe"
    exit 0
fi

# Try python
if command -v python >/dev/null 2>&1; then
    echo "python"
    exit 0
fi

# Try python3
if command -v python3 >/dev/null 2>&1; then
    echo "python3"
    exit 0
fi

# If nothing found, default to python (will fail later with a clear error)
echo "python"
exit 1

