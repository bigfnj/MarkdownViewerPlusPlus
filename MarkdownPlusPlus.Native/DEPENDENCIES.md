# Native Dependency Setup

This project targets Visual Studio 2026 v18 with the MSVC v145 platform toolset.

## Required Visual Studio Components

Install these through Visual Studio Installer:

- Desktop development with C++
- MSVC v145 build tools
- Windows 10/11 SDK
- NuGet package restore support

These are Visual Studio workloads/components rather than repo-local tools, so they cannot be vendored into the project folder.

## Repo-Local Installs

The following dependencies have been installed into the local repository filesystem:

| Dependency | Version | Local path |
|---|---:|---|
| Microsoft.Web.WebView2 SDK | 1.0.3912.50 | `packages/Microsoft.Web.WebView2.1.0.3912.50` |
| cmark-gfm | 0.29.0.gfm.13 | `third_party/cmark-gfm` |
| Mermaid | 11.14.0 | `MarkdownPlusPlus.Native/assets/mermaid/mermaid.min.js` |
| NuGet CLI | latest at install time | `tools/nuget/nuget.exe` |

The WebView2 package is intentionally under the solution-level `packages` folder because the native `.vcxproj` imports its MSBuild props/targets from that location.

To recreate the repo-local dependency install, run:

```powershell
python .\tools\install-native-deps.py
```

The CMake build is now the preferred native build path. See `BUILDING_NATIVE.md`.

The installer patches vendored `cmark-gfm` from `cmake_minimum_required(VERSION 3.0)` to `3.10` because CMake 4+ / VS2026 rejects or warns on older compatibility declarations.

## Runtime Requirement

The plugin uses the Evergreen Microsoft Edge WebView2 Runtime. It is not bundled. The native preview host detects a missing runtime and shows an install guidance message inside the dockable panel.

## Current Native Integration Status

The Windows/VS2026 native build path, menu-only persisted options, clipboard support, standalone HTML export, PDF export, and print support are implemented and manually accepted for the current practical-parity pass.

Use `SMOKE_CHECKLIST.md` for release-readiness validation after rebuilding and installing the native plugin.
