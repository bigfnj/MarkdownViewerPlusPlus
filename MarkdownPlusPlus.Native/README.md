# Markdown++ Native

Markdown++ is the side-by-side native C++ rewrite of MarkdownViewerPlusPlus for Notepad++.

The native plugin is built with CMake and Visual Studio 2026. See `../BUILDING_NATIVE.md`.

## Current Status

The native rewrite has reached practical parity for the current pass and is manually validated as a Notepad++ x64 plugin.

Confirmed behavior:

- Loads as `Markdown++` in Notepad++.
- Opens a dockable WebView2 preview panel.
- Auto-opens for `.md`, `.markdown`, `.mdown`, `.mkd`, and `.mkdn` files.
- Renders GitHub-flavored Markdown through `cmark-gfm`.
- Renders bundled/offline Mermaid diagrams.
- Shows visible inline Mermaid parse/render errors while keeping other diagrams working.
- Resolves local images and SVGs through a document virtual host.
- Opens local Markdown links in Notepad++.
- Opens external `http://` and `https://` links once in the default browser.
- Handles fragment-only preview links and syncs the Notepad++ editor to the target source heading.
- Keeps editor/preview scroll sync good enough for this pass, including EOF spacer alignment.
- Persists menu-only settings for auto-open, scroll sync, Mermaid rendering, and preview update delay.
- Copies rendered HTML to the clipboard.
- Exports standalone HTML with working fragment jumps.
- Exports standalone PDF without preview-only local Markdown link prompts.
- Opens the WebView2 system print dialog.

## Menu Surface

The plugin intentionally uses direct menu commands rather than the rejected native Options dialog.

Commands:

- `Markdown++`
- `Refresh preview`
- `Copy HTML to clipboard`
- `Export HTML...`
- `Export PDF...`
- `Print...`
- `Open automatically for Markdown files`
- `Synchronize scrolling`
- `Render Mermaid diagrams`
- `Preview update delay`
- `About`

## Runtime Layout

The packaged Notepad++ plugin folder is intentionally small:

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

The WebView2 SDK and `cmark-gfm` are build-time dependencies. The Microsoft Edge WebView2 Evergreen Runtime remains a user-machine runtime requirement and is not bundled.

## Validation

Use `../smoke-tests/markdownplusplus-smoke.md` for broad manual validation and `../smoke-tests/smoke-test.md` for focused link behavior.

The final checklist lives in `SMOKE_CHECKLIST.md`.

## Legacy Project

The original C# MarkdownViewerPlusPlus project remains in the repository. Keep it intact until the native rewrite is ready for a deliberate replacement/release decision.
