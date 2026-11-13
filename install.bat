@echo off
setlocal EnableDelayedExpansion

title SantientPHP Windows Installer

echo ============================================================
echo             SantientPHP Windows Installer
echo ============================================================
echo.

:: Check for Administrator privileges
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
if %errorLevel% neq 0 (
    echo.
    echo [ERROR] Installation failed with exit code %errorLevel%.
) else (
    echo.
    echo [SUCCESS] SantientPHP is now installed and ready to use!
)

echo.
echo Press any key to exit installer...
pause >nul

