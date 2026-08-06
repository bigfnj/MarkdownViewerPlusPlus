<#
.SYNOPSIS
    Build the Markdown++ MSI installer (x64) from a packaged plugin folder.

.DESCRIPTION
    Wraps `wix build` for installer\Package.wxs. Resolves the WiX version from the
    installed `wix` dotnet tool and adds matching Util/UI extensions, then builds an
    x64 MSI that auto-detects Notepad++ and installs the plugin into its plugins folder.

.PARAMETER PayloadDir
    Path to the packaged "MarkdownPlusPlus" folder (contains MarkdownPlusPlus.dll and
    assets\). If omitted, the newest build\cmake\*\package\Release\MarkdownPlusPlus is used.

.PARAMETER Version
    Product version (x.y.z). If omitted, parsed from CMakeLists.txt.

.PARAMETER OutDir
    Output directory for the .msi. Defaults to build\installer.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\installer\build-installer.ps1
#>
[CmdletBinding()]
param(
    [string]$PayloadDir,
    [string]$Version,
    [string]$OutDir
)

$ErrorActionPreference = 'Stop'
$installerDir = $PSScriptRoot
$repoRoot     = Split-Path -Parent $installerDir

# --- Resolve version from CMakeLists.txt if not supplied -------------------------
if (-not $Version) {
    $cmake = Get-Content -Raw (Join-Path $repoRoot 'CMakeLists.txt')
    if ($cmake -notmatch 'VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
        throw "Could not find VERSION x.y.z in CMakeLists.txt; pass -Version explicitly."
    }
    $Version = $Matches[1]
}
Write-Host "Version: $Version"

# --- Resolve payload directory ---------------------------------------------------
if (-not $PayloadDir) {
    $candidates = @(
        Get-ChildItem -Path (Join-Path $repoRoot 'build\cmake') -Directory -ErrorAction SilentlyContinue |
            ForEach-Object { Join-Path $_.FullName 'package\Release\MarkdownPlusPlus' } |
            Where-Object { Test-Path $_ } |
            Sort-Object { (Get-Item $_).LastWriteTime } -Descending
    )
    if (-not $candidates) {
        throw "No packaged plugin found under build\cmake\*\package\Release\MarkdownPlusPlus. Build/package the plugin first, or pass -PayloadDir."
    }
    $PayloadDir = $candidates[0]
}
$PayloadDir = (Resolve-Path $PayloadDir).Path
if (-not (Test-Path (Join-Path $PayloadDir 'MarkdownPlusPlus.dll'))) {
    throw "PayloadDir '$PayloadDir' does not contain MarkdownPlusPlus.dll."
}
Write-Host "Payload:  $PayloadDir"

# --- Output path -----------------------------------------------------------------
if (-not $OutDir) { $OutDir = Join-Path $repoRoot 'build\installer' }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$msi = Join-Path $OutDir "MarkdownPlusPlus-$Version-win-x64.msi"

# --- Locate the wix tool (PATH, or the dotnet global-tools folder) ---------------
$wix = (Get-Command wix -ErrorAction SilentlyContinue).Source
if (-not $wix) {
    $fallback = Join-Path $env:USERPROFILE '.dotnet\tools\wix.exe'
    if (Test-Path $fallback) { $wix = $fallback }
}
if (-not $wix) {
    throw "The 'wix' tool was not found. Install it with: dotnet tool install --global wix"
}

# --- Ensure matching WiX extensions ---------------------------------------------
$wixVerRaw = (& $wix --version)                    # e.g. "5.0.2+aabbcc"
$wixVer    = ($wixVerRaw -split '\+')[0].Trim()
Write-Host "WiX:      $wixVer"
foreach ($ext in @('WixToolset.Util.wixext', 'WixToolset.UI.wixext')) {
    Write-Host "Adding extension $ext/$wixVer"
    & $wix extension add -g "$ext/$wixVer"
    if ($LASTEXITCODE -ne 0) { throw "Failed to add WiX extension $ext" }
}

# --- Build -----------------------------------------------------------------------
Write-Host "Building $msi"
& $wix build (Join-Path $installerDir 'Package.wxs') `
    -arch x64 `
    -ext WixToolset.Util.wixext `
    -ext WixToolset.UI.wixext `
    -d "Version=$Version" `
    -d "PayloadDir=$PayloadDir" `
    -o $msi
if ($LASTEXITCODE -ne 0) { throw "wix build failed with exit code $LASTEXITCODE" }

Write-Host ""
Write-Host "Built: $msi"