:; # Unix shell polyglot check: if executed under bash/sh, delegate to uninstall.sh
:; if [ -n "$BASH_VERSION" ] || [ -n "$ZSH_VERSION" ] || [ -n "$SH_VERSION" ] || [ "$(uname -s 2>/dev/null)" != "" ]; then
:;   SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
:;   if [ -f "$SCRIPT_DIR/uninstall.sh" ]; then
:;     exec "$SCRIPT_DIR/uninstall.sh" "$@"
:;   fi
:;   echo "[ERROR] On Unix/Linux/macOS, please execute ./uninstall.sh"
:;   exit 1
:; fi
@echo off
setlocal EnableDelayedExpansion

title SantientPHP OS Environment & Uninstaller

echo ============================================================
echo        SantientPHP OS Environment & Uninstaller
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
    goto :RunUninstaller
) else (
    echo Administrator privileges are required to manage SantientPHP system installation.
    echo Requesting UAC Administrator elevation...
    echo.
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process cmd -ArgumentList '/c \"\"%~dp0uninstall.bat\" %*\"' -Verb RunAs"
    exit /b
)

:RunUninstaller
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall.ps1" %*
set SPH_EXIT=%errorLevel%

if %SPH_EXIT% neq 0 (
    echo.
    echo [ERROR] Process finished with exit code %SPH_EXIT%.
)

echo.
echo Press any key to exit...
pause >nul
exit /b %SPH_EXIT%
