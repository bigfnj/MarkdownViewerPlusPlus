param(
    [ValidateSet("Debug", "Release")]
    [string] $Configuration = "Release",
    [string] $PluginsRoot = "",
    [switch] $Force
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$packageDir = Join-Path $repoRoot "build\cmake\vs2026-x64\package\$Configuration\MarkdownPlusPlus"

function Resolve-PluginsRoot {
    param([string] $RequestedPath)

    if ($RequestedPath) {
        return $RequestedPath
    }

    $candidates = @()
    if ($env:ProgramFiles) {
        $candidates += (Join-Path $env:ProgramFiles "Notepad++\plugins")
    }

    $programFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
    if ($programFilesX86) {
        $candidates += (Join-Path $programFilesX86 "Notepad++\plugins")
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    if ($candidates.Count -gt 0) {
        return $candidates[0]
    }

    throw "Could not resolve the Notepad++ plugins folder. Pass -PluginsRoot explicitly."
}

if (!(Test-Path -LiteralPath $packageDir)) {
    throw "Package folder not found: $packageDir. Build with -Package before installing."
}

$resolvedPluginsRoot = Resolve-PluginsRoot -RequestedPath $PluginsRoot
$targetDir = Join-Path $resolvedPluginsRoot "MarkdownPlusPlus"

if (!$Force) {
    $running = Get-Process -Name "notepad++" -ErrorAction SilentlyContinue
    if ($running) {
        throw "Notepad++ is running. Close Notepad++ or rerun with -Force if you want to try copying anyway."
    }
}

Write-Host "Installing Markdown++ native plugin"
Write-Host "  From: $packageDir"
Write-Host "  To:   $targetDir"

New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
Get-ChildItem -LiteralPath $targetDir -Force | Remove-Item -Recurse -Force
Copy-Item -Path (Join-Path $packageDir "*") -Destination $targetDir -Recurse -Force

Write-Host "Install complete."
