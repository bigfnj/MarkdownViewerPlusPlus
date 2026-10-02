# Backlog — Markdown Viewer PlusPlus

Repo-level backlog: installer, release, packaging, and dependencies. The plugin
*feature* backlog lives in
[`MarkdownPlusPlus.Native/BACKLOG.md`](MarkdownPlusPlus.Native/BACKLOG.md).

## Preview copy loses formatting on paste (2026-10-02) — fixed for 1.4.0

Reported as: copying from the preview pane and pasting "loses all of the formatting, everything
is clumped together, I lose the bold and italics". The standing workaround was to export HTML,
open it, and copy from there.

**Two plausible diagnoses were killed by measurement before anything was written.** Neither the
missing-HTML-flavour theory nor the Notepad++-steals-Ctrl+C theory survived. A driven Notepad++
plus a real Word paste (`tools/measure-preview-copy.ps1`) showed the pane copy already produced
a well-formed `HTML Format` payload with `<strong>` and `<em>` intact, and that Ctrl+C reaches
the WebView perfectly well. Right-click Copy failing too had already ruled out the accelerator.

### What it actually was

Chromium's default copy inlines the **computed** style of every selected node, so the preview's
dark theme rode along. Measured: a pane copy pasted into Word as `colorBGR=15986150`, which is
`rgb(230, 237, 243)` — near-white text on a white page. Bold and italic were present the whole
time and invisible. Targets that strip inline styles (Teams, Confluence) fell back to the
plain-text flavour, where Chromium joins a heading to the next block with a single newline, which
is the "clumped together" half of the report.

**The defect has two shapes and that nearly hid it.** Depending on the selection, Chromium either
inlines the colour per element (Word honours it, text invisible) or hoists it onto a wrapper span
(Word discards the wrapper, paste looks fine). Two consecutive runs disagreed for exactly this
reason. A check that only looks at what Word ended up with can therefore pass while the defect is
live — confirmed by mutation, see below.

### Fixed

- 🔴 **`preview.js` now owns the copy.** A `copy` handler serialises the selection from the
  preview DOM, which carries no inline styles because all styling lives in `preview.css`. The
  paste target applies its own colours. Only structural styles are added back (table borders,
  code and blockquote framing) and none of them set a foreground colour. Partial selections
  inside `<strong>`, `<em>`, `<code>` and friends are re-wrapped in their inline ancestors, since
  `cloneContents()` on such a range returns a bare text node and would otherwise have dropped the
  emphasis — a regression against the very thing being fixed.
- 🟠 **Plain-text flavour is now readable.** Blocks are separated by a blank line, list items get
  markers, table rows are tab-separated, and a Mermaid block emits its **source** rather than the
  rendered SVG's node labels run together.
- 🟠 **Page `<script>` content no longer leaks into a copy.** `Ctrl+A` selects the whole body, so
  the first version of the text walker emitted
  `window.MarkdownPlusPlusOptions={mermaidEnabled:true};` at the end of every full-document copy.
  Caught by measurement, not by review.
- 🟡 **The text walker descended nothing.** A whole-document selection clones a single
  `<article>`, so iterating top-level children collapsed the entire document onto one line. The
  first fix changed nothing user-visible and the measurement said so.

### Verified

`tools/measure-preview-copy.ps1`, 12 checks, all passing against the installed 1.4.0 build:
theme colour absent from the payload, emphasis present as markup, no script leak, blocks
separated, Mermaid source present, and Word reporting `bold=-1`, `italic=-1`, one real table and
automatic text colour.

**Mutation-tested.** Disabling the copy handler in the *installed* asset turned the gate red on 3
of 12 checks, naming the theme colour first. One check, "pasted text uses the target document
colour", **passed while the defect was live** because that selection let Chromium hoist the colour
onto a wrapper Word then dropped. It is kept as corroboration with a comment saying exactly that;
the payload check is the one that fails every time.

### Open

- 🟡 **Diagrams do not paste as pictures.** Deliberate, decided 2026-10-02. Word's HTML paste
  accepts a PNG data URI (produces a real `wdInlineShapePicture`) and rejects an SVG one
  (produces nothing), measured directly with a synthetic clipboard payload. The copy therefore
  emits the diagram SVG as an `<img>`, which SVG-capable targets draw and Word does not. Making
  it work everywhere means rasterising each rendered diagram to PNG and caching it on the element,
  because the `copy` event is synchronous and canvas work is not. That adds work to a render path
  with a history of blank-pane bugs and no test coverage, so it was deferred rather than bundled
  into this release. Prior behaviour was worse: Word made three broken 15x11pt shapes and leaked
  the node labels as stray text.
- ⚪ Only **Word** was measured on the paste side. Outlook, Teams, OneNote and Confluence were
  named in the report but not instrumented. The payload is standard semantic HTML, so they should
  behave, but that is reasoning rather than evidence.

## A bumped dependency pin did nothing locally (2026-10-02)

`MARKDOWNPP_WEBVIEW2_VERSION` was a `CACHE STRING`, and `set(... CACHE ...)` never overwrites an
existing cache entry. So the 1.0.4129.50 -> 1.0.4191.47 bump that shipped in **v1.3.0 was ignored
by every configure into an existing build directory**, which kept compiling against 1.0.4129.50
and said nothing. Found by reading `CMakeCache.txt` after the v1.3.0 pull, where the cached value
was still the pre-pull one.

CI configures from scratch on every run, so CI matched the file and the divergence was invisible
from both sides. `MarkdownPlusPlus.Native/CMakeLists.txt` now holds the pin in a plain variable
and fails configure when the build directory disagrees, with `-DMARKDOWNPP_ALLOW_WEBVIEW2_PIN_MISMATCH=ON`
as the deliberate escape. Mutation-tested against the genuinely stale directory that revealed it:
one error, naming the file, printing both versions and both roots.

## Post-fix audit (2026-09-11) — including a regression in the fix itself

A full read-only audit run after `52dc53d` landed, told to skip everything already fixed or
recorded. **Its single most valuable finding was a regression introduced by `52dc53d` itself**,
which is the argument for auditing your own work rather than only the code you inherited.

### Fixed

- 🔴 **REGRESSION from `52dc53d`: the message overlay was muted, not just unstuck.** `HideMessage()`
  called `ShowWindow(SW_HIDE)`, but `ShowMessage()` never called `SW_SHOW` — the overlay had only
  ever been visible because it was created `WS_VISIBLE`. So after the first successful navigation
  hid it, every later error wrote text into a hidden window: "print requires a newer WebView2
  Runtime", "could not open the print dialog", "could not start PDF export", "could not attach to
  the WebView2 preview" — all silently swallowed, and README's troubleshooting table promised a
  dark-grey panel that could no longer appear twice in one session. Now shows and raises the
  overlay explicitly.
- 🔴 **`PathToFileUri` returned `""` for every input, so HTML export was quietly broken.** Its size
  probe passed a NULL buffer to `UrlCreateFromPathW` and demanded `E_POINTER` back; that call
  answers `E_INVALIDARG` to a NULL buffer, so the early return always fired. Every caller in
  `BuildStandaloneDocument` is behind an `if (!uri.empty())`, so an exported HTML file silently
  shipped with **no `preview.js`, no `mermaid.min.js` and no `<base href>`** — no diagrams, no
  anchors, and every relative image broken, with no error shown. Now probes with a real buffer and
  grows only on request. Three tests that pinned this as a known bug have been promoted to real
  regression tests.
- 🟠 **`NavigateToString`'s HRESULT was discarded and its documented 2 MB limit unchecked.** Past
  the ceiling the call fails, `documentLoaded_` is already false, and every later render retakes
  the same failing path: a permanently blank pane with no message. `CMARK_OPT_SOURCEPOS` puts a
  `data-sourcepos` attribute on every block, so the HTML runs well above the markdown size and the
  limit arrives sooner than you would guess. Now size-checked, HRESULT-checked, and reported.
- 🟠 **`ExecuteScript`'s synchronous HRESULT was discarded and the function returned `true`
  regardless.** If the script cannot be queued the completion lambda never runs, so the
  `documentLoaded_ = false` recovery never fires — while the caller has been told the update
  applied. The same failure shape as the `replaceContent` bug, one layer up.
- 🟠 **`SetVirtualHostNameToFolderMapping`'s HRESULT was discarded.** If it fails, `preview.css`,
  `preview.js` and `mermaid.min.js` all 404 under the page's `default-src 'none'` CSP and the
  preview renders as unstyled text with no scroll sync, links or diagrams — with nothing saying why.
- 🟠 **Print and PDF failures were completely invisible**: both return `bool` and both results were
  dropped, so a failure produced no dialog, no message and no log.
- 🟠 **Uninitialised `COREWEBVIEW2_PROCESS_FAILED_KIND`** read into the one diagnostic that exists
  for a crashed WebView2.

### Added: the repo's first automated tests

`MarkdownPlusPlus.Native/tests/` — a dependency-free harness, **45 tests / 209 assertions in 6
CTest targets, all green**, over the genuinely pure units (`MarkdownRenderer`, `PluginOptionsStore`, `WinUtil`,
`HtmlUtil`). Two properties worth preserving:

- `MDPP_KNOWN_BUG_TEST` pins a bug with its reason and **fails when every assertion starts
  passing**, i.e. it alarms on unexpected success and tells you to promote it. That is how the
  `PathToFileUri` fix was confirmed rather than assumed.
- The suite refuses to look clean while degraded: a filter matching nothing is an error, and the
  known-bug count is printed on every run. `--known-bugs` selecting zero is the one deliberate
  exception, since that is the end state the mechanism exists to reach.

**Mutation-tested, 45 mutations with 43 detected.** The two survivors are reported in
`tests/README.md` rather than papered over, and one earlier "survivor" turned out to be a false
negative in the mutation driver itself — MSBuild skipped a recompile for a header edited
milliseconds after the previous build, so the mutant was never compiled. Re-run with
`--clean-first` it was detected. Only SURVIVED verdicts were ever at risk from that; a detected
verdict cannot be fabricated by a stale build.

**It cannot catch a render-pipeline regression.** `PluginController` and `WebViewHost` need a live
Notepad++ and WebView2 and are untested; `tests/README.md` says so explicitly.

### Open, from the audit

- 🟡 **`DebugLog` coverage is still thin** — three call sites, all in `WebViewHost.cpp`. Unlogged:
  both `ApplyPending*` early returns, an empty `ReadCurrentBufferUtf8`, a failed `WriteUtf8File`.
  README's "every message marks a failure path" is true; the converse is not yet.
- ~~🟡 **Version drift**: About says 1.1.0 and the `.rc` says `1,1,0,0` while `CMakeLists` says
  1.2.0.~~ **FIXED 2026-09-11 for the 1.2.1 release.** Both now read a generated
  `Version.h` (`src/Version.h.in` -> `configure_file`), so the only place a version is written is
  `project(... VERSION x.y.z)`. Drift is now impossible rather than currently-absent, which
  matters because the release workflow only ever compared the tag against CMakeLists and would
  never have caught it. Verified on the built DLL: `FileVersion 1.2.1.0`,
  `ProductVersion 1.2.1.0`, About reads "Version 1.2.1".
- 🟡 **Per-render waste on the typing path.** The rendered body is copied ~7 times per render, and
  on the steady-state in-place path the entire `document` string is built and then never read —
  roughly 1.5 MB of pointless memcpy per keystroke on a 100 KB file. `GetNotepadString` also
  zero-fills 64 KB per call, twice per render. The `ExecuteScript` reserve under-counts because
  escaping roughly doubles the HTML.
- 🟡 **`CombinePath` returns an EMPTY string on overflow, not a truncated one.** Corrected from
  the audit's "truncates silently" after it was measured against the live API: the buffer is
  `MAX_PATH`, and when `left + "\" + right` would exceed it `PathAppendW` fails and *clears* the
  buffer. A 255-character left operand already yields `""`. That is the worse failure of the two,
  because it blanks the asset root rather than bending it, and the result is discarded unchecked.
  On the asset-root, config and link-resolution paths. Latent only because the plugins directory
  is short. No test pins it, deliberately — pinning current behaviour would freeze a bug rather
  than a contract. `CanonicalizeWindowsPath` separately uses `PathCanonicalizeW`, which MSDN
  explicitly recommends against, with an input not bounded to `MAX_PATH`.
- 🟡 **`EscapeScriptString` does not escape `"`.** Safe at the only call site today
  (`WebViewHost.cpp` builds single-quoted JS literals) and the tests pin that single-quote
  contract, but it is a trap for anyone who switches quote style.
- ⚪ **Two branches inside the tested units have no reachable input**, recorded in
  `tests/README.md` rather than covered by a test that could not fail: `PathToFileUri`'s
  `FAILED(result)` return (`UrlCreateFromPathW` succeeded on every input tried, including UNC,
  device paths, control characters and a 6000-character path) and the specific "delete the guard"
  mutation of `PluginOptionsStore::Load`'s empty-path check.
- 🟡 **`UrlDecode` mis-decodes percent-encoded UTF-8** (`UrlUnescapeW` yields one `wchar_t` per
  `%XX`), so a local link written by any tool that encodes non-ASCII filenames never opens.
- 🟡 **`preview.js` heading ids**: `slugifyHeading` uses ASCII-only `\w`, so no heading in a
  non-English document ever gets an id and no intra-document anchor works there;
  `assignHeadingIds` can also mint a duplicate id when a heading already has one.
- 🟡 **The in-place path is taken for empty renders**, so a whitespace-only file does a full
  page reload on every debounce tick.
- 🟡 **CSP allows bare `img-src https:`**, so opening an untrusted markdown file fetches remote
  images and leaks the reader's IP. Every other directive there is tight.
- 🟡 **Five `EventRegistrationToken` members are written and never read**; `Destroy()` makes no
  `remove_*` calls. Safe only because `Close()` runs first.
- 🟡 **Two window classes are registered and never unregistered**, so on DLL unload they point at
  unmapped memory.
- ⚪ **Dead code**: `SetScrollRatio` (whole two-layer chain, no callers), `BuildDocument` (only
  caller is a test asserting it equals `BuildPreview().document`), `PreviewScrollFallbackRatio`'s
  true branch (unreachable), `MessageProc`'s identical if/else, `StartPrintToPdf`'s discarded
  return, `CopyHtmlToClipboard`'s `plainText` parameter (always the same value as `articleHtml`),
  `SC_UPDATE_CONTENT`, the `std::string` `DebugLog` overload, `<cstdlib>`, and three `preview.js`
  exports the host never calls.
- ⚪ **Confirmed CLEAN** and worth not re-auditing: COM out-param ownership on every path;
  `SetTimer`/`KillTimer`, subclass, and `CoInitializeEx` pairing; the clipboard `GlobalAlloc`
  path; the JSON mini-parsers; `Utf8ToWide`/`WideToUtf8` including embedded NULs and surrogates;
  cmark-gfm node/buffer lifecycle; scroll-sync suppression (no stuck state possible). Notably
  **`ReadCurrentBufferUtf8` is correct** under both Scintilla 4 and 5 semantics — it was a prime
  suspect for the blank preview and is not the cause.

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

`v1.2.0` was committed but **never tagged** — blocked by a GitHub Actions outage on 2026-08-06,
leaving tags at `v1.1.0` while `CMakeLists` said 1.2.0. Rather than cut a stale 1.2.0, the
2026-09-11 fixes were folded in and released as **`v1.2.1`**, which supersedes it. 1.2.0 is
deliberately skipped as a tag; its content is included.

Worth knowing for the next release: the `Verify tag matches CMakeLists.txt VERSION` step in
`release.yml` is the only automated version gate, and it compares the tag against CMakeLists
**only**. That is why the `.rc` and the About box could sit two minor versions behind without
anything failing. They are generated now, so that gap is closed at the source rather than by
adding another check.

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

- Check pins with `python tools/check-deps.py` (pinned vs. latest upstream). Run it with
  `-X utf8` on Windows or it dies on the arrow glyph it prints.
- As of 2026-10-02: Mermaid **12.1.0**, WebView2 SDK **1.0.4258.31**, cmark-gfm
  **0.29.0.gfm.13** (current — GitHub has not published a newer release).
- **Mermaid 12 is render-verified.** v1.3.0 moved Mermaid across a major version (11.16.1 ->
  12.0.0) with no code change and no diagram ever rendered from the result. Checked on
  2026-10-02 at 12.1.0: the bundle still assigns `globalThis["mermaid"]`, still exports
  `initialize` / `parse` / `run` / `render`, and a `graph TD` fixture renders to a real SVG in
  the pane. Bumping Mermaid needs a rendered diagram, not a successful build.
- Bumping the WebView2 pin does nothing in an existing build directory on its own. See
  "A bumped dependency pin did nothing locally" above.
