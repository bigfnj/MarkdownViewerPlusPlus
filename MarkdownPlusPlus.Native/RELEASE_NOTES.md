# Markdown++ Native Release Notes

## Native test build 0.1.0.0

This is the first practical-parity native test build for the side-by-side `Markdown++` Notepad++ plugin.

### Included

- Native x64 C++ Notepad++ plugin exports.
- Dockable WebView2 preview panel.
- `cmark-gfm` GitHub-flavored Markdown rendering.
- Bundled/offline Mermaid rendering.
- Visible inline Mermaid errors for malformed diagrams.
- Local document virtual-host mapping for images and links.
- Local Markdown link clicks open in Notepad++.
- External web links open in the default browser.
- Fragment-only anchor links scroll both preview and editor.
- Source-line-based editor/preview scroll sync.
- EOF spacer for end-of-document alignment.
- Menu-only persisted settings:
  - auto-open Markdown files
  - synchronize scrolling
  - render Mermaid diagrams
  - preview update delay
- Copy rendered HTML to clipboard.
- Standalone HTML export.
- Standalone PDF export.
- WebView2 system print dialog.

### Runtime Requirement

The Microsoft Edge WebView2 Evergreen Runtime must be installed on the user machine. The plugin detects a missing runtime and shows install guidance in the preview panel.

### Package Contents

Expected release/install folder:

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

### Known Deliberate Choices

- The legacy C# MarkdownViewerPlusPlus project remains in the repository.
- The native Options dialog was rejected during testing and should not be reintroduced for this pass.
- Native settings are direct plugin-menu commands.
- The native build is x64-only for now.
