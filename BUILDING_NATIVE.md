# Building Markdown++ Native With CMake

The native rewrite is CMake-first. Visual Studio project files are generated from `CMakeLists.txt`.

## Requirements

- Visual Studio 2026 v18
- MSVC v145 toolset
- Windows 10/11 SDK
- CMake 4.2 or newer for the `Visual Studio 18 2026` generator

Repo-local dependencies are installed with:

```powershell
python .\tools\install-native-deps.py
```

## Configure

One-command build with logs:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-native.ps1 -Configuration Release -Package
```

One-command build, package, and install:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-native.ps1 -Configuration Release -Install
```

Logs are written to:

```text
build/logs/
```

Manual configure:

```powershell
cmake --preset vs2026-x64
```

Equivalent explicit command:

```powershell
cmake -S . -B build\cmake\vs2026-x64 -G "Visual Studio 18 2026" -A x64
```

## Build

```powershell
cmake --build --preset vs2026-release
```

The DLL and copied assets are emitted under:

```text
build/cmake/vs2026-x64/out/Release/
```

Expected runtime output:

```text
out/Release/
  MarkdownPlusPlus.dll
  assets/
    preview.css
    preview.js
    mermaid/
      mermaid.min.js
      README.md
```

## Package

```powershell
cmake --build --preset vs2026-package-release
```

The ready-to-copy Notepad++ plugin folder is emitted under:

```text
build/cmake/vs2026-x64/package/Release/MarkdownPlusPlus/
```

Expected package contents:

```text
MarkdownPlusPlus/
  MarkdownPlusPlus.dll
  assets/
    preview.css
    preview.js
    mermaid/
      mermaid.min.js
      README.md
```

The package is intentionally limited to runtime files. Build-time dependencies such as the WebView2 SDK and `cmark-gfm` source are not copied into the plugin folder.

## Install

Install the packaged plugin folder into the default Notepad++ plugins directory:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\install-native-plugin.ps1 -Configuration Release
```

Or build/package/install in one step:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-native.ps1 -Configuration Release -Install
```

If Notepad++ is installed somewhere nonstandard, pass the plugins root:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-native.ps1 -Configuration Release -Install -PluginsRoot "D:\Apps\Notepad++\plugins"
```

Close Notepad++ before installing so `MarkdownPlusPlus.dll` is not locked. If Notepad++ is installed under `C:\Program Files`, run PowerShell as Administrator if Windows denies the copy.

## Smoke Test

After install, run the final checklist:

```text
MarkdownPlusPlus.Native/SMOKE_CHECKLIST.md
```

Primary manual test files:

```text
smoke-tests/markdownplusplus-smoke.md
smoke-tests/smoke-test.md
```

## Installer (MSI)

The `installer/` folder holds a WiX v5 project that builds an x64 MSI. The MSI
auto-detects the Notepad++ install directory from the registry and installs the
plugin into `<Notepad++>\plugins\MarkdownPlusPlus`.

Prerequisites (one-time):

```powershell
dotnet tool install --global wix --version 5.0.2
```

Build the MSI from a packaged plugin folder:

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\build-installer.ps1
```

By default the script finds the newest
`build\cmake\*\package\Release\MarkdownPlusPlus`, reads the version from
`CMakeLists.txt`, adds the matching WiX Util/UI extensions, and writes
`build\installer\MarkdownPlusPlus-<version>-win-x64.msi`. Override any of these:

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\build-installer.ps1 `
    -PayloadDir "build\cmake\vs2026-x64\package\Release\MarkdownPlusPlus" `
    -Version 1.1.0 `
    -OutDir ".\build\installer"
```

The release workflow builds the same MSI on the Windows runner and attaches it to
the GitHub release alongside the standalone zip.

## Notes

- The CMake target links the generated WebView2 SDK from `packages/Microsoft.Web.WebView2.1.0.4129.50`.
- The CMake target builds and links generated `third_party/cmark-gfm` static libraries.
- The build copies `MarkdownPlusPlus.Native/assets` beside the DLL.
- The runtime WebView2 Evergreen install is still a user-machine runtime requirement, not a repo-local dependency.
- The native plugin is currently x64-only.
