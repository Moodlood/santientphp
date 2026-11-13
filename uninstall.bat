@echo off
setlocal EnableDelayedExpansion

title SantientPHP Windows Uninstaller

echo ============================================================
echo            SantientPHP Windows Uninstaller
echo ============================================================
echo.

:: Check for Administrator privileges
net session >nul 2>&1
if %errorLevel% == 0 (
    goto :RunUninstaller
) else (
    echo Administrator privileges are required to uninstall SantientPHP.
    echo Requesting UAC Administrator elevation...
    echo.
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process cmd -ArgumentList '/c \"\"%~dp0uninstall.bat\" %*\"' -Verb RunAs"
    exit /b
)

:RunUninstaller
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall.ps1" %*
if %errorLevel% neq 0 (
    echo.
    echo [ERROR] Uninstallation failed with exit code %errorLevel%.
) else (
    echo.
    echo [SUCCESS] SantientPHP was completely removed.
)

echo.
echo Press any key to exit...
pause >nul

