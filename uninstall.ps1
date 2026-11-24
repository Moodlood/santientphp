# ==============================================================================
# SantientPHP (SPH) OS-Aware Environment Manager & Uninstaller
# Detects OS, audits developer toolchain resources, auto-installs missing prerequisites,
# or cleanly uninstalls SantientPHP from Program Files, Drive C, PATH, and Registry.
# ==============================================================================

[CmdletBinding()]
param(
    [string]$InstallDir = "C:\Program Files\SantientPHP",
    [string]$DriveCLink = "C:\SantientPHP",
    [switch]$InstallResources,
    [switch]$Repair,
    [switch]$Uninstall,
    [switch]$Quiet,
    [switch]$Force
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

function Write-Err {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

function Write-Info {
    param([string]$Message)
    if (-not $Quiet) {
        Write-Host "    $Message" -ForegroundColor Gray
    }
}

# 1. Administrator Check & Auto-Elevation
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Warn "Administrator privileges are recommended to manage SantientPHP system installation."
    Write-Step "Attempting elevation via Windows UAC prompt..."
    $elevated = $false
    try {
        $argList = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
        if ($InstallResources) { $argList += " -InstallResources" }
        if ($Repair)           { $argList += " -Repair" }
        if ($Uninstall)        { $argList += " -Uninstall" }
        if ($Quiet)            { $argList += " -Quiet" }
        if ($Force)            { $argList += " -Force" }
        $proc = Start-Process powershell.exe -Verb RunAs -ArgumentList $argList -PassThru -Wait
        exit $proc.ExitCode
    } catch {
        Write-Warn "UAC elevation not available or declined in this console. Continuing with current user privileges..."
    }
}

# 2. OS & Hardware Architecture Detection
function Get-OSDetails {
    $osObj = [PSCustomObject]@{
        Caption = [System.Environment]::OSVersion.VersionString
        BuildNumber = [System.Environment]::OSVersion.Version.Build
        Architecture = [System.Environment]::GetEnvironmentVariable("PROCESSOR_ARCHITECTURE")
        Cores = [System.Environment]::ProcessorCount
        SystemDrive = [System.Environment]::GetEnvironmentVariable("SystemDrive")
    }

    try {
        $wmi = Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue
        if ($wmi) {
            $osObj.Caption = $wmi.Caption
            $osObj.BuildNumber = $wmi.BuildNumber
            if ($wmi.OSArchitecture) {
                $osObj.Architecture = $wmi.OSArchitecture
            }
        }
    } catch {}

    return $osObj
}

$OS = Get-OSDetails

if (-not $Quiet) {
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "       SantientPHP OS-Aware Toolchain & Uninstaller         " -ForegroundColor White
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "  Operating System: $($OS.Caption) ($($OS.Architecture))" -ForegroundColor Gray
    Write-Host "  OS Build Number:  $($OS.BuildNumber)" -ForegroundColor Gray
    Write-Host "  Logical Cores:    $($OS.Cores)" -ForegroundColor Gray
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host ""
}

# 3. Package Manager Resolver
function Get-PackageManagerCmd {
    # 1. Check winget in PATH
    $w = Get-Command winget -ErrorAction SilentlyContinue
    if ($w) { return "winget" }

    # 2. Check winget in LocalAppData
    $localWinget = "$env:LOCALAPPDATA\Microsoft\WindowsApps\winget.exe"
    if (Test-Path $localWinget) { return $localWinget }

    # 3. Check Chocolatey
    $c = Get-Command choco -ErrorAction SilentlyContinue
    if ($c) { return "choco" }

    # 4. Check Scoop
    $s = Get-Command scoop -ErrorAction SilentlyContinue
    if ($s) { return "scoop" }

    return $null
}

# 4. Toolchain Resource Auditor
function Test-DeveloperToolchain {
    $checks = @()

    # C/C++ Compiler
    $gcc = Get-Command gcc -ErrorAction SilentlyContinue
    $clang = Get-Command clang -ErrorAction SilentlyContinue
    if ($gcc) {
        $ver = (& gcc --version 2>&1 | Select-Object -First 1)
        $checks += [PSCustomObject]@{ Category="Compiler"; Name="Host C/C++ Compiler (gcc)"; Found=$true; Details=$ver; Pkg="BrechtSanders.WinLibs.POSIX.UCRT" }
    } elseif ($clang) {
        $ver = (& clang --version 2>&1 | Select-Object -First 1)
        $checks += [PSCustomObject]@{ Category="Compiler"; Name="Host C/C++ Compiler (clang)"; Found=$true; Details=$ver; Pkg="LLVM.LLVM" }
    } else {
        $checks += [PSCustomObject]@{ Category="Compiler"; Name="Host C/C++ Compiler (GCC/Clang)"; Found=$false; Details="Not found in PATH (C++20 required)"; Pkg="BrechtSanders.WinLibs.POSIX.UCRT" }
    }

    # Linker
    $ld = Get-Command ld -ErrorAction SilentlyContinue
    if ($ld) {
        $ver = (& ld --version 2>&1 | Select-Object -First 1)
        $checks += [PSCustomObject]@{ Category="Toolchain"; Name="Native Linker (ld)"; Found=$true; Details=$ver; Pkg="" }
    } else {
        $checks += [PSCustomObject]@{ Category="Toolchain"; Name="Native Linker (ld)"; Found=$false; Details="Not found in PATH (included with GCC/LLVM)"; Pkg="" }
    }

    # CMake
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) {
        $ver = (& cmake --version 2>&1 | Select-Object -First 1)
        $checks += [PSCustomObject]@{ Category="Build System"; Name="CMake (>=3.20)"; Found=$true; Details=$ver; Pkg="Kitware.CMake" }
    } else {
        $checks += [PSCustomObject]@{ Category="Build System"; Name="CMake (>=3.20)"; Found=$false; Details="Not found in PATH"; Pkg="Kitware.CMake" }
    }

    # Ninja
    $ninja = Get-Command ninja -ErrorAction SilentlyContinue
    if ($ninja) {
        $ver = (& ninja --version 2>&1 | Select-Object -First 1)
        $checks += [PSCustomObject]@{ Category="Build System"; Name="Ninja Build System"; Found=$true; Details="ninja $ver"; Pkg="Ninja-build.Ninja" }
    } else {
        $checks += [PSCustomObject]@{ Category="Build System"; Name="Ninja Build System"; Found=$false; Details="Not found in PATH"; Pkg="Ninja-build.Ninja" }
    }

    # Git
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git) {
        $ver = (& git --version 2>&1 | Select-Object -First 1)
        $checks += [PSCustomObject]@{ Category="VCS"; Name="Version Control (git)"; Found=$true; Details=$ver; Pkg="Git.Git" }
    } else {
        $checks += [PSCustomObject]@{ Category="VCS"; Name="Version Control (git)"; Found=$false; Details="Not found in PATH"; Pkg="Git.Git" }
    }

    # Windows Native Subsystems
    $sysDir = [System.Environment]::GetFolderPath([System.Environment+SpecialFolder]::System)
    $dlls = @("kernel32.dll", "winhttp.dll", "winsqlite3.dll")
    $missingDlls = @()
    foreach ($dll in $dlls) {
        if (-not (Test-Path (Join-Path $sysDir $dll))) {
            $missingDlls += $dll
        }
    }
    if ($missingDlls.Count -eq 0) {
        $checks += [PSCustomObject]@{ Category="Subsystems"; Name="Windows Native Subsystems"; Found=$true; Details="kernel32.dll, winhttp.dll, winsqlite3.dll present"; Pkg="" }
    } else {
        $checks += [PSCustomObject]@{ Category="Subsystems"; Name="Windows Native Subsystems"; Found=$false; Details="Missing: $($missingDlls -join ', ')"; Pkg="" }
    }

    return $checks
}

# 5. Missing Resource Auto-Installer
function Install-MissingResources {
    Write-Step "Checking for missing development prerequisites..."
    $pm = Get-PackageManagerCmd
    $checks = Test-DeveloperToolchain
    $missing = $checks | Where-Object { -not $_.Found -and $_.Pkg -ne "" }

    if ($missing.Count -eq 0) {
        Write-Success "All required developer resources (Compiler, CMake, Ninja, Git) are already installed!"
        return $true
    }

    Write-Warn "The following required developer resources are missing on this system:"
    foreach ($m in $missing) {
        Write-Host "  - [$($m.Category)] $($m.Name) (Package: $($m.Pkg))" -ForegroundColor Yellow
    }

    if (-not $pm) {
        Write-Err "No supported Windows package manager (winget, choco, scoop) was detected."
        Write-Host "Please install missing tools manually:" -ForegroundColor Cyan
        Write-Host "  1. MinGW-w64 GCC (C++20): https://winlibs.com/" -ForegroundColor Gray
        Write-Host "  2. CMake (>=3.20):        https://cmake.org/download/" -ForegroundColor Gray
        Write-Host "  3. Ninja:                 https://github.com/ninja-build/ninja/releases" -ForegroundColor Gray
        Write-Host "  4. Git for Windows:       https://git-scm.com/download/win" -ForegroundColor Gray
        return $false
    }

    Write-Step "Automatically installing missing resources using Windows Package Manager ($pm)..."
    foreach ($m in $missing) {
        Write-Step "Installing $($m.Name) [ID: $($m.Pkg)]..."
        try {
            if ($pm -like "*winget*") {
                & $pm install --id $m.Pkg -e --silent --accept-source-agreements --accept-package-agreements
            } elseif ($pm -eq "choco") {
                $chocoPkg = switch ($m.Pkg) {
                    "BrechtSanders.WinLibs.POSIX.UCRT" { "mingw" }
                    "Kitware.CMake" { "cmake" }
                    "Ninja-build.Ninja" { "ninja" }
                    "Git.Git" { "git" }
                    default { $m.Pkg }
                }
                & choco install $chocoPkg -y
            } elseif ($pm -eq "scoop") {
                $scoopPkg = switch ($m.Pkg) {
                    "BrechtSanders.WinLibs.POSIX.UCRT" { "gcc" }
                    "Kitware.CMake" { "cmake" }
                    "Ninja-build.Ninja" { "ninja" }
                    "Git.Git" { "git" }
                    default { $m.Pkg }
                }
                & scoop install $scoopPkg
            }
            Write-Success "Successfully installed $($m.Name)."
        } catch {
            Write-Warn "Could not auto-install $($m.Pkg): $_"
        }
    }

    # Refresh PATH in current process
    Write-Step "Refreshing System PATH in current session..."
    $machinePath = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::Machine)
    $userPath = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::User)
    $env:PATH = "$machinePath;$userPath"

    Write-Success "Developer resources installed and PATH refreshed."
    return $true
}

# 6. SantientPHP Uninstaller Routine
function Uninstall-SantientPHP {
    Write-Step "Starting clean uninstallation of SantientPHP..."

    # 1. Remove Environment Variables
    Write-Step "Removing environment variables (SPH_HOME, SANTIENTPHP_HOME)..."
    [Environment]::SetEnvironmentVariable("SPH_HOME", $null, [EnvironmentVariableTarget]::Machine)
    [Environment]::SetEnvironmentVariable("SANTIENTPHP_HOME", $null, [EnvironmentVariableTarget]::Machine)

    # 2. Clean System PATH
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

    # 3. Clean PATHEXT
    $machinePathext = [Environment]::GetEnvironmentVariable("PATHEXT", [EnvironmentVariableTarget]::Machine)
    if ($machinePathext) {
        $pathextParts = $machinePathext -split ';' | Where-Object { $_ -ne "" -and $_ -ne ".SPH" }
        $newPathext = $pathextParts -join ';'
        [Environment]::SetEnvironmentVariable("PATHEXT", $newPathext, [EnvironmentVariableTarget]::Machine)
        Write-Success "Cleaned System PATHEXT."
    }

    # Broadcast environment changes to all running processes
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

    # 4. Remove Windows File Associations
    Write-Step "Removing Windows file associations (.sph)..."
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

    # 5. Remove Add/Remove Programs Registry Key
    Write-Step "Removing Windows Programs & Features registration..."
    $uninstallKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\SantientPHP"
    if (Test-Path $uninstallKey) {
        Remove-Item -Path $uninstallKey -Recurse -Force -ErrorAction SilentlyContinue
    }
    Write-Success "Uninstalled from Windows Programs & Features."

    # 6. Remove Drive C Junction Link
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

    # 7. Remove Program Files Installation Directory
    if (Test-Path $InstallDir) {
        Write-Step "Removing installation directory '$InstallDir'..."
        $currentScript = $PSCommandPath
        if ($currentScript -and $currentScript.StartsWith($InstallDir, [System.StringComparison]::OrdinalIgnoreCase)) {
            Start-Process cmd.exe -ArgumentList "/c timeout /t 1 >nul & rd /s /q `"$InstallDir`"" -WindowStyle Hidden
        } else {
            Remove-Item -Path $InstallDir -Recurse -Force -ErrorAction SilentlyContinue
        }
        Write-Success "Installation files removed."
    }

    if (-not $Quiet) {
        Write-Host ""
        Write-Host "[SUCCESS] SantientPHP has been completely removed from this PC." -ForegroundColor Green
        Write-Host ""
    }
}

# 7. SantientPHP Installation & Repair Routine
function Repair-SantientPHP {
    Write-Step "Starting SantientPHP Resource Provisioning & Reinstall/Repair..."
    Install-MissingResources | Out-Null

    $RepoDir = $PSScriptRoot
    if (-not (Test-Path (Join-Path $RepoDir "CMakeLists.txt"))) {
        $RepoDir = (Get-Item $PSScriptRoot).Parent.FullName
    }
    $InstallScript = Join-Path $RepoDir "install.ps1"
    if (Test-Path $InstallScript) {
        Write-Step "Running installer to build and configure SantientPHP..."
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $InstallScript
    } else {
        Write-Err "Installer script not found at '$InstallScript'."
    }
}

# 8. Execution Flow Dispatcher
$checks = Test-DeveloperToolchain
$sphInstalled = (Test-Path (Join-Path $InstallDir "bin\sphc.exe")) -or (Test-Path $DriveCLink)

if ($Uninstall -or $Force) {
    Uninstall-SantientPHP
    exit 0
}

if ($InstallResources) {
    Install-MissingResources | Out-Null
    exit 0
}

if ($Repair) {
    Repair-SantientPHP
    exit 0
}

# Interactive OS-Aware Menu Mode
if (-not $Quiet) {
    Write-Host "SantientPHP Status:" -ForegroundColor Cyan
    if ($sphInstalled) {
        Write-Host "  [OK] SantientPHP is currently installed in: $InstallDir" -ForegroundColor Green
        if (Test-Path $DriveCLink) {
            Write-Host "  [OK] Drive C alias junction is active:     $DriveCLink" -ForegroundColor Green
        }
    } else {
        Write-Host "  [!] SantientPHP is NOT installed in: $InstallDir" -ForegroundColor Yellow
    }

    Write-Host ""
    Write-Host "Developer Toolchain Resources Check:" -ForegroundColor Cyan
    foreach ($c in $checks) {
        if ($c.Found) {
            Write-Host "  [OK] $($c.Name): $($c.Details)" -ForegroundColor Green
        } else {
            Write-Host "  [MISSING] $($c.Name): $($c.Details)" -ForegroundColor Red
        }
    }

    $missingCount = ($checks | Where-Object { -not $_.Found -and $_.Pkg -ne "" }).Count

    Write-Host ""
    Write-Host "Select an action:" -ForegroundColor White
    if ($sphInstalled) {
        Write-Host "  [1] Uninstall SantientPHP completely (remove files, PATH, registry, junctions) [Default]" -ForegroundColor White
    } else {
        Write-Host "  [1] Clean up any residual SantientPHP settings, PATH, and registry keys" -ForegroundColor White
    }
    Write-Host "  [2] Auto-install missing developer resources (GCC, CMake, Ninja, Git)" -ForegroundColor White
    Write-Host "  [3] Install / Repair SantientPHP with all required resources" -ForegroundColor White
    Write-Host "  [4] Exit" -ForegroundColor White
    Write-Host ""

    $choice = Read-Host "Enter option [1-4] (default: 1)"
    if ([string]::IsNullOrWhiteSpace($choice)) {
        $choice = "1"
    }

    switch ($choice.Trim()) {
        "1" {
            Uninstall-SantientPHP
        }
        "2" {
            Install-MissingResources | Out-Null
        }
        "3" {
            Repair-SantientPHP
        }
        "4" {
            Write-Host "Exiting without making changes." -ForegroundColor Gray
            exit 0
        }
        default {
            Write-Warn "Invalid choice '$choice'. Defaulting to uninstallation."
            Uninstall-SantientPHP
        }
    }
} else {
    # In quiet mode without explicit flags, default to uninstallation
    Uninstall-SantientPHP
}
