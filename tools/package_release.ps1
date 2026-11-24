# ==============================================================================
# SantientPHP (SPH) Release Packager for Windows x64
# Builds a portable .zip distribution ready for GitHub Releases
# ==============================================================================

[CmdletBinding()]
param(
    [string]$Version = "0.2.1",
    [string]$OutputDir = "dist"
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Get-Item $PSScriptRoot).Parent.FullName
$SphcBin = Join-Path $RepoRoot "build\bin\sphc.exe"

if (-not (Test-Path $SphcBin)) {
    Write-Error "Compiler binary '$SphcBin' not found! Build the project before packaging."
    exit 1
}

$DistName = "santientphp-v$Version-windows-x64"
$DistDir = Join-Path $RepoRoot "$OutputDir\$DistName"
$ZipFile = Join-Path $RepoRoot "$OutputDir\$DistName.zip"
$DistOutDir = Join-Path $RepoRoot $OutputDir

if (Test-Path $DistDir) {
    Remove-Item -Recurse -Force $DistDir
}
if (Test-Path $ZipFile) {
    Remove-Item -Force $ZipFile
}
New-Item -ItemType Directory -Path (Join-Path $DistDir "bin") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $DistDir "lib") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $DistDir "runtime") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $DistDir "stdlib") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $DistDir "examples") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $DistDir "docs") -Force | Out-Null

Write-Host "==> Staging binaries..." -ForegroundColor Cyan
Copy-Item $SphcBin (Join-Path $DistDir "bin\sphc.exe") -Force
Copy-Item $SphcBin (Join-Path $DistDir "bin\sph.exe") -Force
Copy-Item $SphcBin (Join-Path $DistDir "bin\santientphp.exe") -Force

$SphTests = Join-Path $RepoRoot "build\bin\sph_tests.exe"
if (Test-Path $SphTests) {
    Copy-Item $SphTests (Join-Path $DistDir "bin\sph_tests.exe") -Force
}

Write-Host "==> Staging static runtime libraries..." -ForegroundColor Cyan
Get-ChildItem (Join-Path $RepoRoot "build\lib\*.a") | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $DistDir "lib") -Force
}

Write-Host "==> Staging runtime headers and sources..." -ForegroundColor Cyan
Copy-Item (Join-Path $RepoRoot "runtime\*") (Join-Path $DistDir "runtime") -Recurse -Force

Write-Host "==> Staging standard library..." -ForegroundColor Cyan
Copy-Item (Join-Path $RepoRoot "stdlib\*") (Join-Path $DistDir "stdlib") -Recurse -Force

Write-Host "==> Staging examples & docs..." -ForegroundColor Cyan
Copy-Item (Join-Path $RepoRoot "examples\*") (Join-Path $DistDir "examples") -Recurse -Force
Copy-Item (Join-Path $RepoRoot "docs\*") (Join-Path $DistDir "docs") -Recurse -Force

Write-Host "==> Staging configs & scripts..." -ForegroundColor Cyan
@("sph.ini", "sph.ini-development", "sph.ini-production", "sph.toml", "install.bat", "install.ps1", "install.sh", "uninstall.bat", "uninstall.ps1", "uninstall.sh", "LICENSE", "README.md", "main.sph", "hello.sph") | ForEach-Object {
    $src = Join-Path $RepoRoot $_
    if (Test-Path $src) {
        Copy-Item $src (Join-Path $DistDir $_) -Force
    }
}

Write-Host "==> Creating ZIP archive: $ZipFile..." -ForegroundColor Cyan
Compress-Archive -Path "$DistDir\*" -DestinationPath $ZipFile -Force

Write-Host "==> Copying VS Code Extension to dist/..." -ForegroundColor Cyan
$VsixSource = Join-Path $RepoRoot "editors\vscode\santientphp-$Version.vsix"
if (Test-Path $VsixSource) {
    Copy-Item $VsixSource (Join-Path $DistOutDir "santientphp-$Version.vsix") -Force
}

Write-Host "==> Calculating SHA256 checksums..." -ForegroundColor Cyan
$ChecksumFile = Join-Path $DistOutDir "SHA256SUMS.txt"
if (Test-Path $ChecksumFile) {
    Remove-Item $ChecksumFile -Force
}

Get-ChildItem -Path $DistOutDir -File | Where-Object { $_.Extension -in '.zip', '.vsix' } | ForEach-Object {
    $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower()
    $line = "$hash  $($_.Name)"
    Add-Content -Path $ChecksumFile -Value $line
    Write-Host "    $line" -ForegroundColor Gray
}

Write-Host "`n[SUCCESS] Release packaging complete!" -ForegroundColor Green
Write-Host "Distribution archive: $ZipFile"
Write-Host "VS Code extension:    $(Join-Path $DistOutDir "santientphp-$Version.vsix")"
Write-Host "Checksums:            $ChecksumFile"

