@echo off
REM Ensure Python is available, download and install if needed
REM Outputs Python executable path to stdout, errors to stderr

setlocal enabledelayedexpansion

REM First check common installation locations (more reliable than PATH)
REM Check common Python installation locations
REM Output paths with forward slashes for Make compatibility (no quotes - Make will handle quoting)
if exist "C:\Program Files\Python311\python.exe" (
    "C:\Program Files\Python311\python.exe" --version >nul 2>&1
    if !ERRORLEVEL! == 0 (
        echo C:/Program Files/Python311/python.exe
        exit /b 0
    )
)
if exist "C:\Program Files\Python310\python.exe" (
    "C:\Program Files\Python310\python.exe" --version >nul 2>&1
    if !ERRORLEVEL! == 0 (
        echo C:/Program Files/Python310/python.exe
        exit /b 0
    )
)
if exist "C:\Program Files\Python39\python.exe" (
    "C:\Program Files\Python39\python.exe" --version >nul 2>&1
    if !ERRORLEVEL! == 0 (
        echo C:/Program Files/Python39/python.exe
        exit /b 0
    )
)
if exist "C:\Program Files (x86)\Python311\python.exe" (
    "C:\Program Files (x86)\Python311\python.exe" --version >nul 2>&1
    if !ERRORLEVEL! == 0 (
        echo C:/Program Files (x86)/Python311/python.exe
        exit /b 0
    )
)
if exist "%LOCALAPPDATA%\Programs\Python\Python311\python.exe" (
    "%LOCALAPPDATA%\Programs\Python\Python311\python.exe" --version >nul 2>&1
    if !ERRORLEVEL! == 0 (
        REM Convert backslashes to forward slashes
        set PYTHON_PATH=%LOCALAPPDATA%\Programs\Python\Python311\python.exe
        set PYTHON_PATH=!PYTHON_PATH:\=/!
        echo !PYTHON_PATH!
        exit /b 0
    )
)
if exist "%USERPROFILE%\AppData\Local\Programs\Python\Python311\python.exe" (
    "%USERPROFILE%\AppData\Local\Programs\Python\Python311\python.exe" --version >nul 2>&1
    if !ERRORLEVEL! == 0 (
        REM Convert backslashes to forward slashes
        set PYTHON_PATH=%USERPROFILE%\AppData\Local\Programs\Python\Python311\python.exe
        set PYTHON_PATH=!PYTHON_PATH:\=/!
        echo !PYTHON_PATH!
        exit /b 0
    )
)

REM Try to find Python in PATH - actually test execution, not just existence
REM Test py.exe first (Python launcher, usually works and bypasses store alias)
py.exe --version >nul 2>&1
if %ERRORLEVEL% == 0 (
    REM Verify it can actually run Python code (not just a stub)
    echo import sys > %TEMP%\test_python.py
    py.exe %TEMP%\test_python.py >nul 2>&1
    if %ERRORLEVEL% == 0 (
        del %TEMP%\test_python.py 2>nul
        echo py.exe
        exit /b 0
    )
    del %TEMP%\test_python.py 2>nul
)

REM Try python.exe - but verify it's not the Microsoft Store alias
REM The store alias will fail when trying to run actual Python code
where python.exe >nul 2>&1
if %ERRORLEVEL% == 0 (
    REM Test if it can actually execute Python code
    echo import sys > %TEMP%\test_python.py
    python.exe %TEMP%\test_python.py >nul 2>&1
    if %ERRORLEVEL% == 0 (
        REM It works, get the actual path to verify it's not in WindowsApps
        for /f "delims=" %%i in ('python.exe -c "import sys; print(sys.executable)" 2^>nul') do set PYTHON_PATH=%%i
        if defined PYTHON_PATH (
            echo %PYTHON_PATH% | findstr /i "WindowsApps" >nul
            if %ERRORLEVEL% neq 0 (
                REM Not the store alias, use it
                del %TEMP%\test_python.py 2>nul
                echo python.exe
                exit /b 0
            )
        )
    )
    del %TEMP%\test_python.py 2>nul
)

REM Try python (without .exe)
where python >nul 2>&1
if %ERRORLEVEL% == 0 (
    REM Test if it can actually execute Python code
    echo import sys > %TEMP%\test_python.py
    python %TEMP%\test_python.py >nul 2>&1
    if %ERRORLEVEL% == 0 (
        REM It works, get the actual path to verify it's not in WindowsApps
        for /f "delims=" %%i in ('python -c "import sys; print(sys.executable)" 2^>nul') do set PYTHON_PATH=%%i
        if defined PYTHON_PATH (
            echo %PYTHON_PATH% | findstr /i "WindowsApps" >nul
            if %ERRORLEVEL% neq 0 (
                REM Not the store alias, use it
                del %TEMP%\test_python.py 2>nul
                echo python
                exit /b 0
            )
        )
    )
    del %TEMP%\test_python.py 2>nul
)

REM Python not found, try to install it
echo Python not found. Attempting to download and install Python... >&2
echo. >&2

REM Check if PowerShell is available
where powershell.exe >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo PowerShell is required to download Python automatically. >&2
    echo Please install Python manually from https://www.python.org/downloads/ >&2
    echo Make sure to check "Add Python to PATH" during installation. >&2
    exit /b 1
)

REM Create temp directory for download
set TEMP_DIR=%TEMP%\pvxs-python-install
if not exist "%TEMP_DIR%" mkdir "%TEMP_DIR%"

REM Download Python installer (Python 3.11 for Windows x64)
set PYTHON_VERSION=3.11.9
set PYTHON_URL=https://www.python.org/ftp/python/%PYTHON_VERSION%/python-%PYTHON_VERSION%-amd64.exe
set INSTALLER=%TEMP_DIR%\python-installer.exe

echo Downloading Python %PYTHON_VERSION% from %PYTHON_URL%... >&2
powershell -NoProfile -ExecutionPolicy Bypass -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; try { Invoke-WebRequest -Uri '%PYTHON_URL%' -OutFile '%INSTALLER%' -UseBasicParsing; exit 0 } catch { Write-Host 'Download failed:' $_.Exception.Message; exit 1 } }" 2>&1
if %ERRORLEVEL% neq 0 (
    echo Failed to download Python installer. >&2
    echo Please install Python manually from https://www.python.org/downloads/ >&2
    echo Make sure to check "Add Python to PATH" during installation. >&2
    rmdir /s /q "%TEMP_DIR%" 2>nul
    exit /b 1
)

echo Installing Python (this may take a few minutes)... >&2
REM Install Python silently with options:
REM /quiet - silent installation
REM InstallAllUsers=1 - install for all users
REM PrependPath=1 - add Python to PATH
REM Include_test=0 - don't install test suite
REM Include_pip=1 - include pip
"%INSTALLER%" /quiet InstallAllUsers=1 PrependPath=1 Include_test=0 Include_pip=1
set INSTALL_ERROR=%ERRORLEVEL%

REM Clean up installer
del "%INSTALLER%" 2>nul
rmdir "%TEMP_DIR%" 2>nul

if %INSTALL_ERROR% neq 0 (
    echo Python installation failed with error code %INSTALL_ERROR%. >&2
    echo Please install Python manually from https://www.python.org/downloads/ >&2
    exit /b 1
)

echo Python installation completed. >&2
echo Waiting for installation to complete... >&2

REM Wait a moment for installation to finish
timeout /t 5 /nobreak >nul

REM Check installation locations directly (PATH may not be updated yet)
REM Check Program Files first (for InstallAllUsers=1)
REM Convert backslashes to forward slashes (no quotes - Make will handle quoting)
if exist "C:\Program Files\Python311\python.exe" (
    "C:\Program Files\Python311\python.exe" --version >nul 2>&1
    if !ERRORLEVEL! == 0 (
        echo C:/Program Files/Python311/python.exe
        exit /b 0
    )
)
if exist "C:\Program Files\Python310\python.exe" (
    "C:\Program Files\Python310\python.exe" --version >nul 2>&1
    if !ERRORLEVEL! == 0 (
        echo C:/Program Files/Python310/python.exe
        exit /b 0
    )
)
if exist "C:\Program Files\Python39\python.exe" (
    "C:\Program Files\Python39\python.exe" --version >nul 2>&1
    if !ERRORLEVEL! == 0 (
        echo C:/Program Files/Python39/python.exe
        exit /b 0
    )
)
if exist "C:\Program Files (x86)\Python311\python.exe" (
    "C:\Program Files (x86)\Python311\python.exe" --version >nul 2>&1
    if !ERRORLEVEL! == 0 (
        echo C:/Program Files (x86)/Python311/python.exe
        exit /b 0
    )
)

REM Check user installation locations
if exist "%LOCALAPPDATA%\Programs\Python\Python311\python.exe" (
    "%LOCALAPPDATA%\Programs\Python\Python311\python.exe" --version >nul 2>&1
    if !ERRORLEVEL! == 0 (
        set PYTHON_PATH=%LOCALAPPDATA%\Programs\Python\Python311\python.exe
        set PYTHON_PATH=!PYTHON_PATH:\=/!
        echo !PYTHON_PATH!
        exit /b 0
    )
)
if exist "%USERPROFILE%\AppData\Local\Programs\Python\Python311\python.exe" (
    "%USERPROFILE%\AppData\Local\Programs\Python\Python311\python.exe" --version >nul 2>&1
    if !ERRORLEVEL! == 0 (
        set PYTHON_PATH=%USERPROFILE%\AppData\Local\Programs\Python\Python311\python.exe
        set PYTHON_PATH=!PYTHON_PATH:\=/!
        echo !PYTHON_PATH!
        exit /b 0
    )
)

REM Try PATH-based detection (may work if PATH was updated)
REM Test py.exe first
py.exe --version >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo import sys > %TEMP%\test_python.py
    py.exe %TEMP%\test_python.py >nul 2>&1
    if %ERRORLEVEL% == 0 (
        del %TEMP%\test_python.py 2>nul
        echo py.exe
        exit /b 0
    )
    del %TEMP%\test_python.py 2>nul
)

REM Try python.exe but verify it works and isn't the store alias
where python.exe >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo import sys > %TEMP%\test_python.py
    python.exe %TEMP%\test_python.py >nul 2>&1
    if %ERRORLEVEL% == 0 (
        for /f "delims=" %%i in ('python.exe -c "import sys; print(sys.executable)" 2^>nul') do set PYTHON_PATH=%%i
        if defined PYTHON_PATH (
            echo %PYTHON_PATH% | findstr /i "WindowsApps" >nul
            if %ERRORLEVEL% neq 0 (
                del %TEMP%\test_python.py 2>nul
                echo python.exe
                exit /b 0
            )
        )
    )
    del %TEMP%\test_python.py 2>nul
)

REM Python was installed but cannot be found
echo Python was installed but cannot be found. >&2
echo Installation may have completed but Python is not accessible yet. >&2
echo Please check these locations manually: >&2
echo   C:\Program Files\Python311\ >&2
echo   C:\Program Files (x86)\Python311\ >&2
echo   %LOCALAPPDATA%\Programs\Python\Python311\ >&2
echo   %USERPROFILE%\AppData\Local\Programs\Python\Python311\ >&2
echo You may need to restart your command prompt for PATH changes to take effect. >&2
exit /b 1

