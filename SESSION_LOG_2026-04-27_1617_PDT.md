# Markdown++ Native Session Log

Timestamp: 2026-04-27 16:17:05 PDT -0700

Project root: `/home/Vibe-Projects/MarkdownViewerPlusPlus`

Previous session log: `SESSION_LOG_2026-04-24_1714_PDT.md`

## Session Standing Instructions

- Keep this session log updated as work progresses.
- Keep `AI_UNDERSTANDING.md` updated after each user prompt with the latest durable context.
- After each assistant response, recommend the next strongest move.
- Treat untracked native rewrite files as important project work, not disposable scratch.
- Keep the original C# MarkdownViewerPlusPlus project until native Markdown++ reaches practical parity.

## Current Goal

Continue the side-by-side native rewrite of MarkdownViewerPlusPlus into a C++ x64 Notepad++ plugin named Markdown++.

The immediate checkpoint is to rebuild/package the current native plugin and have the user test the latest edit debounce behavior in Notepad++.

## Current Status

Carried forward from `SESSION_LOG_2026-04-24_1714_PDT.md`:

- Native plugin registers as `Markdown++`.
- Dockable right-side preview pane exists.
- Preview uses WebView2 Evergreen runtime.
- Markdown rendering uses `cmark-gfm`.
- GitHub-flavored extensions are enabled.
- Mermaid is bundled/offline through `MarkdownPlusPlus.Native/assets/mermaid/mermaid.min.js`.
- Preview assets are served through `https://markdownplusplus.local/`.
- Current Markdown file directory is mapped through `https://markdownplusplus.document/` so local images and links work.
- WebView2 user data is stored at `%LOCALAPPDATA%\MarkdownPlusPlus\WebView2`.
- Text edits update the existing document in place after first load.
- Scroll sync uses source-line-based native/JS coordination, throttling, and Mermaid block interpolation.
- User confirmed the 140ms edit debounce works properly.
- New issue reported: jumping around in the Notepad++ document did not sync the preview unless the user scrolled.

## Current Implementation Update

### Caret-Aware Editor Sync

Problem: Scroll sync followed the editor's first visible line, but cursor/search/click jumps could change the active location without producing the same scroll-path behavior. The preview could remain at the old position until the user performed an actual scroll.

Fix:

- Added Scintilla constants for:
  - `SCI_GETCURRENTPOS`
  - `SCI_LINEFROMPOSITION`
- Added `CurrentEditorCaretSourceLine()`.
- Editor-to-preview sync now tracks both:
  - viewport source line / scroll ratio
  - caret source line
- If the viewport changed, sync remains anchored to the first visible editor line.
- If only the caret changed, sync uses the caret source line so Notepad++ jumps can move the preview.
- Added `WM_LBUTTONUP` to the Scintilla subclass trigger list so mouse clicks in the editor can also prompt sync evaluation.

Important files:

- `MarkdownPlusPlus.Native/src/NppApi.h`
- `MarkdownPlusPlus.Native/src/PluginController.h`
- `MarkdownPlusPlus.Native/src/PluginController.cpp`

### Navigation False Refresh Fix

Problem: User tested the first caret-sync attempt and reported that clicking anywhere in the Markdown document made the viewer do a full refresh, but it still did not move.

Fix:

- Tightened text-change detection so `SCN_MODIFIED` only schedules a render for `SC_MOD_INSERTTEXT` or `SC_MOD_DELETETEXT`.
- Removed the overly broad `modificationType == 0` fallback that could treat navigation/click notifications as edits.
- Added `SC_UPDATE_SELECTION`, `SC_UPDATE_CONTENT`, and `SC_UPDATE_H_SCROLL` constants.
- `SCN_UPDATEUI` now prefers the caret source line when Scintilla reports a selection/caret update.
- Mouse-up and key-up subclass notifications also prefer the caret source line.
- Corrected the x64 `SCNotification` ABI layout by using pointer-sized `INT_PTR` fields for Scintilla position values:
  - `position`
  - `length`
  - `linesAdded`
  - `line`
  - `annotationLinesAdded`
- Added the trailing `characterSource` field to match the modern Scintilla notification shape.

### Edit Render Caret Anchor

Problem: User confirmed navigation sync works, then asked whether the Markdown preview can move to the current editing position in the Markdown source file.

Fix:

- Added a `pendingRenderUseCaretAnchor_` flag for debounced edit renders.
- Changed `RenderCurrentBufferNow(...)` to accept whether the render should anchor to the caret source line or the editor viewport source line.
- Debounced text edits now render/restore preview position using `CurrentEditorCaretSourceLine()`.
- Manual refresh, opening the preview, and buffer activation still render/restore using `CurrentEditorSourceLine()` so normal viewport-preserving behavior remains intact.

Important files:

- `MarkdownPlusPlus.Native/src/PluginController.h`
- `MarkdownPlusPlus.Native/src/PluginController.cpp`

### Viewport Anchor Alignment

Problem: User tested a larger smoke document with additional inline images and Mermaid blocks. The preview found the correct source block, but placed it at the top of the Markdown preview. In the attached screenshot, the caret/editor position was at `## Local Links And Images` around line 246 and lower in the Notepad++ viewport, while the same heading was at the top of the WebView preview.

Fix:

- Added an editor caret viewport anchor ratio.
- `CurrentEditorCaretViewportRatio()` maps the caret's Scintilla visible line to a `[0.0, 1.0]` viewport-relative position.
- `PreviewWindow::SetDocument`, `WebViewHost::NavigateToString`, and `SetScrollSourceLine` now carry an anchor ratio in addition to source line and fallback scroll ratio.
- `preview.js::scrollToSourceLine(...)` now positions the target Markdown block at the same relative vertical viewport anchor instead of always near the top.
- Preview scroll corrections smaller than 2px are skipped to reduce jitter.
- Image load/error events re-apply the current scroll request so late-loading local/external images do not leave source mapping stale.
- During a dirty/debounced edit window, `SCN_UPDATEUI` no longer sends a separate pre-render scroll sync; the debounced render performs the anchored restore once.

### Click Sync Top-Anchor Correction

Problem: User reported that clicking into a line still syncs that line to the top of the preview, but typing immediately afterward aligns perfectly. This means the anchored render path works, but click navigation can still receive an early generic/top-anchor sync before the caret-preferred sync arrives.

Fix:

- `SCN_UPDATEUI` now syncs only for explicit selection or scroll updates.
- Generic/non-scroll update notifications no longer trigger preview sync.
- Caret-preferred sync bypasses the 50ms editor scroll throttle so it can correct an immediately preceding top-anchor scroll.
- Editor-to-preview duplicate detection now includes anchor ratio, not just source line and scroll ratio.
- Added `lastEditorScrollAnchorRatio_` and `kScrollAnchorEpsilon`.

### End-Of-Document Alignment Spacer

Problem: User confirmed click alignment is corrected, then reported the remaining sync mismatch at the very end of the document. In the screenshot, Notepad++ shows substantial blank space after the final Markdown lines, but the WebView preview is clamped to its maximum scroll position and cannot scroll farther down to place the final source line at the same viewport anchor.

Decision: This is normal browser scroll-clamping behavior unless the preview document has EOF slack space. Since Notepad++ effectively provides that slack at EOF, Markdown++ should mimic it.

Fix:

- Added `.markdown-body::after` with `height: calc(100vh - 48px)`.
- This gives the WebView enough bottom scroll room to align final lines/headings with the editor caret position.
- The spacer is visual whitespace only and is not a source-mapped block.

### Visible Mermaid Parse Errors

Problem: Malformed Mermaid diagrams only produced console noise and a generic `.mermaid-error` class, which is not useful to a normal Notepad++ user.

Fix:

- Mermaid rendering is now performed per diagram block instead of one global `.mermaid` run.
- Each Mermaid block stores its original source in `data-mermaid-source`.
- Each block is parsed with `mermaid.parse(...)` before rendering when that API is available.
- If parsing or rendering fails, only that block is replaced with a visible inline error panel.
- The error panel shows:
  - `Mermaid render error`
  - Mermaid's error text
  - the original Mermaid source
- Valid Mermaid blocks continue rendering even if another Mermaid block fails.
- Added light/dark styles for `.markdown-body .mermaid-error`.

### Native Options Plumbing

Problem: Native options were only a placeholder. The next parity step needs persistent settings and menu controls before a richer Options dialog can be useful.

Fix:

- Added `PluginOptions` and `PluginOptionsStore`.
- Options persist to `MarkdownPlusPlus.ini` under Notepad++'s plugin config directory from `NPPM_GETPLUGINSCONFIGDIR`.
- Added menu commands:
  - `Synchronize scrolling`
  - `Render Mermaid diagrams`
  - `Cycle render debounce delay`
- Mermaid and scroll-sync menu entries are checkable and persisted.
- `Options` now shows the current native settings and config file path instead of the old placeholder.
- Mermaid rendering can be toggled without recompiling. The Mermaid asset remains loaded, but `preview.js` skips normalization/rendering when disabled.
- Scroll sync can be toggled. When disabled:
  - editor-to-preview and preview-to-editor scroll sync stop
  - text renders preserve the preview's last known scroll ratio instead of forcing the editor position
- Render debounce delay cycles through:
  - `0`
  - `80`
  - `140`
  - `250`
  - `500`
- Debounce delay `0` renders immediately.

Important files:

- `MarkdownPlusPlus.Native/src/PluginOptions.h`
- `MarkdownPlusPlus.Native/src/PluginOptions.cpp`
- `MarkdownPlusPlus.Native/src/PluginController.cpp`
- `MarkdownPlusPlus.Native/src/PluginController.h`
- `MarkdownPlusPlus.Native/src/PreviewWindow.cpp`
- `MarkdownPlusPlus.Native/src/PreviewWindow.h`
- `MarkdownPlusPlus.Native/src/WebViewHost.cpp`
- `MarkdownPlusPlus.Native/src/WebViewHost.h`
- `MarkdownPlusPlus.Native/src/MarkdownRenderer.cpp`
- `MarkdownPlusPlus.Native/src/MarkdownRenderer.h`
- `MarkdownPlusPlus.Native/assets/preview.js`
- `MarkdownPlusPlus.Native/assets/preview.css`
- `MarkdownPlusPlus.Native/CMakeLists.txt`

Validation performed in WSL:

- `git diff --check` on the touched native source/header/JS/context files
- `cmake -S . -B build/cmake/linux-check`
- `cmake --build build/cmake/linux-check --config Release`
- Node is not installed in WSL, so `preview.js` syntax was not checked with Node.

Full MSVC/Notepad++ validation is still needed.

## Build Command For User

Run from Windows PowerShell using the same `Z:` path style as the existing CMake cache:

```powershell
cd Z:\home\Vibe-Projects\MarkdownViewerPlusPlus
powershell -ExecutionPolicy Bypass -File .\tools\build-native.ps1 -Configuration Release -Install
```

Expected package output:

```text
build\cmake\vs2026-x64\package\Release\MarkdownPlusPlus\
```

The install step copies that `MarkdownPlusPlus` folder into the Notepad++ plugins folder. Close Notepad++ first so the DLL is not locked.

## Test Request For User

Open this file in Notepad++:

```text
Z:\home\Vibe-Projects\MarkdownViewerPlusPlus\smoke-tests\markdownplusplus-smoke.md
```

Test with the Markdown++ preview panel open:

1. Confirm the plugin loads and the preview opens.
2. Confirm Mermaid renders.
3. Confirm the local SVG image renders.
4. Confirm the external HTTPS image renders.
5. Fast-type near and inside the Mermaid section.
6. Watch for preview jumps, full reload flashes, scroll stutter, or delayed updates.
7. Scroll both the editor and preview around the Mermaid block and local image.

## Next Strongest Move

First, rebuild/package and test viewport-anchor alignment with the larger smoke document.

If the editor and preview now align well across inline images/Mermaid, add visible Mermaid parse-error output in the preview so malformed diagrams produce an in-page error instead of only console noise.

If alignment is still off, instrument the rendered `data-sourcepos` block list and tune source-block selection around images/Mermaid.

## Prompt Updates

### 2026-04-27 16:17 PDT

User asked to start a new session log, keep `AI_UNDERSTANDING.md` updated after each prompt, provide build/test commands, and recommend the next strongest move after each prompt.

### 2026-04-27 16:31 PDT

User reported the debounce worked properly. User also reported that jumping around in the Notepad++ document does not sync the preview unless they scroll. Implemented caret-aware editor sync for jump/click/search-style navigation while preserving viewport-first behavior for actual scrolling.

### 2026-04-27 16:46 PDT

User tested the first caret-sync attempt and reported that every click caused a full viewer refresh but did not move the preview. Tightened text-change detection, made selection updates prefer caret source-line sync, and corrected the x64 Scintilla `SCNotification` layout so `modificationType` and `updated` are read from the correct offsets.

### 2026-04-27 17:00 PDT

User confirmed the navigation fix works and asked whether the preview can move to the current editing position in the Markdown file. Implemented caret-anchored debounced edit renders so live text edits restore the WebView preview to `CurrentEditorCaretSourceLine()` rather than the editor viewport's first visible line.

### 2026-04-27 17:18 PDT

User reported edit-follow is not very smooth and shared a screenshot showing source alignment drift with more inline images and Mermaid blocks: the correct heading was synced but placed at the top of the preview rather than matching the caret's relative position in the Notepad++ viewport. Implemented viewport-anchor-ratio sync, late image-load scroll re-application, tiny-scroll suppression, and suppression of extra pre-render scroll sync while an edit render is pending.

### 2026-04-27 17:29 PDT

User reported clicking still syncs to the top, while typing afterward aligns perfectly. Updated click/navigation sync so caret-preferred sync is not blocked by the scroll throttle, generic `SCN_UPDATEUI` events do not trigger top-anchor sync, and editor-to-preview duplicate detection includes anchor ratio.

### 2026-04-27 17:37 PDT

User confirmed click alignment is corrected and asked whether the remaining end-of-document mismatch is normal. Explained that WebView was clamped at maximum scroll while Notepad++ shows EOF slack space. Added a bottom preview spacer via `.markdown-body::after` so final source lines can align with the editor viewport anchor.

### 2026-04-27 17:44 PDT

User confirmed the end-of-document spacer works perfectly. Scroll sync is now considered good enough for this pass. Next strongest move is visible Mermaid parse-error output in the preview.

### 2026-04-27 17:52 PDT

Implemented visible Mermaid parse/render errors. Mermaid blocks now render independently; malformed blocks show an inline error panel with the error text and original source while valid diagrams continue rendering.

### 2026-04-27 18:00 PDT

User tested malformed Mermaid and saw the expected inline `Mermaid render error` panel with parse error text and original source. Visible Mermaid error output is confirmed working.

### 2026-04-27 18:18 PDT

Implemented first-pass native options plumbing: INI-backed persisted settings, menu toggles for Mermaid and scroll sync, debounce-delay cycling, and real behavior changes for Mermaid rendering, scroll sync, and render debounce.

### 2026-04-27 18:25 PDT

User confirmed all native options plumbing is working: Mermaid toggle, scroll-sync toggle, debounce-delay cycling, Options summary, and persistence behavior are accepted for this pass.

### 2026-04-27 18:34 PDT

Implemented plugin install/copy automation:

- Added `tools/install-native-plugin.ps1`.
- Added `-Install`, `-PluginsRoot`, and `-ForceInstall` to `tools/build-native.ps1`.
- `-Install` implies package build, then copies the packaged `MarkdownPlusPlus` folder into the Notepad++ plugins root.
- Default plugin root resolution checks:
  - `%ProgramFiles%\Notepad++\plugins`
  - `%ProgramFiles(x86)%\Notepad++\plugins`
- Installer refuses to run while `notepad++.exe` is active unless `-Force` / `-ForceInstall` is used.
- Installer mirrors the package into `...\plugins\MarkdownPlusPlus`.
- Updated `BUILDING_NATIVE.md` with build/package/install commands and custom `-PluginsRoot` usage.

### 2026-04-27 18:42 PDT

User confirmed the new `tools/build-native.ps1 -Configuration Release -Install` path worked. Build/package/install automation is accepted for this pass.

### 2026-04-27 Install Follow-Up

User later reported that after building with `-Install`, the plugin menu was not present and asked whether `-Package` was also required. Clarification: `-Install` already implies package. Likely causes to investigate:

- Install script copied to a different Notepad++ plugins root than the Notepad++ instance being launched.
- Notepad++ was still running or re-opened from a different install/portable path.
- Plugin folder exists but does not contain `MarkdownPlusPlus.dll` and `assets`.
- Installed folder path should be checked against Notepad++'s actual executable/install location.

### 2026-04-27 23:22 PDT

User confirmed the installed plugin is present and showed that `Plugins > Markdown++ > Options` still opens the temporary summary message box. Replaced the summary with a real native Win32 modal Options dialog backed by the existing `PluginOptions` and INI store.

Implemented:

- Added `MarkdownPlusPlus.Native/src/OptionsDialog.h`.
- Added `MarkdownPlusPlus.Native/src/OptionsDialog.cpp`.
- Added the new files to `MarkdownPlusPlus.Native/CMakeLists.txt`.
- Updated `PluginController::ShowOptions()` to open the dialog, save accepted changes, update menu checks, rerender when Mermaid changes, and resync when scroll sync is re-enabled.

Dialog controls:

- `Render Mermaid diagrams` checkbox.
- `Synchronize scrolling` checkbox.
- Render debounce dropdown with `0`, `80`, `140`, `250`, and `500` ms.
- Read-only config-file path field.
- OK and Cancel buttons.

Validation:

- `x86_64-w64-mingw32-g++ -std=c++20 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -I MarkdownPlusPlus.Native/src -fsyntax-only MarkdownPlusPlus.Native/src/OptionsDialog.cpp`
- `cmake -S . -B build/cmake/linux-check`
- `cmake --build build/cmake/linux-check --config Release`
- `git diff --check -- MarkdownPlusPlus.Native/src/OptionsDialog.cpp MarkdownPlusPlus.Native/src/OptionsDialog.h MarkdownPlusPlus.Native/src/PluginController.cpp MarkdownPlusPlus.Native/CMakeLists.txt`

Next strongest move: build/install on Windows, verify the dialog replaces the message box, and exercise OK/Cancel plus restart persistence.

### 2026-04-27 23:30 PDT

User tested the native Options dialog and reported:

- The `Preview` section label is unclear/unnecessary.
- Settings did not save.
- The plugin did not automatically open the preview when Notepad++ opened a `.md` file.
- Expected behavior: preview should auto-open for Markdown files and should not load for non-Markdown files.

Implemented follow-up fixes:

- Removed the unnecessary `Preview` label from the Options dialog.
- Added `AutoOpenMarkdown` to `PluginOptions` with a default of `true`.
- Added an Options checkbox: `Open automatically for Markdown files`.
- Changed `PluginOptionsStore::Save()` to return success/failure, ensure the parent directory exists, write `AutoOpenMarkdown`, and flush the INI profile cache.
- Changed `PluginController::ShowOptions()` to show an error if the INI write fails and roll back in-memory settings on failure.
- Added Notepad++ `NPPN_READY` handling.
- Added current-document Markdown extension detection for `.md`, `.markdown`, `.mdown`, `.mkd`, and `.mkdn`.
- Added auto-open/auto-close behavior on `NPPN_READY` and `NPPN_BUFFERACTIVATED` when auto-open is enabled.

Validation:

- `x86_64-w64-mingw32-g++ -std=c++20 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -I MarkdownPlusPlus.Native/src -fsyntax-only MarkdownPlusPlus.Native/src/PluginOptions.cpp`
- `x86_64-w64-mingw32-g++ -std=c++20 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -I MarkdownPlusPlus.Native/src -fsyntax-only MarkdownPlusPlus.Native/src/OptionsDialog.cpp`
- `cmake --build build/cmake/linux-check --config Release`
- `git diff --check -- MarkdownPlusPlus.Native/src/PluginOptions.h MarkdownPlusPlus.Native/src/PluginOptions.cpp MarkdownPlusPlus.Native/src/OptionsDialog.cpp MarkdownPlusPlus.Native/src/NppApi.h MarkdownPlusPlus.Native/src/PluginController.h MarkdownPlusPlus.Native/src/PluginController.cpp SESSION_LOG_2026-04-27_1617_PDT.md AI_UNDERSTANDING.md`

Note: full `PluginController.cpp` MinGW syntax check is still blocked in this Linux workspace by the missing WebView2 SDK header, which is expected for the Windows-only plugin target.

Next strongest move: rebuild/install on Windows, confirm option persistence writes `MarkdownPlusPlus.ini`, then test `.md` startup auto-open and non-Markdown auto-close.

### 2026-04-27 23:36 PDT

User reported the native Options dialog produced the save-error message and asked to remove it, since the menu-based options were working before the dialog. Agreed that menu-selectable options are the better surface for this pass.

Implemented:

- Removed the native Options dialog from the build.
- Deleted `MarkdownPlusPlus.Native/src/OptionsDialog.h`.
- Deleted `MarkdownPlusPlus.Native/src/OptionsDialog.cpp`.
- Removed the `Options` menu command and `PluginController::ShowOptions()`.
- Kept all settings as direct plugin-menu entries.
- Added `Open automatically for Markdown files` as a checked plugin-menu option.
- Restored option saving to the simpler pre-dialog fire-and-forget INI write path while preserving the new `AutoOpenMarkdown` key.

Current menu shape:

- `Markdown++`
- `Refresh preview`
- `Open automatically for Markdown files`
- `Synchronize scrolling`
- `Render Mermaid diagrams`
- `Cycle render debounce delay`
- `About`

Validation:

- Confirmed no remaining `OptionsDialog`, `ShowOptions`, `OptionsCommand`, `RunOptions`, or dialog save-error references in native source/CMake.
- `x86_64-w64-mingw32-g++ -std=c++20 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -I MarkdownPlusPlus.Native/src -fsyntax-only MarkdownPlusPlus.Native/src/PluginOptions.cpp`
- `cmake --build build/cmake/linux-check --config Release`
- `git diff --check -- MarkdownPlusPlus.Native/src/PluginOptions.h MarkdownPlusPlus.Native/src/PluginOptions.cpp MarkdownPlusPlus.Native/src/PluginController.h MarkdownPlusPlus.Native/src/PluginController.cpp MarkdownPlusPlus.Native/CMakeLists.txt MarkdownPlusPlus.Native/src/OptionsDialog.cpp MarkdownPlusPlus.Native/src/OptionsDialog.h`

Next strongest move: rebuild/install on Windows and verify the menu-only options persist, the Options item is gone, and Markdown auto-open behaves as expected.

### 2026-04-27 23:48 PDT

User confirmed the menu-only options work great and asked for debounce choices in another submenu.

Implemented:

- Removed the old flat `Cycle render debounce delay` command.
- Added `NPPM_ALLOCATECMDID` to the native Notepad++ API constants.
- Added dynamic Win32 submenu insertion for `Render debounce`.
- Allocates five safe command IDs through Notepad++ for debounce menu items.
- Handles dynamic submenu clicks in `PluginController::MessageProc(WM_COMMAND, ...)`.
- Adds debounce choices:
  - `0 ms (immediate)`
  - `80 ms`
  - `140 ms`
  - `250 ms`
  - `500 ms`
- Checks the currently active debounce value in the submenu.
- Saves the chosen value through the existing INI-backed settings path.

Current menu shape:

- `Markdown++`
- `Refresh preview`
- `Open automatically for Markdown files`
- `Synchronize scrolling`
- `Render Mermaid diagrams`
- `Render debounce >`
- `About`

Validation:

- Confirmed no remaining stale flat cycle-debounce command references.
- `cmake --build build/cmake/linux-check --config Release`
- `git diff --check -- MarkdownPlusPlus.Native/src/NppApi.h MarkdownPlusPlus.Native/src/PluginController.h MarkdownPlusPlus.Native/src/PluginController.cpp SESSION_LOG_2026-04-27_1617_PDT.md AI_UNDERSTANDING.md`

Note: the dynamic submenu uses Notepad++ command ID allocation (`NPPM_ALLOCATECMDID`) so submenu `WM_COMMAND` messages are delivered to `messageProc`.

Next strongest move: rebuild/install on Windows and verify the `Render debounce` submenu appears, each value can be selected, the selected value shows a checkmark, and the choice persists after restart.

### 2026-04-27 23:52 PDT

User confirmed the debounce submenu feels good and asked what `debounce` means / what to look for while testing.

Clarification to give:

- Debounce is the short wait after typing before Markdown++ rerenders the preview.
- `0 ms` renders immediately on each edit notification.
- Larger values wait for typing to pause, reducing preview churn and WebView work.
- User-facing test is not exact timing; it is whether typing feels responsive without flicker, scroll jumps, or stale preview lag.
- Recommended default remains `140 ms` unless the user prefers `80 ms` for snappier feedback or `250 ms` for heavier docs.

Next strongest move: decide whether to rename the menu label from `Render debounce` to plainer wording such as `Preview update delay` if `debounce` feels too technical.

### 2026-04-27 23:54 PDT

User agreed to rename the debounce submenu to clearer wording.

Implemented:

- Renamed the visible submenu label from `Render debounce` to `Preview update delay`.
- Kept internal code/setting names as debounce because they match the implementation.

Validation:

- `cmake --build build/cmake/linux-check --config Release`
- `git diff --check -- MarkdownPlusPlus.Native/src/PluginController.cpp SESSION_LOG_2026-04-27_1617_PDT.md AI_UNDERSTANDING.md`

Next strongest move: rebuild/install on Windows and verify the menu now shows `Preview update delay >` with the same checked timing choices.

### 2026-04-27 23:56 PDT

User confirmed the renamed `Preview update delay` submenu works and that the selected delay checkmark carries/persists.

Accepted:

- Dynamic submenu insertion works in Notepad++.
- `Preview update delay` wording is better than `Render debounce`.
- Delay choice checkmark updates and survives restart.

Next strongest move: do a small menu polish and defaults pass, then move back to rendering parity items such as local-link click handling or export support.

### 2026-04-28 00:00 PDT

User asked to move forward with local-link click handling and to create a new `smoke-test.md` file in the smoke-test directory for this specific feature test.

Implemented:

- Added preview-side click interception for local Markdown links in `MarkdownPlusPlus.Native/assets/preview.js`.
- Local links post `{ type: "linkClick", href: rawHref }` to WebView2 instead of relying on browser navigation.
- Added `WebViewHost::LinkCallback` and `SetLinkCallback`.
- Added `PreviewWindow::SetLinkCallback`.
- Added `NPPM_DOOPEN` to native Notepad++ API constants.
- Added native link message parsing in `WebViewHost::HandleWebMessage`.
- Added `PluginController::ResolvePreviewLinkPath()`:
  - handles raw relative links such as `./file.md` and `../file.md`;
  - handles `https://markdownplusplus.document/...` virtual-host links;
  - handles `file:` URLs;
  - strips query/fragment and URL-decodes;
  - canonicalizes Windows paths.
- Added `PluginController::OpenPreviewLink()`:
  - opens only existing Markdown files;
  - uses `NPPM_DOOPEN` so Notepad++ opens or switches to the target file.
- Added focused smoke test file: `smoke-tests/smoke-test.md`.

Validation:

- `x86_64-w64-mingw32-g++ -std=c++20 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -I MarkdownPlusPlus.Native/src -fsyntax-only MarkdownPlusPlus.Native/src/PluginOptions.cpp`
- `cmake --build build/cmake/linux-check --config Release`
- `git diff --check -- MarkdownPlusPlus.Native/assets/preview.js MarkdownPlusPlus.Native/src/WebViewHost.h MarkdownPlusPlus.Native/src/WebViewHost.cpp MarkdownPlusPlus.Native/src/PreviewWindow.h MarkdownPlusPlus.Native/src/PreviewWindow.cpp MarkdownPlusPlus.Native/src/NppApi.h MarkdownPlusPlus.Native/src/PluginController.h MarkdownPlusPlus.Native/src/PluginController.cpp smoke-tests/smoke-test.md`

Note: full native C++ compile remains Windows/WebView2-SDK-only from this Linux workspace.

Next strongest move: build/install on Windows and test `smoke-tests/smoke-test.md` by clicking same-folder and parent-folder Markdown links in the preview.

### 2026-04-28 00:05 PDT

User confirmed validation passed for local Markdown link click handling.

Accepted:

- Local Markdown links in the preview open/switch Notepad++ to the target file.
- Same-folder and parent-folder relative links are working.
- Focused smoke test `smoke-tests/smoke-test.md` is useful for this feature.

Next strongest move: add external-link handling so HTTPS links open in the default browser while local Markdown links continue opening in Notepad++.

### 2026-04-28 00:07 PDT

User said "go forth" on external-link handling.

Implemented:

- Updated preview click handling to also intercept `http://` and `https://` links.
- Preview now posts the normalized external URL to native code.
- Added native external URL detection for `http://` and `https://`.
- External links open via `ShellExecuteW(..., "open", ...)`, using the user's default browser.
- Local Markdown links still resolve/open through the existing `NPPM_DOOPEN` path.
- Updated `smoke-tests/smoke-test.md` so the external example is part of the feature check.

Validation:

- `cmake --build build/cmake/linux-check --config Release`
- `git diff --check -- MarkdownPlusPlus.Native/assets/preview.js MarkdownPlusPlus.Native/src/PluginController.cpp smoke-tests/smoke-test.md SESSION_LOG_2026-04-27_1617_PDT.md AI_UNDERSTANDING.md`

Next strongest move: build/install on Windows and verify the same smoke test opens local Markdown links in Notepad++ and the external HTTPS example in the default browser.

### 2026-04-28 00:11 PDT

User reported that clicking a URL opens the site and also another browser instance under a different profile. This means the external-link path was double-opening: one path through the new native `ShellExecuteW` handler and another leaking through WebView/browser handling.

Implemented fix:

- Tightened preview external-link detection so `https://markdownplusplus.document/...` and `https://markdownplusplus.local/...` are not treated as external browser links.
- Added WebView2 `NewWindowRequested` handling and marks those requests handled, preventing WebView from spawning an additional browser/profile window.
- Kept native `ShellExecuteW` as the single intended external-link open path.

Validation:

- `cmake --build build/cmake/linux-check --config Release`
- `git diff --check -- MarkdownPlusPlus.Native/assets/preview.js MarkdownPlusPlus.Native/src/WebViewHost.h MarkdownPlusPlus.Native/src/WebViewHost.cpp SESSION_LOG_2026-04-27_1617_PDT.md AI_UNDERSTANDING.md`

Next strongest move: rebuild/install on Windows and verify one click on `https://example.com/` opens exactly one default-browser window/profile.

### 2026-04-28 00:15 PDT

User confirmed all link behavior is working after the double-open fix.

Accepted:

- Local Markdown links open/switch inside Notepad++.
- External `http/https` links open in the default browser.
- External links no longer double-open another browser/profile instance.
- `smoke-tests/smoke-test.md` covers the accepted link behavior.

Next strongest move: implement local anchor link scrolling so `#heading` links navigate within the preview without opening a file or browser.

### 2026-04-28 00:16 PDT

User agreed to implement local anchor link scrolling.

Implemented:

- Added preview-side handling for fragment-only links such as `#expected-behavior`.
- Fragment links now find the target by `id` or `name`, scroll it into view, and update the hash.
- Fragment scrolling stays entirely inside the preview; it does not post to native code or open a file/browser.
- Updated `smoke-tests/smoke-test.md` expected behavior for fragment links.

Validation:

- `cmake --build build/cmake/linux-check --config Release`
- `git diff --check -- MarkdownPlusPlus.Native/assets/preview.js smoke-tests/smoke-test.md SESSION_LOG_2026-04-27_1617_PDT.md AI_UNDERSTANDING.md`

Next strongest move: build/install on Windows and verify the fragment link in `smoke-tests/smoke-test.md` scrolls to `Expected Behavior` without opening anything else.

### 2026-04-28 00:20 PDT

User reported the fragment link did not jump to `Expected Behavior`.

Likely cause:

- cmark-gfm output did not include heading `id` attributes, so `#expected-behavior` had no DOM target.

Implemented fix:

- Added preview-side heading ID generation for `.markdown-body h1` through `h6`.
- Heading IDs are generated from heading text with a simple GitHub-style slug.
- Duplicate heading IDs get numeric suffixes.
- Anchor lookup now falls back to matching headings by generated slug.
- Heading IDs are assigned on initial document load and after in-place content replacement.

Validation:

- `cmake --build build/cmake/linux-check --config Release`
- `git diff --check -- MarkdownPlusPlus.Native/assets/preview.js SESSION_LOG_2026-04-27_1617_PDT.md AI_UNDERSTANDING.md`

Next strongest move: rebuild/install on Windows and retest the `Jump to expected behavior` link.

### 2026-04-28 00:23 PDT

User confirmed the fragment jumps to `Expected Behavior`, but Notepad++ does not sync to the jumped location.

Cause:

- Anchor scrolling intentionally suppressed scroll notifications, so the WebView moved but no `previewScroll` message reached native code.

Implemented fix:

- Split preview scroll posting into:
  - `postScrollRatio()` for unconditional posting.
  - `postScrollRatioIfAllowed()` for normal scroll events that should obey suppression.
- Anchor jumps now post one explicit scroll-sync message on the next animation frame after `scrollIntoView()`.
- Manual/programmatic scroll suppression remains intact for normal sync paths.

Validation:

- `cmake --build build/cmake/linux-check --config Release`
- `git diff --check -- MarkdownPlusPlus.Native/assets/preview.js SESSION_LOG_2026-04-27_1617_PDT.md AI_UNDERSTANDING.md`

Next strongest move: rebuild/install on Windows and verify the preview anchor jump also moves the Notepad++ editor to the corresponding Markdown source line.

### 2026-04-28 00:27 PDT

User reported Notepad++ still does not scroll after the preview anchor jump.

Likely causes:

- The anchor scroll message may still arrive during the native cross-scroll suppression window.
- The message used `currentSourceLine()` after the scroll rather than the clicked heading's explicit source line.

Implemented stronger fix:

- Preview `previewScroll` messages now include a `force` boolean.
- Normal scroll notifications send `force: false`.
- Anchor jumps send `force: true`.
- Anchor jumps now send the clicked target node's `data-sourcepos` start line instead of relying only on viewport-derived `currentSourceLine()`.
- `WebViewHost` parses the `force` boolean and passes it through the scroll callback.
- `PluginController::HandlePreviewScroll()` bypasses preview-to-editor suppression, throttle, and duplicate checks only for forced anchor jumps.

Validation:

- `cmake --build build/cmake/linux-check --config Release`
- `git diff --check -- MarkdownPlusPlus.Native/assets/preview.js MarkdownPlusPlus.Native/src/WebViewHost.h MarkdownPlusPlus.Native/src/WebViewHost.cpp MarkdownPlusPlus.Native/src/PluginController.h MarkdownPlusPlus.Native/src/PluginController.cpp SESSION_LOG_2026-04-27_1617_PDT.md AI_UNDERSTANDING.md`

Next strongest move: rebuild/install on Windows and retest whether clicking `Jump to expected behavior` moves both the preview and Notepad++ editor.

### 2026-04-28 00:32 PDT

User reported the forced anchor-sync change still does not make Notepad++ scroll. User asked to pick this up in another session and to write notes/current next steps.

Current failing behavior:

- In `smoke-tests/smoke-test.md`, clicking `Jump to expected behavior` scrolls the WebView preview to `## Expected Behavior`.
- Notepad++ editor does not scroll to the corresponding Markdown source line.
- Local Markdown links and external links are working and accepted.

Current attempted implementation:

- `MarkdownPlusPlus.Native/assets/preview.js`
  - Generates heading IDs in `assignHeadingIds()`.
  - Handles fragment-only links in `scrollToAnchor(hash)`.
  - Finds the anchor target and calls `scrollIntoView()`.
  - Sends a forced preview-scroll message:
    ```js
    postScrollMessage(sourceLineForNode(target), currentScrollRatio(), true);
    ```
  - `sourceLineForNode(target)` walks upward looking for `data-sourcepos` and falls back to `currentSourceLine()`.

- `MarkdownPlusPlus.Native/src/WebViewHost.cpp`
  - Parses `previewScroll` messages with `line`, `ratio`, and `force`.
  - Passes `force` through `ScrollCallback`.

- `MarkdownPlusPlus.Native/src/PluginController.cpp`
  - `SetScrollCallback` now receives `(sourceLine, ratio, force)`.
  - `HandlePreviewScroll(sourceLine, ratio, force)` bypasses preview-to-editor suppression, throttle, and duplicate checks when `force` is true.
  - If `sourceLine > 0`, it calls `ScrollEditorToSourceLine(sourceLine)`.

Likely failure points to inspect next:

1. The forced `previewScroll` message may not be reaching `WebViewHost::HandleWebMessage()`.
2. `TryParseJsonBool()` or `TryParsePreviewScrollMessage()` may not parse the WebView JSON shape correctly.
3. `sourceLineForNode(target)` may be returning `0` because the generated heading element may not have `data-sourcepos`; if so, native falls back to ratio or no useful line.
4. `ScrollEditorToSourceLine()` may receive a good line but not move because `SCI_VISIBLEFROMDOCLINE` / visible-line delta is wrong for the current Scintilla state.
5. The callback path may be blocked because `scrollCallback_` is not set after a preview recreation.

Recommended next debug step:

- Add temporary targeted instrumentation for anchor jumps:
  - In `preview.js`, include `debugReason: "anchor"` and the target heading text/id in the posted message.
  - In `WebViewHost::HandleWebMessage()`, surface a short `OutputDebugStringW` or temporary `MessageBoxW` with the raw JSON and parsed `line/ratio/force`.
  - In `PluginController::HandlePreviewScroll()`, log/message the received `line/ratio/force` and whether `ScrollEditorToSourceLine()` is called.
  - In `ScrollEditorToSourceLine()`, log/message `sourceLine`, `documentLine`, `targetVisibleLine`, `currentFirstVisibleLine`, and `delta`.

Recommended next functional fix after instrumentation:

- If heading nodes lack `data-sourcepos`, propagate sourcepos during heading ID generation by finding the nearest heading/source block or by using cmark sourcepos output around headings.
- If the message reaches native with a good line, focus on Scintilla scrolling math.
- If the message does not reach native, focus on WebView postMessage / JSON parsing.

Next strongest move for the next session: instrument the anchor jump payload and native scroll call chain, then remove the temporary diagnostics once the break point is identified.

### 2026-04-28 00:33 PDT

Session handoff note:

- User confirmed the anchor editor-sync issue is still unresolved and chose to pause this session.
- Do not keep trying blind fixes first next time. Start by instrumenting the forced anchor jump path end to end.
- Current accepted behavior:
  - Auto-open Markdown files works.
  - Local Markdown file links open/switch in Notepad++.
  - External URL clicks open exactly once in the default browser.
  - Fragment links scroll the WebView preview to the expected heading.
- Current failing behavior:
  - Fragment links do not also scroll the Notepad++ editor to the target Markdown source line.
  - Repro remains `smoke-tests/smoke-test.md` -> click `Jump to expected behavior`.
- Next-session first task:
  1. Add temporary diagnostics in `preview.js`, `WebViewHost.cpp`, and `PluginController.cpp`.
  2. Rebuild/install on Windows.
  3. Click `Jump to expected behavior` and determine whether the break is JS message creation, WebView message parsing, native callback delivery, missing `data-sourcepos`, or Scintilla scroll math.
  4. Patch the identified break and remove diagnostics.

Next strongest move for the next session: add targeted instrumentation before making another functional change.

### 2026-04-30

User rebuilt/tested the anchor diagnostics but reported
`%TEMP%\MarkdownPlusPlus-anchor-debug.log` did not exist after clicking the
fragment link. That means either the installed DLL was not the diagnostic build,
or the anchor/forced scroll message never reached native code at all.

Added stronger temporary diagnostic heartbeat logging:

- `MarkdownPlusPlus.Native/src/PluginController.cpp`
  - Logs `PluginController initialized` during plugin initialization.
  - Logs when the preview scroll and link callbacks are registered.
- `MarkdownPlusPlus.Native/src/WebViewHost.cpp`
  - Logs when WebView creation is requested.
  - Logs when the WebView message handler is registered.
  - Logs when WebViewHost becomes ready.

After this change, the log file should exist as soon as the diagnostic build is
loaded and the preview is opened. If it still does not exist, suspect install or
wrong DLL first. If it exists but contains no anchor raw message after clicking
`Jump to expected behavior`, suspect the preview JS message is not being posted
or not reaching `WebMessageReceived`.

### 2026-04-29 21:54 PDT

User asked to understand the project and pick up where we left off. Re-read
`AI_UNDERSTANDING.md`, `SESSION_LOG_2026-04-27_1617_PDT.md`, and the native
source around fragment-link handling.

Confirmed current active issue:

- `smoke-tests/smoke-test.md` -> `Jump to expected behavior` scrolls the WebView
  preview to `## Expected Behavior`.
- Notepad++ editor still does not follow to the corresponding source line.
- Local Markdown links and external browser links remain accepted behavior.

Implemented the planned temporary targeted diagnostics:

- `MarkdownPlusPlus.Native/assets/preview.js`
  - Anchor jump `previewScroll` messages now include:
    - `debugReason: "anchor"`
    - `targetId`
    - `targetText`
    - `targetSourceLine`
    - `targetSourcepos`
- `MarkdownPlusPlus.Native/src/WebViewHost.cpp`
  - Logs forced/anchor WebView messages to `OutputDebugStringW`.
  - Also appends the same diagnostics to `%TEMP%\MarkdownPlusPlus-anchor-debug.log`.
  - Logs raw JSON and parsed `scroll/link/line/ratio/force/hasScrollCallback`.
- `MarkdownPlusPlus.Native/src/PluginController.cpp`
  - Logs forced `HandlePreviewScroll()` inputs and state:
    `sourceLine`, `ratio`, `force`, `previewVisible`, `scrollSyncEnabled`,
    and `previewToEditorSuppressed`.
  - Logs `ScrollEditorToSourceLine()` Scintilla math:
    `sourceLine`, `documentLine`, `targetVisibleLine`, `currentFirstVisibleLine`,
    and `delta`.
  - Logs whether `SCI_LINESCROLL` was sent and the first visible line afterward.

Validation:

- `cmake --build build/cmake/linux-check --config Release`
- `rg -n "[[:blank:]]+$" MarkdownPlusPlus.Native/assets/preview.js MarkdownPlusPlus.Native/src/WebViewHost.cpp MarkdownPlusPlus.Native/src/PluginController.cpp MarkdownPlusPlus.Native/src/PluginController.h`

Note: the Linux CMake target is still a Windows-only stub, and full native
compile still requires the Windows/WebView2/MSVC build path.

Next strongest move: rebuild/install on Windows, click `Jump to expected
behavior`, inspect `%TEMP%\MarkdownPlusPlus-anchor-debug.log`, identify the
break point, patch it, then remove the temporary diagnostics.

### 2026-04-30

User asked to persist the freshly rediscovered project context into
`AI_UNDERSTANDING.md` so future sessions do not need to repeat the same repo
survey. Added `QUICK_RELOAD_CONTEXT_2026_04_30` covering the legacy C# track,
native C++ rewrite track, current accepted native behavior, unresolved
fragment-link editor-sync bug, temporary diagnostics, build/test commands, and
next strongest move.

### 2026-04-30 anchor follow-up

User provided the heartbeat log. It confirms the diagnostic DLL is loaded and
WebView message handling is registered, but there were no anchor raw-message or
forced-scroll entries. That points away from install/wrong-DLL and toward the JS
anchor path failing before `window.chrome.webview.postMessage(...)`, or the
message not reaching WebView2.

Implemented a stronger anchor JS path:

- `MarkdownPlusPlus.Native/assets/preview.js`
  - Adds `postDebugMessage(...)` for targeted diagnostics.
  - Logs anchor click, missing target, found target, and history update failure.
  - Computes the target source line before the history/hash update.
  - Wraps `history.replaceState` / `window.location.hash` in `try/catch` so a
    history exception cannot prevent native sync.
  - Posts the forced anchor scroll immediately, on `requestAnimationFrame`, and
    once more after 80 ms.
- `MarkdownPlusPlus.Native/src/WebViewHost.cpp`
  - Widened diagnostics to log raw `previewDebug`, `previewScroll`, `linkClick`,
    any `debugReason`, and forced messages, not just exact
    `debugReason:"anchor"` messages.

Validation:

- `node --check MarkdownPlusPlus.Native/assets/preview.js`
- `cmake --build build/cmake/linux-check --config Release`
- `rg -n "[[:blank:]]+$" ...` on touched files

Next test: rebuild/install on Windows, clear the temp log, open
`smoke-tests/smoke-test.md`, click `Jump to expected behavior`, and inspect
`%TEMP%\MarkdownPlusPlus-anchor-debug.log`. Expected best case: editor now
scrolls to `## Expected Behavior`. If not, the log should include
`previewDebug` / `previewScroll` raw messages showing the next break point.

User confirmed the patched anchor path worked: clicking `Jump to expected
behavior` now scrolls both the WebView preview and the Notepad++ editor to
`## Expected Behavior`.

Cleaned up the temporary diagnostics while preserving the functional fix:

- Kept the anchor sync hardening in `preview.js`:
  - source line is computed before hash/history update,
  - hash/history update is wrapped in `try/catch`,
  - forced preview scroll is posted immediately, on `requestAnimationFrame`, and
    after 80 ms.
- Removed JS `previewDebug` messages and debug payload fields.
- Removed `%TEMP%\MarkdownPlusPlus-anchor-debug.log` logging and
  `OutputDebugStringW` diagnostic plumbing from `WebViewHost.cpp` and
  `PluginController.cpp`.
- Simplified `ScrollEditorToSourceLine` back to its non-diagnostic signature.

Validation after cleanup:

- `node --check MarkdownPlusPlus.Native/assets/preview.js`
- `cmake --build build/cmake/linux-check --config Release`
- no leftover anchor diagnostic strings in native/preview sources
- no trailing whitespace in touched source files

Next strongest move: rebuild/install the cleanup build on Windows and quickly
retest `smoke-tests/smoke-test.md` anchor click once to make sure the cleaned
build preserves the confirmed behavior.

User confirmed the cleanup build still works. Fragment-only anchor links now
remain accepted behavior after diagnostics were removed: the preview jumps and
the Notepad++ editor scrolls to the target source heading.

### 2026-04-30 native parity hardening

User asked to finish off native parity hardening. Implemented the remaining
practical legacy renderer action surface as native menu commands:

- `Copy HTML to clipboard`
  - Renders the current buffer through cmark-gfm.
  - Places CF_HTML on the clipboard with exact UTF-8 byte offsets.
  - Places the rendered HTML fragment as `CF_UNICODETEXT` fallback, matching
    the legacy plugin's clipboard behavior.
- `Export HTML...`
  - Uses a native save dialog.
  - Writes a standalone UTF-8 HTML document with embedded preview CSS.
  - Adds a `file://` base URI pointing at the current Markdown directory so
    local image/link paths keep resolving from the source document folder.
  - References the bundled Mermaid and preview scripts by file URI when Mermaid
    rendering is enabled.
- `Export PDF...`
  - Uses the live WebView2 preview through `ICoreWebView2_7::PrintToPdf`.
  - Opens/refeshes the preview first if the WebView document is not ready, then
    asks the user to rerun after load.
- `Print...`
  - Uses the live WebView2 preview through
    `ICoreWebView2_16::ShowPrintUI(COREWEBVIEW2_PRINT_DIALOG_KIND_SYSTEM)`.

Also added a print-media CSS override so the preview EOF spacer, which is needed
for scroll-sync parity, does not create trailing blank pages in print/PDF output.
Updated the native README and smoke test checklist for the new parity commands.

Validation:

- `node --check MarkdownPlusPlus.Native/assets/preview.js`
- `cmake --build build/cmake/linux-check --config Release`
- no trailing whitespace in touched native source/docs

Linux validation cannot compile the Windows/WebView2 plugin. Next strongest
move: rebuild/install on Windows, open
`smoke-tests/markdownplusplus-smoke.md`, and smoke-test `Copy HTML to clipboard`,
`Export HTML...`, `Export PDF...`, and `Print...` from the Markdown++ menu.

Follow-up: User tested `Copy HTML to clipboard` by pasting into a plain-text
target and saw exact Markdown text. Checked the legacy C# implementation:
`ClipboardHelper.CopyToClipboard(BuildHtml(ConvertedText,...), ConvertedText)`
uses the rendered HTML fragment, not raw Markdown, as the text fallback. Patched
native copy behavior to use `rendered.articleHtml` for both CF_HTML content and
the Unicode text fallback, and updated the smoke expectation accordingly.

Follow-up: User reported `Export HTML...` opened the generated file, but the
`Jump to expected behavior` fragment link did nothing. Root cause: exported HTML
runs outside WebView2 native messaging, and `preview.js` returned from the click
handler before fragment-only anchors when `window.chrome.webview` was absent.
Also, standalone export only loaded `preview.js` when Mermaid was enabled even
though that script also assigns heading IDs and handles fragment jumps. Patched
`handleLinkClick()` so `#fragment` links are handled before the WebView2
postMessage check, and changed standalone HTML export to always include
`preview.js` while only including Mermaid itself when Mermaid rendering is
enabled. Added a standalone exported-HTML anchor check to the smoke file.

Follow-up: User asked whether `smoke-tests/smoke-test.pdf` generated as a valid
stand-alone PDF because opening it prompted for `.md` permission. Inspected the
PDF directly. It is structurally valid (`%PDF-1.4`, one page, trailer/EOF
present), has no `/OpenAction`, `/Launch`, `/GoToR`, or JavaScript action, but
it is not cleanly stand-alone: WebView2 generated link annotations for local
Markdown links as preview-only URIs:

```text
https://markdownplusplus.document/markdownplusplus-smoke.md
https://markdownplusplus.document/BUILDING_NATIVE.md
```

Patched PDF export so `preview.js` exposes `preparePdfExport()` /
`restorePdfExport()`. Before `ICoreWebView2_7::PrintToPdf`, native code asks the
preview to temporarily remove hrefs for preview-local/relative/file links while
leaving fragment links and external web links intact, then restores the preview
links after PDF generation completes or fails. Updated the smoke expectation to
verify the PDF has no local Markdown permission prompts or
`markdownplusplus.document` link annotations.

Follow-up: User confirmed the `Print...` menu command worked. Treat WebView2
system print UI parity as accepted for this pass. Remaining parity validation is
focused on regenerated PDF stand-alone link cleanup, exported HTML fragment
jumps after the standalone patch, and clipboard fallback behavior after the copy
patch.

Follow-up: User rebuilt and tested the remaining parity fixes and reported
everything works perfectly. Treat native parity hardening as accepted for this
pass:

- `Copy HTML to clipboard` no longer falls back to raw Markdown.
- `Export HTML...` standalone fragment jumps work.
- `Export PDF...` produces a stand-alone PDF without preview-only
  `markdownplusplus.document` / local `.md` permission prompts.
- `Print...` works.

Next strongest move: either do a small release-readiness cleanup pass
(documentation, package contents, version/readme notes, and final smoke-test
checklist) or move to the next deliberate feature gap. Do not reintroduce the
rejected Options dialog; keep menu-only settings.

### 2026-04-30 release-readiness cleanup

User asked to go through the release-readiness cleanup steps before discussing
feature gaps. Completed:

- Rewrote `MarkdownPlusPlus.Native/README.md` around the accepted native
  feature set, menu surface, runtime layout, validation files, and legacy C#
  project boundary.
- Added `MarkdownPlusPlus.Native/SMOKE_CHECKLIST.md` as the final manual
  release smoke checklist.
- Added `MarkdownPlusPlus.Native/RELEASE_NOTES.md` for native test build
  `0.1.0.0`.
- Updated `BUILDING_NATIVE.md` with expected output/package trees and smoke
  test references.
- Updated `MarkdownPlusPlus.Native/DEPENDENCIES.md` to remove stale completed
  integration work and point to the smoke checklist.
- Added a small native rewrite pointer to the root `README.md`.
- Verified the current package folder shape:

```text
build/cmake/vs2026-x64/package/Release/MarkdownPlusPlus/MarkdownPlusPlus.dll
build/cmake/vs2026-x64/package/Release/MarkdownPlusPlus/assets/preview.css
build/cmake/vs2026-x64/package/Release/MarkdownPlusPlus/assets/preview.js
build/cmake/vs2026-x64/package/Release/MarkdownPlusPlus/assets/mermaid/README.md
build/cmake/vs2026-x64/package/Release/MarkdownPlusPlus/assets/mermaid/mermaid.min.js
```

Validation:

- `node --check MarkdownPlusPlus.Native/assets/preview.js`
- `cmake --build build/cmake/linux-check --config Release`
- trailing-whitespace scan on touched release-readiness docs

Next discussion: feature gaps. Strong candidates are theme/custom CSS controls,
file-extension configurability beyond hard-coded Markdown extensions,
find-in-preview / preview context commands, accessibility/keyboarding polish,
and release packaging automation/versioning.

### 2026-04-30 external-link profile regression

User reported a smoke-test regression: the sample external URL opened another
Edge profile instead of staying on the plugin-controlled/default-browser path.

Fix:

- WebView2 `NavigationStarting` now cancels user-initiated preview navigation
  and routes routable `http`, `https`, and `file` URLs through the existing
  Markdown++ link callback.
- WebView2 `NewWindowRequested` now marks the request handled and routes the
  requested URI through the same callback instead of letting WebView2 own the
  popup/profile path.
- Preview JS now intercepts modified primary-clicks and middle-click
  `auxclick` events for local/external Markdown preview links.
- Native external-link classification now excludes
  `https://markdownplusplus.document` and `https://markdownplusplus.local` so
  internal preview links cannot be accidentally shell-opened as browser URLs.

Validation:

- `git diff --check -- MarkdownPlusPlus.Native/src/WebViewHost.cpp MarkdownPlusPlus.Native/src/PluginController.cpp MarkdownPlusPlus.Native/assets/preview.js`
- `cmake --build build/cmake/linux-check --config Release`
- Windows VS2026 UNC package build:
  `cmake --build "\\wsl.localhost\Ubuntu\home\Vibe-Projects\MarkdownViewerPlusPlus\build\cmake\vs2026-x64-unc" --config Release --target markdownplusplus_package`

Fresh package:

```text
build/cmake/vs2026-x64-unc/package/Release/MarkdownPlusPlus/MarkdownPlusPlus.dll
build/cmake/vs2026-x64-unc/package/Release/MarkdownPlusPlus/assets/preview.css
build/cmake/vs2026-x64-unc/package/Release/MarkdownPlusPlus/assets/preview.js
build/cmake/vs2026-x64-unc/package/Release/MarkdownPlusPlus/assets/mermaid/README.md
build/cmake/vs2026-x64-unc/package/Release/MarkdownPlusPlus/assets/mermaid/mermaid.min.js
```

Note: The normal `tools/build-native.ps1 -Package` path failed from this Codex
PowerShell session because it entered the repo through
`\\wsl.localhost\Ubuntu\...` while the existing `vs2026-x64` CMake cache was
created from `Z:\home\...`; the session also lacked a `Z:` drive mapping. The
separate `vs2026-x64-unc` tree built and packaged successfully.

Install note: Attempted to install the fresh package into
`C:\Program Files\Notepad++\plugins\MarkdownPlusPlus`, but this PowerShell
session was not elevated and Windows denied writes under `Program Files`.
Notepad++ was not running. Package remains ready for admin install/manual smoke
test.

User rebuilt/installed with:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-native.ps1 -Configuration Release -Install
```

User confirmed the retest worked. Treat the external-link Edge-profile
regression as fixed and accepted for the native release-candidate smoke pass.

### 2026-04-30 native baseline checkpoint

User asked to make the checkpoint after the accepted retest.

Checkpoint scope:

- Native C++ `Markdown++` source and assets.
- CMake root/native build configuration.
- Native dependency/build/install helper scripts.
- Native build, dependency, release, backlog, and smoke-test documentation.
- Source smoke-test Markdown/SVG fixtures.
- Session/context notes needed to preserve the rewrite state.
- `.gitignore` rules for generated build output, repo-local dependency
  installs/caches, generated smoke outputs, and local AI-assistant files.

Generated/local files intentionally left out:

- `build/`
- `packages/`
- `third_party/cmark-gfm/`
- `MarkdownPlusPlus.Native/assets/mermaid/mermaid.min.js`
- `tools/nuget/`
- `tools/__pycache__/`
- generated `smoke-tests/*.html`
- generated `smoke-tests/*.pdf`
- copied smoke files and `*:Zone.Identifier`
- local assistant instruction files ignored by `.gitignore`

Validation before commit:

- `git diff --check -- . ':!third_party'`
- `cmake --build build/cmake/linux-check --config Release`
- `node --check MarkdownPlusPlus.Native/assets/preview.js`

Next after checkpoint: proceed with the planned native-only legacy cleanup
phase, starting by removing the old C# project/build entry points and replacing
root documentation with native-only docs.
