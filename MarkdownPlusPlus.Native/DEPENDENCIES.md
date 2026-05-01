# Native Dependency Setup

This project targets Visual Studio 2026 v18 with the MSVC v145 platform toolset.

## Required Visual Studio Components

Install these through Visual Studio Installer:

- Desktop development with C++
- MSVC v145 build tools
- Windows 10/11 SDK
- NuGet package restore support

These are Visual Studio workloads/components rather than repo-local tools, so
they cannot be vendored into the project folder.

## Repo-Local Generated Installs

The dependency installer recreates these generated paths under the repository
root:

| Dependency | Version | Local path |
|---|---:|---|
| Microsoft.Web.WebView2 SDK | 1.0.3912.50 | `packages/Microsoft.Web.WebView2.1.0.3912.50` |
| cmark-gfm | 0.29.0.gfm.13 | `third_party/cmark-gfm` |
| Mermaid | 11.14.0 | `MarkdownPlusPlus.Native/assets/mermaid/mermaid.min.js` |
| NuGet CLI | latest at install time | `tools/nuget/nuget.exe` |

To recreate the repo-local dependency install, run:

```powershell
python .\tools\install-native-deps.py
```

The installer patches generated `cmark-gfm` CMake compatibility metadata from
older minimum versions to `3.10` because CMake 4+ / VS2026 rejects or warns on
older compatibility declarations.

Generated dependency paths are ignored by Git. They are build inputs, not source
files for this repository.

## Runtime Requirement

The plugin uses the Evergreen Microsoft Edge WebView2 Runtime. It is not
bundled. The native preview host detects a missing runtime and shows an install
guidance message inside the dockable panel.

## Notices

- Markdown++ project code is MIT licensed. See `../LICENSE.md`.
- Microsoft.Web.WebView2 SDK and runtime are distributed under Microsoft's
  package/runtime terms. The generated NuGet package includes `LICENSE.txt` and
  `NOTICE.txt`.
- `cmark-gfm` is generated from the pinned GitHub release and includes its
  upstream `COPYING` file in `third_party/cmark-gfm` after dependency install.
- Mermaid is generated from the pinned npm package and is MIT licensed.
- Notepad++ plugin API constants and message contracts are used for native
  plugin integration.

Use `SMOKE_CHECKLIST.md` for release-readiness validation after rebuilding and
installing the native plugin.
