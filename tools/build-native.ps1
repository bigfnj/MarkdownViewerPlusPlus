param(
    [ValidateSet("Debug", "Release")]
    [string] $Configuration = "Release",
    [switch] $Package,
    [switch] $Install,
    [string] $PluginsRoot = "",
    [switch] $ForceInstall
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$logDir = Join-Path $repoRoot "build\logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

function Invoke-LoggedStep {
    param(
        [string] $Name,
        [string] $LogFile,
        [scriptblock] $Command
    )

    Write-Host "==> $Name"
    Push-Location $repoRoot
    try {
        $previousErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            & $Command 2>&1 | Tee-Object -FilePath $LogFile
            $exitCode = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $previousErrorActionPreference
        }

        if ($exitCode -ne 0) {
            throw "$Name failed with exit code $exitCode. See $LogFile"
        }
    }
    finally {
        Pop-Location
    }
}

Invoke-LoggedStep `
    -Name "Install native dependencies" `
    -LogFile (Join-Path $logDir "native-deps.log") `
    -Command { python .\tools\install-native-deps.py }

Invoke-LoggedStep `
    -Name "Configure CMake VS2026 x64" `
    -LogFile (Join-Path $logDir "cmake-configure-vs2026-x64.log") `
    -Command { cmake --preset vs2026-x64 }

$buildPreset = if ($Configuration -eq "Debug") { "vs2026-debug" } else { "vs2026-release" }
Invoke-LoggedStep `
    -Name "Build Markdown++ native $Configuration" `
    -LogFile (Join-Path $logDir "cmake-build-$Configuration.log") `
    -Command { cmake --build --preset $buildPreset }

$shouldPackage = $Package -or $Install

if ($shouldPackage) {
    Invoke-LoggedStep `
        -Name "Package Markdown++ native $Configuration" `
        -LogFile (Join-Path $logDir "cmake-package-$Configuration.log") `
        -Command { cmake --build .\build\cmake\vs2026-x64 --config $Configuration --target markdownplusplus_package }
}

if ($Install) {
    Invoke-LoggedStep `
        -Name "Install Markdown++ native plugin $Configuration" `
        -LogFile (Join-Path $logDir "native-install-$Configuration.log") `
        -Command {
            $installArgs = @(
                "-ExecutionPolicy", "Bypass",
                "-File", ".\tools\install-native-plugin.ps1",
                "-Configuration", $Configuration
            )

            if ($PluginsRoot) {
                $installArgs += @("-PluginsRoot", $PluginsRoot)
            }

            if ($ForceInstall) {
                $installArgs += "-Force"
            }

            powershell @installArgs
        }
}
