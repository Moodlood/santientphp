# ==============================================================================
# SantientPHP (SPH) Windows Uninstaller
# Completely removes SantientPHP from Program Files, Drive C, PATH, and Registry
# ==============================================================================

[CmdletBinding()]
param(
    [string]$InstallDir = "C:\Program Files\SantientPHP",
    [string]$DriveCLink = "C:\SantientPHP",
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

function Write-Step {
    param([string]$Message)
    if (-not $Quiet) {
        Write-Host "==> $Message" -ForegroundColor Cyan
    }
}

function Write-Success {
    param([string]$Message)
    if (-not $Quiet) {
        Write-Host "[OK] $Message" -ForegroundColor Green
    }
}

function Write-Warn {
    param([string]$Message)
    Write-Host "[WARN] $Message" -ForegroundColor Yellow
}

# 1. Administrator check & auto-elevation
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Warn "Administrator privileges are required to uninstall SantientPHP."
    Write-Step "Requesting elevation via Windows UAC prompt..."
    try {
        $argList = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
        if ($Quiet) { $argList += " -Quiet" }
        $proc = Start-Process powershell.exe -Verb RunAs -ArgumentList $argList -PassThru -Wait
        exit $proc.ExitCode
    } catch {
        Write-Host "[ERROR] Elevation declined or failed: $_" -ForegroundColor Red
        exit 1
    }
}

if (-not $Quiet) {
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "            SantientPHP Windows Uninstaller                 " -ForegroundColor White
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host ""
}

# 2. Remove System PATH and Environment Variables
Write-Step "Removing environment variables (SPH_HOME, SANTIENTPHP_HOME)..."
[Environment]::SetEnvironmentVariable("SPH_HOME", $null, [EnvironmentVariableTarget]::Machine)
[Environment]::SetEnvironmentVariable("SANTIENTPHP_HOME", $null, [EnvironmentVariableTarget]::Machine)

$binDir = Join-Path $InstallDir "bin"
$cBinDir = Join-Path $DriveCLink "bin"

$machinePath = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::Machine)
if ($machinePath) {
    $pathParts = $machinePath -split ';' | Where-Object { 
        $_ -ne "" -and 
        $_ -ne $binDir -and 
        $_ -ne "$binDir\" -and 
        $_ -ne $cBinDir -and 
        $_ -ne "$cBinDir\" 
    }
    $newPath = $pathParts -join ';'
    [Environment]::SetEnvironmentVariable("PATH", $newPath, [EnvironmentVariableTarget]::Machine)
    Write-Success "Cleaned System PATH."
}

# Clean PATHEXT
$machinePathext = [Environment]::GetEnvironmentVariable("PATHEXT", [EnvironmentVariableTarget]::Machine)
if ($machinePathext) {
    $pathextParts = $machinePathext -split ';' | Where-Object { $_ -ne "" -and $_ -ne ".SPH" }
    $newPathext = $pathextParts -join ';'
    [Environment]::SetEnvironmentVariable("PATHEXT", $newPathext, [EnvironmentVariableTarget]::Machine)
    Write-Success "Cleaned System PATHEXT."
}

# Broadcast environment changes
try {
    Add-Type -Namespace Win32 -Name NativeMethods -MemberDefinition @"
    [System.Runtime.InteropServices.DllImport("user32.dll", SetLastError = true, CharSet = System.Runtime.InteropServices.CharSet.Auto)]
    public static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint Msg, UIntPtr wParam, string lParam, uint fuFlags, uint uTimeout, out UIntPtr lpdwResult);
"@ -ErrorAction SilentlyContinue
    $HWND_BROADCAST = [IntPtr]0xffff
    $WM_SETTINGCHANGE = 0x001A
    $res = [UIntPtr]::Zero
    [Win32.NativeMethods]::SendMessageTimeout($HWND_BROADCAST, $WM_SETTINGCHANGE, [UIntPtr]::Zero, "Environment", 2, 2000, [ref]$res) | Out-Null
} catch {}

# 3. Remove Registry File Associations
Write-Step "Removing Windows file associations..."
$KeysToRemove = @(
    "HKLM:\SOFTWARE\Classes\.sph",
    "HKLM:\SOFTWARE\Classes\SantientPHP.Script",
    "HKCU:\SOFTWARE\Classes\.sph",
    "HKCU:\SOFTWARE\Classes\SantientPHP.Script"
)
foreach ($key in $KeysToRemove) {
    if (Test-Path $key) {
        Remove-Item -Path $key -Recurse -Force -ErrorAction SilentlyContinue
    }
}
Write-Success "File associations removed."

# 4. Remove Add/Remove Programs Registry Key
Write-Step "Removing Windows Add/Remove Programs registration..."
$uninstallKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\SantientPHP"
if (Test-Path $uninstallKey) {
    Remove-Item -Path $uninstallKey -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Success "Uninstalled from Windows Programs & Features."

# 5. Remove Drive C Junction Link
if (Test-Path $DriveCLink) {
    Write-Step "Removing Drive C link '$DriveCLink'..."
    $item = Get-Item $DriveCLink -Force -ErrorAction SilentlyContinue
    if ($item -and ($item.Attributes -match "ReparsePoint")) {
        [System.IO.Directory]::Delete($DriveCLink)
    } else {
        Remove-Item -Path $DriveCLink -Recurse -Force -ErrorAction SilentlyContinue
    }
    Write-Success "Drive C link removed."
}

# 6. Remove Program Files Installation Directory
if (Test-Path $InstallDir) {
    Write-Step "Removing installation directory '$InstallDir'..."
    # Self-deletion: schedule deletion if running from inside the directory
    $currentScript = $PSCommandPath
    if ($currentScript -and $currentScript.StartsWith($InstallDir, [System.StringComparison]::OrdinalIgnoreCase)) {
        # Running from inside install dir: launch external cmd to remove folder after exit
        Start-Process cmd.exe -ArgumentList "/c timeout /t 1 >nul & rd /s /q `"$InstallDir`"" -WindowStyle Hidden
    } else {
        Remove-Item -Path $InstallDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    Write-Success "Installation files removed."
}

if (-not $Quiet) {
    Write-Host ""
    Write-Host "SantientPHP has been successfully uninstalled from this PC." -ForegroundColor Green
    Write-Host ""
}

