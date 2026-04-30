# Markdown++ Native Session Log

Timestamp: 2026-04-24 17:14:24 PDT -0700

Project root: `/home/Vibe-Projects/MarkdownViewerPlusPlus`

## Goal

Modernize MarkdownViewerPlusPlus into a direct native Notepad++ plugin named Markdown++, built side-by-side with the existing project until parity.

## User Decisions

- Language/runtime: C++ native Notepad++ plugin.
- UI renderer: WebView2.
- WebView2 policy: require Evergreen runtime and detect/guide if missing.
- Markdown engine: cmark-gfm.
- Mermaid: keep bundled/offline Mermaid rendering.
- Architecture: x64 only.
- Removed scope: Outlook/email export.
- Migration strategy: side-by-side until parity.
- Visual target: modern GitHub-like default theme, with compatibility CSS option later.

## Current Native Project

New native project files live under:

- `MarkdownPlusPlus.Native/`
- `MarkdownPlusPlus.Native/src/`
- `MarkdownPlusPlus.Native/assets/`

Top-level build/docs/tools:

- `CMakeLists.txt`
- `CMakePresets.json`
- `BUILDING_NATIVE.md`
- `tools/install-native-deps.py`
- `tools/build-native.ps1`

Smoke fixtures:

- `smoke-tests/markdownplusplus-smoke.md`
- `smoke-tests/markdownplusplus-smoke-image.svg`
- User-supplied recording: `smoke-tests/2026-04-24 16-24-55.mkv`

Local dependencies:

- WebView2 SDK: `packages/Microsoft.Web.WebView2.1.0.3912.50/`
- cmark-gfm source: `third_party/cmark-gfm/`
- Mermaid: `MarkdownPlusPlus.Native/assets/mermaid/mermaid.min.js`
- NuGet CLI: `tools/nuget/nuget.exe`

## Build Command

Run from the repo root in the same Windows path style used to create the CMake cache, currently:

```powershell
PS Z:\home\Vibe-Projects\MarkdownViewerPlusPlus> powershell -ExecutionPolicy Bypass -File .\tools\build-native.ps1 -Configuration Release -Package
```

Package output:

```text
build/cmake/vs2026-x64/package/Release/MarkdownPlusPlus/
```

Copy that folder into Notepad++'s plugin folder for manual testing.

Note: Running CMake/MSBuild through `\\wsl.localhost\...` can collide with an existing `Z:\...` CMake cache. Use the same path style as the original configure. The UNC throwaway verification build used:

```text
build/cmake/vs2026-x64-unc/
```

## Current Build Status

Known working environment:

- Visual Studio 2026 v18
- MSVC 19.50 / v145 toolset
- Windows SDK 10.0.26100.0 targeting Windows 10.0.26200
- CMake with `Visual Studio 18 2026` generator

Verification performed:

- `git diff --check` passed after recent edits.
- Linux CMake configure passed with `build/cmake/linux-check`.
- MSVC Release target build passed in throwaway UNC build folder.

Expected warning noise:

- CMake/MSBuild warns about path case and UNC current directory handling, especially when launched through WSL/UNC.
- Earlier `Z:\...` builds also showed CMake path-case warnings but succeeded.

## Implemented Behavior

The plugin currently:

- Registers as `Markdown++` in Notepad++.
- Opens a dockable right-side preview pane.
- Renders Markdown through cmark-gfm with GitHub-flavored extensions:
  - tables
  - strikethrough
  - autolinks
  - tagfilter
  - task lists
  - footnotes
- Uses `CMARK_OPT_SOURCEPOS` so rendered blocks carry `data-sourcepos` for scroll sync.
- Loads WebView2 from the Evergreen runtime.
- Shows a custom native error panel if WebView2 is missing or cannot start.
- Error panel includes the official WebView2 download URL and opens it on click.
- Uses `%LOCALAPPDATA%\MarkdownPlusPlus\WebView2` as the WebView2 user data folder to avoid access denied failures under Program Files.
- Maps assets through `https://markdownplusplus.local/`.
- Maps the current Markdown directory through `https://markdownplusplus.document/` so local images/links work from WebView2.
- Allows external HTTPS images in CSP.
- Bundles Mermaid offline and renders Mermaid fenced code blocks.
- Supports local SVG/image smoke test and external HTTPS image smoke test.

## Recent Fixes

### WebView2 Startup

Problem: WebView2 runtime was installed but preview failed with `0x80070005`.

Fix:

- Added repo-controlled user data folder under `%LOCALAPPDATA%\MarkdownPlusPlus\WebView2`.
- Startup failure message now reports:
  - startup HRESULT
  - version query HRESULT
  - user data folder
  - Microsoft WebView2 URL

### Local Images

Problem: local sidecar images did not load reliably with `NavigateToString`, especially from WSL paths.

Fix:

- Added virtual host mapping for the current Markdown directory:

```text
https://markdownplusplus.document/
```

- The generated document uses that as `<base href>`.

### Scroll Sync

Problem: initial ratio-based scroll sync bounced/stuttered badly.

Fix:

- Subclassed both Scintilla handles to catch viewport messages.
- Added source-line-based sync using Scintilla messages:
  - `SCI_GETFIRSTVISIBLELINE`
  - `SCI_GETLINECOUNT`
  - `SCI_LINESCROLL`
  - `SCI_LINESONSCREEN`
  - `SCI_VISIBLEFROMDOCLINE`
  - `SCI_DOCLINEFROMVISIBLE`
- Added cross-side suppression and throttling:
  - scroll throttle: 50ms
  - cross-scroll suppression: 260ms
  - ratio epsilon: 0.01
- JS side suppresses programmatic scroll notifications and cancels stale scroll requests.

### Mermaid Scroll Jump

Problem: scrolling past Mermaid caused a full-page jump because one tall rendered diagram mapped to only a few source lines.

Fix:

- `preview.js` now interpolates source line position within tall rendered blocks.
- `scrollToSourceLine` computes an offset inside the block instead of snapping only to the block top.

### Text Edit Jump

Problem: typing refreshed the entire WebView, briefly jumping to top before correcting position.

Fix:

- First preview load still uses `NavigateToString`.
- Subsequent text edits update the existing document in-place through `window.MarkdownPlusPlusPreview.replaceContent(...)`.
- Only `.markdown-body` is replaced.
- Mermaid is rerun after replacement.
- Scroll is restored immediately and again after Mermaid layout settles.
- Script string escaping handles `<`, `>`, `&`, quotes, backslashes, CR/LF, and Unicode line separators.

Important files:

- `MarkdownPlusPlus.Native/src/WebViewHost.cpp`
- `MarkdownPlusPlus.Native/src/WebViewHost.h`
- `MarkdownPlusPlus.Native/assets/preview.js`
- `MarkdownPlusPlus.Native/src/MarkdownRenderer.cpp`
- `MarkdownPlusPlus.Native/src/MarkdownRenderer.h`
- `MarkdownPlusPlus.Native/src/PreviewWindow.cpp`
- `MarkdownPlusPlus.Native/src/PreviewWindow.h`
- `MarkdownPlusPlus.Native/src/HtmlUtil.cpp`

### Edit Debounce

Problem: after in-place updates, every keystroke still triggered a render and Mermaid rerender.

Fix:

- Text-change `SCN_MODIFIED` now schedules a one-shot native timer.
- Debounce delay: 140ms.
- Manual refresh, buffer activation, and opening the preview still render immediately.
- Scheduled render is cancelled on shutdown/hide/manual immediate render.

Important files:

- `MarkdownPlusPlus.Native/src/PluginController.cpp`
- `MarkdownPlusPlus.Native/src/PluginController.h`

Key functions:

- `ScheduleRenderCurrentBuffer`
- `CancelScheduledRender`
- `OnRenderDebounceTimer`
- `RenderCurrentBufferNow`

## User-Tested Status

Confirmed by user:

- Plugin loads in Notepad++.
- WebView2 now starts.
- Mermaid renders successfully.
- Local image smoke test works after virtual host mapping.
- External HTTPS image works.
- Scroll sync became much smoother after throttling/interpolation.
- In-place edits work properly.

Current latest change to test:

- 140ms edit debounce.

## Current Repo Status Snapshot

At timestamp, `git status --short` showed:

```text
 M .gitignore
?? AI_UNDERSTANDING.md
?? BUILDING_NATIVE.md
?? CMakeLists.txt
?? CMakePresets.json
?? MarkdownPlusPlus.Native/
?? smoke-tests/
?? third_party/
?? tools/
```

This repo appears to contain a large uncommitted native rewrite. Do not assume untracked means disposable.

## Known Quirks / Risks

- JavaScript syntax was not checked with `node --check` because Node is not installed in WSL here.
- MSVC build was verified through a throwaway UNC build folder, not the user's normal `Z:\...` package command.
- CMake path style matters. A cache created from `Z:\...` rejects configure attempts through `\\wsl.localhost\...`.
- Mermaid rerender can still be expensive for large diagrams, though debounce should reduce churn.
- Options dialog is still a placeholder.
- Compatibility CSS option is planned but not wired yet.
- Plugin install/copy automation is not yet built.
- Existing original managed/shim project is still present and should not be removed until side-by-side parity is proven.

## Next Strongest Moves

1. Rebuild/package from normal Windows PowerShell using the `Z:\home\...` repo path.
2. Test fast typing in `smoke-tests/markdownplusplus-smoke.md`, especially around Mermaid.
3. Add a small visible Mermaid parse-error surface in the preview instead of relying only on console errors.
4. Add options plumbing:
   - theme: modern GitHub-like / compatibility CSS
   - Mermaid enabled/disabled
   - scroll sync enabled/disabled
   - render debounce delay
5. Add a simple install/copy script for the packaged plugin folder.
6. Expand smoke tests for:
   - malformed Mermaid
   - local links
   - relative images in nested folders
   - large Markdown files
   - task lists/tables/footnotes
7. Start parity checklist against the old MarkdownViewerPlusPlus behavior.

## Fast Resume Checklist

When resuming:

1. Read this file first.
2. Run:

```bash
cd /home/Vibe-Projects/MarkdownViewerPlusPlus
git status --short
```

3. If code changed since this log, inspect:

```bash
git diff -- MarkdownPlusPlus.Native/src MarkdownPlusPlus.Native/assets
```

4. Ask the user for latest Notepad++ behavior after rebuilding, especially debounce/scroll behavior.
5. Continue with the next strongest move above.
