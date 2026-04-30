# Markdown++ Native Backlog

This backlog is for the native C++ `Markdown++` plugin only. The goal is to
finish the remaining useful parity items, then make the repository native-only
with no legacy C# code or release artifacts.

## Feature Backlog

### P0 - Release Candidate Checkpoint

Capture the accepted practical-parity native build before more feature work.

Deliverables:

- Rebuild/package native `0.1.0.0`.
- Run `SMOKE_CHECKLIST.md`.
- Verify package contents contain only:
  - `MarkdownPlusPlus.dll`
  - `assets/preview.css`
  - `assets/preview.js`
  - `assets/mermaid/mermaid.min.js`
  - `assets/mermaid/README.md`
- Record any manual smoke failures before cleanup starts.

### P1 - Configurable Rendered File Extensions

Close the legacy General-tab parity gap without reintroducing the rejected
Options dialog.

Target behavior:

- Persist a comma-separated extension list in `MarkdownPlusPlus.ini`.
- Empty list means render all files.
- Support extensions with or without a leading dot.
- Preserve the native default auto-open list for Markdown files unless the user
  explicitly changes it.
- Support unsaved `new *` Notepad++ buffers behind a menu setting equivalent to
  the legacy `inclNewFiles`.

Likely files:

- `src/PluginOptions.h`
- `src/PluginOptions.cpp`
- `src/PluginController.h`
- `src/PluginController.cpp`
- `SMOKE_CHECKLIST.md`
- `README.md`

### P1 - Custom CSS / Theme Controls

Close the legacy HTML-tab custom CSS parity gap while keeping settings
menu-driven or file-driven.

Target behavior:

- Load optional user CSS from a documented config file or INI value.
- Apply user CSS to live preview and standalone HTML export.
- Keep built-in `preview.css` as the base style.
- Preserve dark-mode behavior unless user CSS overrides it.

Open decision:

- Prefer a config-side `custom.css` file over an in-menu text editor. This keeps
  native UI simple and avoids reviving the rejected Options dialog.

Likely files:

- `src/PluginOptions.*`
- `src/MarkdownRenderer.*`
- `src/WebViewHost.*`
- `assets/preview.css`
- `SMOKE_CHECKLIST.md`
- `README.md`

### P2 - Export Preferences

Close the legacy HTML/PDF option parity gaps.

Target behavior:

- Optional open-after-export for HTML.
- Optional open-after-export for PDF.
- PDF orientation.
- PDF paper size.
- PDF margins.

Notes:

- Native PDF export uses WebView2, not PDFSharp, so map legacy behavior to
  WebView2 print settings where available.
- Keep defaults matching the current accepted export behavior.

Likely files:

- `src/PluginOptions.*`
- `src/PluginController.*`
- `src/WebViewHost.*`
- `SMOKE_CHECKLIST.md`
- `README.md`

### P3 - Outlook Email Export

Legacy parity item. Implement only if there is a real user need.

Target behavior:

- Send current rendered document as plain text.
- Send current rendered document as HTML.
- Detect Outlook availability and fail with a clear message when unavailable.

Notes:

- This adds COM automation and a nontrivial support surface.
- It should stay behind explicit menu commands if implemented.

### P3 - Native UI Polish

Small parity and release-quality improvements.

Candidates:

- Native toolbar/dock icons.
- Preview context commands.
- Find in preview.
- Keyboard focus/accessibility polish.
- Version/package automation.

These are not blockers for removing legacy code.

## Legacy Cleanup Plan

The cleanup target is a native-only repository. The old C# plugin should not be
carried as source, build configuration, generated output, or documentation.

### Phase 0 - Freeze Native Baseline

Do this before deleting anything.

- Confirm the native package builds and passes smoke testing.
- Commit or otherwise checkpoint the accepted native baseline.
- Make sure dependency setup can recreate WebView2 SDK, cmark-gfm, Mermaid, and
  NuGet CLI from scripts.

### Phase 1 - Remove Legacy Source and Build Entry Points

Delete legacy C# plugin code and .NET build files:

- `MarkdownViewerPlusPlus/`
- `MarkdownViewerPlusPlus.sln`
- `appveyor.yml`
- `_config.yml`

Remove old Visual Studio/.NET/ILMerge/AppVeyor assumptions from docs.

### Phase 2 - Replace Root Documentation

Rewrite the root `README.md` as native-only documentation.

Remove or replace:

- legacy MarkdownViewerPlusPlus badges
- legacy release links for `0.8.2`
- .NET Framework compatibility notes
- Plugin Manager instructions for the old plugin
- screenshots of the old WinForms UI/options dialog
- legacy options-dialog documentation
- legacy dependency credits that no longer apply

Keep or add:

- native build/install commands
- WebView2 runtime requirement
- package layout
- current native feature list
- manual smoke-test entry points
- native dependency/license notes

### Phase 3 - Dependency and License Audit

Remove license files that apply only to legacy dependencies after confirming no
native file still depends on them:

- `license/HtmlRenderer-License.md`
- `license/Markdig-License.md`
- `license/Ms-PL-License.md`
- `license/PDFSharp-License.md`
- legacy PluginPack.NET/Apache references if no native code uses them

Keep or add native dependency notices for:

- WebView2 SDK/runtime
- cmark-gfm
- Mermaid
- Notepad++ plugin API usage

### Phase 4 - Clean Generated and Local Files

Do not track generated outputs or local caches.

Candidates to remove from Git and ignore:

- `build/`
- `packages/`
- `.download-cache/`
- `tools/nuget/nuget.exe`
- `tools/__pycache__/`
- `third_party/cmark-gfm/` if it remains installer-generated
- generated smoke outputs such as `smoke-tests/*.html`, `smoke-tests/*.pdf`,
  copied smoke files, and `*:Zone.Identifier`

Decision needed:

- Either track `third_party/cmark-gfm` as vendored source, or treat it as a
  generated repo-local dependency installed by `tools/install-native-deps.py`.
  The current setup script supports the generated-dependency model.

### Phase 5 - Native-Only Verification

After removal:

- Run dependency install from a clean checkout.
- Configure CMake.
- Build/package Release.
- Install into Notepad++.
- Run `SMOKE_CHECKLIST.md`.
- Run `git status --short` and confirm only intentional source changes remain.

## Cleanup Definition of Done

- No `MarkdownViewerPlusPlus/` C# project remains.
- No old `.sln`, AppVeyor, Jekyll, ILMerge, DllExport, Markdig, HtmlRenderer,
  PDFSharp, Svg.NET, or WinForms artifacts remain.
- Root docs describe only native `Markdown++`.
- Native build from clean checkout is reproducible using documented commands.
- Release package contains only native runtime files.
- `git status --short` is clean after build artifacts are ignored or removed.
