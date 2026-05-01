# Markdown++ Native Backlog

This backlog is for the native C++ `Markdown++` plugin.

## Completed Baselines

### Native Release-Candidate Checkpoint

Status: complete.

- Native baseline checkpoint committed as `95ecffe`.
- Release build was rebuilt and installed by the user.
- Manual retest accepted the external-link default-browser behavior.
- Package layout was verified to contain only runtime plugin files:
  - `MarkdownPlusPlus.dll`
  - `assets/preview.css`
  - `assets/preview.js`
  - `assets/mermaid/mermaid.min.js`
  - `assets/mermaid/README.md`

### Native-Only Repository Cleanup

Status: in progress.

The native baseline is frozen. The cleanup pass removes historical source,
obsolete build entry points, stale dependency notices, generated outputs, and
product documentation that no longer describes the native plugin.

Definition of done:

- Root documentation describes only native `Markdown++`.
- Native build from a clean checkout is reproducible with documented commands.
- Release package contains only native runtime files.
- Generated build/dependency/smoke outputs remain ignored.
- `git status --short` shows only intentional source changes after validation.

## Feature Backlog

### P1 - Configurable Rendered File Extensions

Add user-configurable preview file extensions without adding an Options dialog.

Target behavior:

- Persist a comma-separated extension list in `MarkdownPlusPlus.ini`.
- Empty list means render all files.
- Support extensions with or without a leading dot.
- Preserve the native default auto-open list for Markdown files unless the user
  explicitly changes it.
- Support unsaved `new *` Notepad++ buffers behind a menu setting.

Likely files:

- `src/PluginOptions.h`
- `src/PluginOptions.cpp`
- `src/PluginController.h`
- `src/PluginController.cpp`
- `SMOKE_CHECKLIST.md`
- `README.md`

### P1 - Custom CSS / Theme Controls

Add custom styling support while keeping settings menu-driven or file-driven.

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

Add optional export controls.

Target behavior:

- Optional open-after-export for HTML.
- Optional open-after-export for PDF.
- PDF orientation.
- PDF paper size.
- PDF margins.

Notes:

- Native PDF export uses WebView2 print settings where available.
- Keep defaults matching the accepted export behavior.

Likely files:

- `src/PluginOptions.*`
- `src/PluginController.*`
- `src/WebViewHost.*`
- `SMOKE_CHECKLIST.md`
- `README.md`

### P3 - Outlook Email Export

Implement only if there is a real user need.

Target behavior:

- Send current rendered document as plain text.
- Send current rendered document as HTML.
- Detect Outlook availability and fail with a clear message when unavailable.

Notes:

- This adds COM automation and a nontrivial support surface.
- It should stay behind explicit menu commands if implemented.

### P3 - Native UI Polish

Small release-quality improvements.

Candidates:

- Native toolbar/dock icons.
- Preview context commands.
- Find in preview.
- Keyboard focus/accessibility polish.
- Version/package automation.
