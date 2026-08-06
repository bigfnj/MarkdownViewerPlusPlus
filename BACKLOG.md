# Backlog — Markdown Viewer PlusPlus

Repo-level backlog: installer, release, packaging, and dependencies. The plugin
*feature* backlog lives in
[`MarkdownPlusPlus.Native/BACKLOG.md`](MarkdownPlusPlus.Native/BACKLOG.md).

## Bugs

- None known. The MSI installer and the plugin's core rendering (GitHub-flavored
  Markdown, Mermaid, local SVG) were verified end-to-end on 2026-08-06.

## Features

### Installer / distribution

- **Code-signing.** Only a real **OV/EV code-signing certificate** removes the
  "Unknown Publisher" UAC prompt and SmartScreen warning for end users. An EV cert
  gives instant SmartScreen reputation; an OV cert builds it over downloads/time.
  Self-signing (e.g. the "Serenity Software" cert) only helps on machines that
  already trust that cert and does **not** clear SmartScreen, so do not prompt end
  users to install a self-signed root (a security anti-pattern). Note: signing is
  independent of elevation — the MSI already requires admin because it is a
  per-machine install into `C:\Program Files\Notepad++\plugins`.
- **Optional WebView2 runtime bootstrap.** Detect a missing Edge WebView2 Evergreen
  Runtime and offer to download/install the bootstrapper (turns the MSI into a WiX
  Burn bundle). Convenience only: the plugin already shows in-panel install guidance
  when the runtime is absent, and it is present on nearly all Win10/11 machines.
- **Notepad++ Plugins Admin listing.** Submit to `nppPluginList` for in-app
  discovery and install. Highest-reach distribution channel; complements the MSI.

### Release process

- **Run the full smoke checklist before tagging.** Work through
  [`MarkdownPlusPlus.Native/SMOKE_CHECKLIST.md`](MarkdownPlusPlus.Native/SMOKE_CHECKLIST.md).
  The 2026-08-06 pass verified rendering (Markdown, Mermaid, local SVG) but did not
  re-check link navigation, editor/preview scroll sync, HTML/PDF export, print, or
  clipboard copy.
- **Bump `CMakeLists.txt` VERSION before each tag.** The release workflow fails if
  the tag does not match the project version.

## Deferred

- **x86 (32-bit) build + installer.** Deliberately not shipped. 64-bit Notepad++ is
  the default download and the WebView2 dependency implies modern 64-bit Windows.
  Revisit only on real demand; it roughly doubles the build and test matrix. The
  x64 MSI already blocks install on a 32-bit-only Notepad++ with a clear message.

## Dependencies

- Check pins with `python tools/check-deps.py` (pinned vs. latest upstream).
- As of 2026-08-06: Mermaid **11.16.1**, WebView2 SDK **1.0.4129.50**, cmark-gfm
  **0.29.0.gfm.13** (current — GitHub has not published a newer release).
