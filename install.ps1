# ==============================================================================
# SantientPHP (SPH) Windows Installer
# Installs SantientPHP to Drive C and Program Files (like real PHP on Windows)
# ==============================================================================

[CmdletBinding()]
param(
    [string]$InstallDir = "C:\Program Files\SantientPHP",
    [string]$DriveCLink = "C:\SantientPHP",
    [switch]$SkipBuild,
    [switch]$NoJunction,
    [switch]$NoPathUpdate,
    [switch]$NoAssociation,
    [switch]$NoRegistry,
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

# Helper for colored output
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

# 1. Administrator Privilege Check & Auto-Elevation
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Warn "Administrator privileges are required to install to '$InstallDir' and update System PATH."
    Write-Step "Requesting elevation via Windows UAC prompt..."
    
    $argList = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    if ($SkipBuild)     { $argList += " -SkipBuild" }
    if ($NoJunction)     { $argList += " -NoJunction" }
    if ($NoPathUpdate)   { $argList += " -NoPathUpdate" }
    if ($NoAssociation)  { $argList += " -NoAssociation" }
    if ($NoRegistry)     { $argList += " -NoRegistry" }
    if ($Quiet)          { $argList += " -Quiet" }
    
    try {
        $proc = Start-Process powershell.exe -Verb RunAs -ArgumentList $argList -PassThru -Wait
        exit $proc.ExitCode
    } catch {
        Write-Err "UAC elevation failed or was declined by user: $_"
        exit 1
    }
}

# Banner
if (-not $Quiet) {
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "             SantientPHP Windows Installer                  " -ForegroundColor White
    Write-Host "       Native Compiled PHP for Drive C & Program Files      " -ForegroundColor DarkCyan
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host ""
}

# Locate repository root (script directory)
$RepoDir = $PSScriptRoot
if (-not (Test-Path (Join-Path $RepoDir "CMakeLists.txt"))) {
    $RepoDir = (Get-Item $PSScriptRoot).Parent.FullName
}

Write-Step "Source repository: $RepoDir"
Write-Step "Target destination: $InstallDir"
if (-not $NoJunction) {
    Write-Step "Drive C alias junction: $DriveCLink"
}

# 2. Build binaries if requested or if missing
$SphcBinary = Join-Path $RepoDir "build\bin\sphc.exe"
$RuntimeLib = Join-Path $RepoDir "build\lib\libsph_runtime.a"

if (-not $SkipBuild -and ((-not (Test-Path $SphcBinary)) -or (-not (Test-Path $RuntimeLib)))) {
    Write-Step "Building fresh SantientPHP binaries using Ninja / CMake..."
    Push-Location $RepoDir
    try {
        if (Get-Command ninja -ErrorAction SilentlyContinue) {
            & ninja -C build
        } else {
            & cmake --build build --config Release
        }
    } finally {
        Pop-Location
    }
}

if (-not (Test-Path $SphcBinary)) {
    Write-Err "Compiler binary '$SphcBinary' not found! Build the project before installing."
    exit 1
}

# 3. Create destination directory structure
Write-Step "Creating installation directories in '$InstallDir'..."
$Directories = @(
    $InstallDir,
    (Join-Path $InstallDir "bin"),
    (Join-Path $InstallDir "lib"),
    (Join-Path $InstallDir "runtime\core"),
    (Join-Path $InstallDir "stdlib"),
    (Join-Path $InstallDir "include"),
    (Join-Path $InstallDir "assets"),
    (Join-Path $InstallDir "docs"),
    (Join-Path $InstallDir "examples")
)

foreach ($dir in $Directories) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
}
Write-Success "Directory structure ready."

# 4. Copy Binaries
Write-Step "Installing executables and CLI wrappers..."
$DestBin = Join-Path $InstallDir "bin"

# sphc.exe (compiler executable)
Copy-Item -Path $SphcBinary -Destination (Join-Path $DestBin "sphc.exe") -Force

# sph.exe & santientphp.exe (identical aliases so you can run 'sph hello.sph' like 'php hello.php')
Copy-Item -Path $SphcBinary -Destination (Join-Path $DestBin "sph.exe") -Force
Copy-Item -Path $SphcBinary -Destination (Join-Path $DestBin "santientphp.exe") -Force

# sph_tests.exe if present
$TestBinary = Join-Path $RepoDir "build\bin\sph_tests.exe"
if (Test-Path $TestBinary) {
    Copy-Item -Path $TestBinary -Destination (Join-Path $DestBin "sph_tests.exe") -Force
}

# Batch wrappers (.cmd) for CMD and PowerShell convenience
$sphCmd = "@echo off`r`n`"%~dp0sphc.exe`" %*"
Set-Content -Path (Join-Path $DestBin "sph.cmd") -Value $sphCmd -Encoding ASCII
Set-Content -Path (Join-Path $DestBin "sphc.cmd") -Value $sphCmd -Encoding ASCII
Set-Content -Path (Join-Path $DestBin "santientphp.cmd") -Value $sphCmd -Encoding ASCII

Write-Success "Binaries installed: sphc.exe, sph.exe, santientphp.exe, and batch wrappers."

# 5. Copy Libraries and Runtime Sources
Write-Step "Installing SantientPHP runtime libraries and C headers..."
$DestLib = Join-Path $InstallDir "lib"
Copy-Item -Path (Join-Path $RepoDir "build\lib\libsph_runtime.a") -Destination (Join-Path $DestLib "libsph_runtime.a") -Force

# Modular runtime archives
$ModularLibs = @("libsph_core.a", "libsph_async.a", "libsph_compat.a")
foreach ($mlib in $ModularLibs) {
    $src = Join-Path $RepoDir "build\lib\$mlib"
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination (Join-Path $DestLib $mlib) -Force
    }
}

$CoreLib = Join-Path $RepoDir "build\lib\libsphc_core.a"
if (Test-Path $CoreLib) {
    Copy-Item -Path $CoreLib -Destination (Join-Path $DestLib "libsphc_core.a") -Force
}

# Runtime C source and header
$DestRuntime = Join-Path $InstallDir "runtime"
Copy-Item -Path (Join-Path $RepoDir "runtime\*") -Destination $DestRuntime -Recurse -Force

# Include directory
Copy-Item -Path (Join-Path $RepoDir "runtime\core\runtime.h") -Destination (Join-Path $InstallDir "include\runtime.h") -Force

Write-Success "Runtime libraries (libsph_runtime.a, libsph_core.a, libsph_async.a, libsph_compat.a) and headers installed."

# 5b. Copy Standard Library (stdlib)
Write-Step "Installing SantientPHP standard library..."
$DestStdlib = Join-Path $InstallDir "stdlib"
$SrcStdlib = Join-Path $RepoDir "stdlib"
if (Test-Path $SrcStdlib) {
    Copy-Item -Path "$SrcStdlib\*" -Destination $DestStdlib -Recurse -Force
    Write-Success "Standard library modules installed to '$DestStdlib'."
}

# 6. Copy Configuration Files (sph.ini - like php.ini)
Write-Step "Installing configuration files (sph.ini)..."
$IniFiles = @("sph.ini", "sph.ini-development", "sph.ini-production")
foreach ($ini in $IniFiles) {
    $src = Join-Path $RepoDir $ini
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination (Join-Path $InstallDir $ini) -Force
    }
}
Write-Success "Configuration installed: sph.ini, sph.ini-development, sph.ini-production."

# 7. Copy Assets, Icons, Documentation, and Examples
Write-Step "Installing documentation, examples, and icons..."
$IconsDir = Join-Path $RepoDir "assets\icons"
if (Test-Path $IconsDir) {
    Copy-Item -Path "$IconsDir\*" -Destination (Join-Path $InstallDir "assets") -Recurse -Force
}

# Docs
$Docs = @("README.md", "AGENT_GUIDE.md")
foreach ($doc in $Docs) {
    $src = Join-Path $RepoDir $doc
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination (Join-Path $InstallDir "docs\$doc") -Force
    }
}

# Examples
$ExamplesDir = Join-Path $RepoDir "examples"
if (Test-Path $ExamplesDir) {
    Copy-Item -Path "$ExamplesDir\*" -Destination (Join-Path $InstallDir "examples") -Recurse -Force
}
$MainSph = Join-Path $RepoDir "main.sph"
if (Test-Path $MainSph) {
    Copy-Item -Path $MainSph -Destination (Join-Path $InstallDir "examples\main.sph") -Force
}
Write-Success "Assets, documentation, and examples installed."

# 8. Copy Uninstaller scripts to installation directory
Copy-Item -Path (Join-Path $RepoDir "uninstall.ps1") -Destination (Join-Path $InstallDir "uninstall.ps1") -Force
Copy-Item -Path (Join-Path $RepoDir "uninstall.bat") -Destination (Join-Path $InstallDir "uninstall.bat") -Force

# 9. Create Drive C Directory Junction (C:\SantientPHP -> C:\Program Files\SantientPHP)
if (-not $NoJunction -and $DriveCLink -and ($DriveCLink -ne $InstallDir)) {
    Write-Step "Creating Drive C link '$DriveCLink' -> '$InstallDir'..."
    if (Test-Path $DriveCLink) {
        # Check if existing junction
        $item = Get-Item $DriveCLink -Force
        if ($item.Attributes -match "ReparsePoint") {
            [System.IO.Directory]::Delete($DriveCLink)
        }
    }
    
    try {
        New-Item -ItemType Junction -Path $DriveCLink -Target $InstallDir -Force | Out-Null
        Write-Success "Drive C junction created: '$DriveCLink' -> '$InstallDir'."
    } catch {
        Write-Warn "Could not create junction link at '$DriveCLink': $_"
    }
}

# 10. Update System Environment Variables (PATH, SPH_HOME, SANTIENTPHP_HOME, PATHEXT)
if (-not $NoPathUpdate) {
    Write-Step "Configuring System Environment Variables..."

    # SPH_HOME and SANTIENTPHP_HOME
    [Environment]::SetEnvironmentVariable("SPH_HOME", $InstallDir, [EnvironmentVariableTarget]::Machine)
    [Environment]::SetEnvironmentVariable("SANTIENTPHP_HOME", $InstallDir, [EnvironmentVariableTarget]::Machine)
    $env:SPH_HOME = $InstallDir
    $env:SANTIENTPHP_HOME = $InstallDir

    # PATH
    $binDir = Join-Path $InstallDir "bin"
    $cBinDir = Join-Path $DriveCLink "bin"
    $machinePath = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::Machine)
    $pathParts = [System.Collections.Generic.List[string]]($machinePath -split ';' | Where-Object { $_ -ne "" })

    $addedPath = $false
    if (-not ($pathParts -contains $binDir)) {
        $pathParts.Add($binDir)
        $addedPath = $true
    }
    if (-not $NoJunction -and (Test-Path $DriveCLink) -and (-not ($pathParts -contains $cBinDir))) {
        $pathParts.Add($cBinDir)
        $addedPath = $true
    }

    if ($addedPath) {
        $newPath = $pathParts -join ';'
        [Environment]::SetEnvironmentVariable("PATH", $newPath, [EnvironmentVariableTarget]::Machine)
        $env:PATH = "$env:PATH;$binDir;$cBinDir"
        Write-Success "Added '$binDir' and '$cBinDir' to System PATH."
    } else {
        Write-Success "Installation bin directory is already in System PATH."
    }

    # PATHEXT: append .SPH so scripts can run without extension
    $machinePathext = [Environment]::GetEnvironmentVariable("PATHEXT", [EnvironmentVariableTarget]::Machine)
    if ($machinePathext -notmatch "\.SPH") {
        $newPathext = "$machinePathext;.SPH"
        [Environment]::SetEnvironmentVariable("PATHEXT", $newPathext, [EnvironmentVariableTarget]::Machine)
        $env:PATHEXT = "$env:PATHEXT;.SPH"
        Write-Success "Added '.SPH' to System PATHEXT."
    }

    # Broadcast environment changes to Windows Shell
    try {
        Add-Type -Namespace Win32 -Name NativeMethods -MemberDefinition @"
        [System.Runtime.InteropServices.DllImport("user32.dll", SetLastError = true, CharSet = System.Runtime.InteropServices.CharSet.Auto)]
        public static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint Msg, UIntPtr wParam, string lParam, uint fuFlags, uint uTimeout, out UIntPtr lpdwResult);
"@ -ErrorAction SilentlyContinue
        $HWND_BROADCAST = [IntPtr]0xffff
        $WM_SETTINGCHANGE = 0x001A
        $res = [UIntPtr]::Zero
        [Win32.NativeMethods]::SendMessageTimeout($HWND_BROADCAST, $WM_SETTINGCHANGE, [UIntPtr]::Zero, "Environment", 2, 2000, [ref]$res) | Out-Null
    } catch {
        # Optional broadcast
    }
}

# 11. Register Windows File Associations & Context Menus (.sph)
if (-not $NoAssociation) {
    Write-Step "Registering Windows file associations and shell menus for .sph..."
    $installedSphc = Join-Path $InstallDir "bin\sphc.exe"
    $iconPath = Join-Path $InstallDir "assets\sph.ico"

    # .sph file extension
    $extKey = "HKLM:\SOFTWARE\Classes\.sph"
    if (-not (Test-Path $extKey)) { New-Item -Path $extKey -Force | Out-Null }
    Set-ItemProperty -Path $extKey -Name "(Default)" -Value "SantientPHP.Script"
    Set-ItemProperty -Path $extKey -Name "Content Type" -Value "text/plain"
    Set-ItemProperty -Path $extKey -Name "PerceivedType" -Value "text"

    # SantientPHP.Script ProgID
    $progKey = "HKLM:\SOFTWARE\Classes\SantientPHP.Script"
    if (-not (Test-Path $progKey)) { New-Item -Path $progKey -Force | Out-Null }
    Set-ItemProperty -Path $progKey -Name "(Default)" -Value "SantientPHP Source File"
    
    # Icon
    if (Test-Path $iconPath) {
        $iconKey = "$progKey\DefaultIcon"
        if (-not (Test-Path $iconKey)) { New-Item -Path $iconKey -Force | Out-Null }
        Set-ItemProperty -Path $iconKey -Name "(Default)" -Value "`"$iconPath`",0"
    }

    # Context Menu Actions:
    # 1. Run (Default)
    $runCmdKey = "$progKey\shell\open\command"
    if (-not (Test-Path $runCmdKey)) { New-Item -Path $runCmdKey -Force | Out-Null }
    Set-ItemProperty -Path "$progKey\shell\open" -Name "(Default)" -Value "Run with SantientPHP"
    Set-ItemProperty -Path $runCmdKey -Name "(Default)" -Value "`"$installedSphc`" run `"%1`" %*"

    # 2. Build Native (.exe)
    $buildCmdKey = "$progKey\shell\build\command"
    if (-not (Test-Path $buildCmdKey)) { New-Item -Path $buildCmdKey -Force | Out-Null }
    Set-ItemProperty -Path "$progKey\shell\build" -Name "(Default)" -Value "Compile with SantientPHP (-O2)"
    Set-ItemProperty -Path $buildCmdKey -Name "(Default)" -Value "`"$installedSphc`" build -O2 `"%1`""

    # 3. Format (.sph)
    $fmtCmdKey = "$progKey\shell\format\command"
    if (-not (Test-Path $fmtCmdKey)) { New-Item -Path $fmtCmdKey -Force | Out-Null }
    Set-ItemProperty -Path "$progKey\shell\format" -Name "(Default)" -Value "Format with SantientPHP Formatter"
    Set-ItemProperty -Path $fmtCmdKey -Name "(Default)" -Value "`"$installedSphc`" fmt `"%1`""

    Write-Success "Registered file association: .sph -> SantientPHP.Script."
}

# 12. Register in Windows Add/Remove Programs (Control Panel & Settings)
if (-not $NoRegistry) {
    Write-Step "Registering in Windows Installed Apps (Add/Remove Programs)..."
    $uninstallKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\SantientPHP"
    if (-not (Test-Path $uninstallKey)) { New-Item -Path $uninstallKey -Force | Out-Null }

    $uninstBat = Join-Path $InstallDir "uninstall.bat"
    $uninstPs1 = Join-Path $InstallDir "uninstall.ps1"
    $icon = Join-Path $InstallDir "assets\sph.ico"

    # Compute installed folder size in KB
    $folderSizeKB = [math]::Round(((Get-ChildItem $InstallDir -Recurse | Measure-Object -Property Length -Sum).Sum / 1KB))

    Set-ItemProperty -Path $uninstallKey -Name "DisplayName" -Value "SantientPHP 0.1.0 (x86_64)"
    Set-ItemProperty -Path $uninstallKey -Name "DisplayVersion" -Value "0.1.0"
    Set-ItemProperty -Path $uninstallKey -Name "Publisher" -Value "SantientPHP Development Team"
    Set-ItemProperty -Path $uninstallKey -Name "InstallLocation" -Value $InstallDir
    Set-ItemProperty -Path $uninstallKey -Name "DisplayIcon" -Value "$icon,0"
    Set-ItemProperty -Path $uninstallKey -Name "UninstallString" -Value "`"$uninstBat`""
    Set-ItemProperty -Path $uninstallKey -Name "QuietUninstallString" -Value "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$uninstPs1`" -Quiet"
    Set-ItemProperty -Path $uninstallKey -Name "InstallDate" -Value (Get-Date -Format "yyyyMMdd")
    Set-ItemProperty -Path $uninstallKey -Name "EstimatedSize" -Value $folderSizeKB
    Set-ItemProperty -Path $uninstallKey -Name "URLInfoAbout" -Value "https://github.com/santientphp"
    Set-ItemProperty -Path $uninstallKey -Name "Comments" -Value "Compiled Systems PHP with bare-metal performance, memory safety, and concurrency."

    Write-Success "Registered SantientPHP in Windows Programs & Features."
}

# 13. Post-Installation Verification & Health Diagnostic
Write-Step "Verifying installation health via SantientPHP Doctor..."
$installedSphc = Join-Path $InstallDir "bin\sphc.exe"

& $installedSphc doctor

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "      SantientPHP Installation Completed Successfully!       " -ForegroundColor White
Write-Host "============================================================" -ForegroundColor Green
Write-Host "Installed Locations:"
Write-Host "  * Program Files : $InstallDir" -ForegroundColor Cyan
if (-not $NoJunction) {
    Write-Host "  * Drive C Alias : $DriveCLink" -ForegroundColor Cyan
}
Write-Host "Available CLI commands from any terminal:"
Write-Host "  sphc --version       (Primary compiler command)" -ForegroundColor Yellow
Write-Host "  sph  --version       (Short PHP-like alias)" -ForegroundColor Yellow
Write-Host "  santientphp --help   (Full-name CLI command)" -ForegroundColor Yellow
Write-Host "  sphc doctor          (Toolchain health check)" -ForegroundColor Yellow
Write-Host ""
Write-Host "Try running an example:"
Write-Host "  sph `"$InstallDir\examples\hello.sph`"" -ForegroundColor Magenta
Write-Host ""

