# Markdown++ Native Smoke Checklist

Use this after a Windows rebuild/install:

```powershell
cd Z:\home\Vibe-Projects\MarkdownViewerPlusPlus
powershell -ExecutionPolicy Bypass -File .\tools\build-native.ps1 -Configuration Release -Install
```

Close Notepad++ before installing unless using `-ForceInstall`.

## Startup

- Open Notepad++ x64.
- Confirm `Plugins > Markdown++` is present.
- Open `Z:\home\Vibe-Projects\MarkdownViewerPlusPlus\smoke-tests\markdownplusplus-smoke.md`.
- Confirm the preview auto-opens when auto-open is enabled.
- Toggle `Plugins > Markdown++ > Markdown++` and confirm the dockable panel hides/shows.

## Rendering

- Confirm headings, tables, task lists, strikethrough, autolinks, code blocks, blockquotes, inline HTML, and footnotes render.
- Confirm the local SVG image renders.
- Confirm the external HTTPS image renders or fails gracefully if the machine blocks network images.
- Confirm valid Mermaid renders.
- Temporarily test malformed Mermaid if needed; it should show an inline `Mermaid render error` panel without breaking valid diagrams.

## Links

Use `smoke-tests\smoke-test.md`:

- Same-folder Markdown links open/switch in Notepad++.
- Parent-folder Markdown links open/switch in Notepad++.
- External HTTPS links open once in the default browser.
- `Jump to expected behavior` scrolls the preview and the Notepad++ editor to `## Expected Behavior`.

## Scroll Sync

- Scroll the editor and confirm the preview follows.
- Scroll the preview and confirm the editor follows.
- Click/search/jump around the editor and confirm the preview follows the caret.
- Type near headings, images, Mermaid, and the end of the file; confirm preview updates without major jumps.
- Confirm final document lines can align near the editor caret because of the preview EOF spacer.

## Menu Options

- Toggle `Open automatically for Markdown files`; switch between Markdown and non-Markdown tabs and confirm expected show/hide behavior.
- Toggle `Synchronize scrolling`; confirm sync stops and resumes.
- Toggle `Render Mermaid diagrams`; confirm Mermaid rendering turns off/on after refresh.
- Change `Preview update delay` between `0`, `80`, `140`, `250`, and `500 ms`; restart Notepad++ and confirm the checkmark persists.

## Clipboard And Export

- `Copy HTML to clipboard`: paste into an HTML-aware target and confirm rendered content. Paste into a plain-text target and confirm it is rendered HTML, not raw Markdown.
- `Export HTML...`: open the generated file in a browser. Confirm styling, local image resolution, Mermaid rendering if enabled, and fragment-only `Jump to expected behavior`.
- `Export PDF...`: open the generated PDF. Confirm it does not prompt for local `.md` permissions and does not contain `markdownplusplus.document` links for local Markdown files. External HTTPS and internal fragment links may remain clickable.
- `Print...`: confirm the system print dialog opens.

## Package Layout

Confirm the installed plugin folder contains:

```text
Notepad++\plugins\MarkdownPlusPlus\
  MarkdownPlusPlus.dll
  assets\preview.css
  assets\preview.js
  assets\mermaid\mermaid.min.js
```

No separate `cmark-gfm` or WebView2 SDK DLLs should be required beside the plugin DLL.
