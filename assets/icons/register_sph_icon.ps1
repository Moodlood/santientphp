# SantientPHP (.sph) Windows File Icon and Type Association Script
# Run this PowerShell script to associate .sph files with the SantientPHP icon in Windows Explorer.

$ErrorActionPreference = "Stop"

$IconPath = Join-Path $PSScriptRoot "sph.ico"
$SphcPath = (Resolve-Path (Join-Path $PSScriptRoot "..\..\build\bin\sphc.exe") -ErrorAction SilentlyContinue).Path

Write-Host "Registering SantientPHP (.sph) file icon in Windows Explorer..." -ForegroundColor Cyan
Write-Host "Icon: $IconPath"

# 1. Register .sph extension under current user
$ExtKey = "HKCU:\Software\Classes\.sph"
if (-not (Test-Path $ExtKey)) {
    New-Item -Path $ExtKey -Force | Out-Null
}
Set-ItemProperty -Path $ExtKey -Name "(Default)" -Value "SantientPHP.Source"
Set-ItemProperty -Path $ExtKey -Name "Content Type" -Value "text/plain"
Set-ItemProperty -Path $ExtKey -Name "PerceivedType" -Value "text"

# 2. Register ProgID and DefaultIcon
$ProgKey = "HKCU:\Software\Classes\SantientPHP.Source"
if (-not (Test-Path $ProgKey)) {
    New-Item -Path $ProgKey -Force | Out-Null
}
Set-ItemProperty -Path $ProgKey -Name "(Default)" -Value "SantientPHP Source File"

$DefaultIconKey = "$ProgKey\DefaultIcon"
if (-not (Test-Path $DefaultIconKey)) {
    New-Item -Path $DefaultIconKey -Force | Out-Null
}
Set-ItemProperty -Path $DefaultIconKey -Name "(Default)" -Value "`"$IconPath`",0"

# 3. Add shell commands (Compile & Run) if sphc.exe exists
if ($SphcPath -and (Test-Path $SphcPath)) {
    $RunCmdKey = "$ProgKey\shell\run_sph\command"
    if (-not (Test-Path $RunCmdKey)) {
        New-Item -Path $RunCmdKey -Force | Out-Null
    }
    Set-ItemProperty -Path "$ProgKey\shell\run_sph" -Name "(Default)" -Value "Run with SantientPHP (sphc)"
    Set-ItemProperty -Path $RunCmdKey -Name "(Default)" -Value "`"$SphcPath`" run `"%1`""
}

Write-Host "[OK] SantientPHP (.sph) file icon registered successfully!" -ForegroundColor Green
Write-Host "Restart Windows Explorer or refresh the folder to view updated icons."

