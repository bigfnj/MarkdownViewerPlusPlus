# Markdown++

Markdown++ is a native x64 Notepad++ plugin for live Markdown preview.

It renders Markdown in a dockable WebView2 panel, uses `cmark-gfm` for
GitHub-flavored Markdown, and bundles Mermaid support for offline diagrams.

## Features

- Dockable Notepad++ preview panel.
- Auto-open for common Markdown extensions.
- GitHub-flavored Markdown rendering through `cmark-gfm`.
- Bundled Mermaid diagram rendering with visible inline errors.
- Local image and SVG resolution through a document virtual host.
- Local Markdown links open or switch files in Notepad++.
- External links open through the default browser path.
- Fragment links scroll both the preview and the editor.
- Source-line-based editor/preview scroll sync.
- Menu-only persisted settings for auto-open, scroll sync, Mermaid rendering,
  and preview update delay.
- Copy rendered HTML to the clipboard.
- Export standalone HTML.
- Export standalone PDF.
- Open the WebView2 system print dialog.

## Requirements

- Windows.
- Notepad++ x64.
- Microsoft Edge WebView2 Evergreen Runtime.
- Visual Studio 2026 v18 with MSVC v145 and a Windows 10/11 SDK for building.
- CMake 4.2 or newer for the `Visual Studio 18 2026` generator.

The WebView2 Evergreen Runtime is a user-machine runtime dependency. The plugin
detects a missing runtime and shows install guidance in the preview panel.

## Build

Install repo-local build dependencies:

```powershell
python .\tools\install-native-deps.py
```

Build and package Release:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-native.ps1 -Configuration Release -Package
```

Build, package, and install:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-native.ps1 -Configuration Release -Install
```

Close Notepad++ before installing so `MarkdownPlusPlus.dll` is not locked. If
Notepad++ is installed under `C:\Program Files`, run PowerShell as
Administrator if Windows denies the copy.

More details are in [BUILDING_NATIVE.md](BUILDING_NATIVE.md).

## Package Layout

The ready-to-copy plugin folder is emitted under:

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

The package contains runtime files only. Build dependencies such as WebView2 SDK
files, NuGet CLI, and `cmark-gfm` source are recreated locally by
`tools/install-native-deps.py` and are not part of the plugin package.

## Smoke Test

After install, run:

```text
MarkdownPlusPlus.Native/SMOKE_CHECKLIST.md
```

Primary manual fixtures:

```text
smoke-tests/markdownplusplus-smoke.md
smoke-tests/smoke-test.md
```

## Repository Layout

```text
MarkdownPlusPlus.Native/     Native plugin source, assets, and native docs
smoke-tests/                 Manual smoke-test Markdown fixtures
tools/                       Dependency, build, and install helpers
BUILDING_NATIVE.md           Full native build instructions
CMakeLists.txt               Native CMake entry point
CMakePresets.json            Visual Studio 2026 x64 presets
```

## Dependencies And Notices

Markdown++ is released under the MIT license in [LICENSE.md](LICENSE.md).

Native dependency setup and third-party notice guidance are documented in
[MarkdownPlusPlus.Native/DEPENDENCIES.md](MarkdownPlusPlus.Native/DEPENDENCIES.md).
