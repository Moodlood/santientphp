:; # Unix shell polyglot check: if executed under bash/sh, delegate to install.sh
:; if [ -n "$BASH_VERSION" ] || [ -n "$ZSH_VERSION" ] || [ -n "$SH_VERSION" ] || [ "$(uname -s 2>/dev/null)" != "" ]; then
:;   SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
:;   if [ -f "$SCRIPT_DIR/install.sh" ]; then
:;     exec "$SCRIPT_DIR/install.sh" "$@"
:;   fi
:;   echo "[ERROR] On Unix/Linux/macOS, please execute ./install.sh"
:;   exit 1
:; fi
@echo off
setlocal EnableDelayedExpansion

title SantientPHP OS-Aware Windows Installer

echo ============================================================
echo             SantientPHP Windows Installer
echo ============================================================
echo.

:: 1. OS & Architecture Detection in CMD
echo [OS-AWARE] Detecting Operating System Environment...
for /f "tokens=4-5 delims=[.] " %%i in ('ver') do set OS_VER=%%i.%%j
echo   Operating System: Microsoft Windows [Version %OS_VER%]
echo   Architecture:     %PROCESSOR_ARCHITECTURE%
echo   Host Processor:   %NUMBER_OF_PROCESSORS% cores
echo.

:: 2. Check for Administrator privileges
net session >nul 2>&1
if %errorLevel% == 0 (
    goto :RunInstaller
) else (
    echo Administrator privileges are required to install to Program Files and Drive C.
    echo Requesting UAC Administrator elevation...
    echo.
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process cmd -ArgumentList '/c \"\"%~dp0install.bat\" %*\"' -Verb RunAs"
    exit /b
)

:RunInstaller
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
set SPH_EXIT=%errorLevel%

if %SPH_EXIT% neq 0 (
    echo.
    echo [ERROR] Installation failed with exit code %SPH_EXIT%.
) else (
    echo.
    echo [SUCCESS] SantientPHP is now installed and ready to use!
)

echo.
echo Press any key to exit installer...
pause >nul
exit /b %SPH_EXIT%
