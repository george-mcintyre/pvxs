@echo off
REM Find Python executable on Windows
REM Tries py.exe (Python launcher), python.exe, and python

where py.exe >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo py.exe
    exit /b 0
)

where python.exe >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo python.exe
    exit /b 0
)

where python >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo python
    exit /b 0
)

REM If nothing found, default to python (will fail later with a clear error)
echo python
exit /b 1

