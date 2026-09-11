# Backlog — Markdown Viewer PlusPlus

Repo-level backlog: installer, release, packaging, and dependencies. The plugin
*feature* backlog lives in
[`MarkdownPlusPlus.Native/BACKLOG.md`](MarkdownPlusPlus.Native/BACKLOG.md).

## Preview blank-pane investigation (2026-09-11)

A user report: "a markdown file will NOT render on the right, sometimes even closing notepad++
and reopening does NOT fix it, and I have to close the file out of notepad++ and reopen it."

Investigated with an instrumented build (temporary `DebugLog` on every silent return, captured
with DebugView against Notepad++ 8.9.7.0). **The measurements changed the diagnosis twice**, which
is the main reason this section exists: two plausible theories were killed by evidence before any
fix was written.

### What was measured, not assumed

- `NavigationCompleted isSuccess=1 source=about:blank` — after `NavigateToString` the WebView
  really does sit on `about:blank`. So `Reload()` can only ever destroy the rendered document.
- **Zero `WM_ACTIVATE` arrivals** across repeated focus changes and tab switches. Notepad++ does
  not deliver `WM_ACTIVATE` to a `WS_CHILD` docked panel. The handler added in `a2aefaa`
  ("Fix WebView2 blank content on window context switch") **has never executed**, which is why
  that commit did not fix the blank pane — and had it run, it would have caused it.
- The startup race theory is **wrong**: `pendingDocument_` is stored and correctly replayed when
  `ready_` flips (`ApplyPendingDocument SKIPPED ready_=0 ... -> ready_=true -> FULL
  NavigateToString`). A render requested before WebView2 is up is never lost.
- Buffer paths resolved **correctly** on every activation observed, so the stale
  `NPPM_GETFULLCURRENTPATH` theory is unproven here (the code is still wrong in principle — see
  Open).
- `RenderCurrentBufferNow` fired **twice per buffer activation** and three times at startup.

### Fixed

- 🔴 **Dead, destructive `WM_ACTIVATE` -> `Reload()` removed**, along with `WebViewHost::Reload()`
  itself so the footgun cannot be picked up again.
- 🔴 **`replaceContent` blanked the pane and reported success.** `article.innerHTML = html || ""`
  wiped the preview then returned `true`, so the host believed the update had applied and never
  fell back to a full re-navigation. A silent, permanent blank that survived a restart because the
  same empty content was re-applied every time. It now returns `false` on empty input, which makes
  the host re-navigate. **This is the best current explanation for the reported symptom**, and it
  matches the one detail nothing else did: a restart does not help, reopening the file does.
- 🟠 **`ready_` was latched true even when `get_CoreWebView2` failed**, leaving a host that
  believed it was ready with no CoreWebView2, no event handlers, no retry and no error shown.
  Now checked, reported, and not latched.
- 🟠 **A failed navigation latched `documentLoaded_ = true`**, so `Ready()` reported true for a
  blank page and print/PDF would run against it. Now honours `IsSuccess`.
- 🟠 **The message overlay outlived its message.** Created `WS_VISIBLE` over the WebView and only
  destroyed in `Destroy()`, so a benign notice ("print is available after the preview finishes
  loading") covered the preview for the rest of the session. Hidden on a successful navigation.
- 🟡 **Double render per tab switch** — two full cmark passes and two WebView2 updates, measured.
- 🟡 **`Refresh preview` is now a HARD refresh** and has a shortcut, `Ctrl+Alt+R`. It previously
  preferred the in-place JS swap, which is exactly the path that cannot be trusted when a user is
  asking for a refresh. `Ctrl+Shift+R` was rejected as the default because Notepad++ binds it to
  Macro > Start Recording.
- 🟡 **Diagnosability.** `DebugLog` had one call site in the whole plugin, so a blank pane left no
  evidence at all. Failure paths now log, and README gained the repo's first troubleshooting
  section. A healthy session logs nothing, so any `[Markdown++]` line is a finding.

### Open

- 🟠 **`NPPN_BUFFERACTIVATED` ignores `nmhdr.idFrom`.** Notepad++ documents that field as the
  activated buffer id; the plugin instead asks `NPPM_GETFULLCURRENTPATH`, which is not guaranteed
  to have caught up. Not observed misbehaving in testing, but with `autoOpenMarkdown` defaulting
  to true a stale read would hide the panel for a markdown file. Fix needs
  `NPPM_GETFULLPATHFROMBUFFERID` (`NPPMSG + 58`) added to `NppApi.h`.
- 🟠 **Teardown from inside a WebView2 callback.** `ProcessFailed` -> `crashCallback_` ->
  `RestartEnvironment` -> `Destroy()` calls `controller_->Close()` and `CoUninitialize()` while a
  WebView2 event handler is on the stack. Unsupported; defer it through a posted message.
- 🟠 **Creation callbacks capture raw `this` and are not cancelled by `Destroy()`.** After a
  restart a stale callback can overwrite `environment_`/`controller_`/`webView_` and orphan a
  controller whose child HWND is never closed — content loaded, nothing painted. Needs a
  generation counter.
- 🟡 **Markdown detection is filename-only.** `.mdx`, `.qmd`, `.rmd`, extensionless files, and
  anything manually set to the Markdown language never preview. `NPPM_GETBUFFERLANGTYPE`
  (`NPPMSG + 64`) would fix the language half.
- 🟡 **The render path checks `Created()` but not `Ready()`** while print/PDF check `Ready()`.
  Harmless today because the pending queue absorbs it, but inconsistent.
- 🟡 **`Resize()` is only called on controller-creation and `WM_SIZE`.** No render path re-applies
  bounds, so a panel sized 0x0 at the wrong instant stays unpainted.
- ⚪ **This is a fork.** Upstream `nea/MarkdownViewerPlusPlus` very likely carries the dead
  `WM_ACTIVATE` handler and the `innerHTML` blanking too. Worth an upstream issue.
- ⚪ **The repro was never captured live.** Every fix above addresses a defect proven by reading
  or by instrumentation, but the user's exact failure was not reproduced on demand in-session.
  The diagnostics shipped here exist so the next occurrence produces evidence.

### Release state

`v1.2.0` was committed but **never tagged** — blocked by a GitHub Actions outage on 2026-08-06.
Tags stop at `v1.1.0` while `CMakeLists` says 1.2.0. The release is still owed.

## Bugs

- See "Preview blank-pane investigation (2026-09-11)" at the top of this file: several
  real defects were found and fixed, so "none known" was never accurate, only unexamined.
- (historical) The MSI installer and the plugin's core rendering (GitHub-flavored
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
