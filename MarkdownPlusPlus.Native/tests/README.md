# Markdown++ native unit tests

The first automated tests in this repo. They cover the parts of the plugin that
are pure functions, and nothing else.

## Running them

From the repository root. The system CMake (3.31) cannot configure this project;
`CMakePresets.json` needs 4.2+ and the `Visual Studio 18 2026` generator, so use
the CMake that ships with Visual Studio:

```powershell
$env:PATH = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;' + $env:PATH
cmake --preset vs2026-x64
cmake --build build/cmake/vs2026-x64 --config Release --target MarkdownPlusPlus_tests
ctest --test-dir build/cmake/vs2026-x64 -C Release --output-on-failure
```

Building the test target does not build the plugin DLL, so the suite still runs
while `PluginController` / `WebViewHost` are mid-edit.

The test binary also runs standalone, which is the quickest loop when you are
iterating on one thing:

```powershell
build\cmake\vs2026-x64\out\Release\tests\MarkdownPlusPlus_tests.exe            # everything
build\cmake\vs2026-x64\out\Release\tests\MarkdownPlusPlus_tests.exe renderer.  # one suite
build\cmake\vs2026-x64\out\Release\tests\MarkdownPlusPlus_tests.exe EmptyInput # one test
build\cmake\vs2026-x64\out\Release\tests\MarkdownPlusPlus_tests.exe --known-bugs
```

The argument is a substring matched against `suite.TestName`. A filter that
matches nothing exits 2 rather than reporting success, so a renamed suite cannot
quietly stop running. Pass `-DMARKDOWNPP_BUILD_TESTS=OFF` to leave the test
target out of the build entirely.

## Layout

| File | Contents |
| --- | --- |
| `TestHarness.h` | The whole framework. Registration, assertions, the runner. |
| `TempFiles.h` | Scratch directory under `%TEMP%`, removed in the destructor. |
| `MarkdownRendererTests.cpp` | `BuildPreview` / `BuildDocument` / `BuildStandaloneDocument`. |
| `PluginOptionsTests.cpp` | `PluginOptionsStore` round-trips and `ClampRenderDebounceMs`. |
| `WinUtilTests.cpp` | UTF-8 conversion, path joining, file URIs, file IO. |
| `HtmlUtilTests.cpp` | `EscapeHtml` and `EscapeScriptString`. |

No third-party test framework. `TestHarness.h` is a couple of hundred lines,
which is cheaper than adding a configure-time network fetch to a project that
deliberately keeps `third_party/` to one entry.

The production sources are compiled once, into the `markdownplusplus_core`
OBJECT library, and linked by both `MarkdownPlusPlus` (the DLL) and
`MarkdownPlusPlus_tests`. There is no second source list to drift.

## What is covered

- Rendering: headings, inline markup, tables, task lists, strikethrough,
  autolinks, fenced code with a language, Mermaid fences, Unicode including
  astral-plane characters, and a 2000-section document.
- The empty and whitespace-only documents, which produce an empty `<article>`
  inside a complete HTML shell. That state is what a blank preview looks like.
- HTML escaping of the title, and the fact that raw `<script>` in the source
  document is dropped by cmark rather than passed into the WebView.
- The `mermaidEnabled` flag changing only the client-side switch, not the
  rendered article.
- `PluginOptionsStore` against a real `.ini`: round-trip, defaults when the file
  or the key is absent, the debounce clamp on both the read and the write path,
  and the section name.
- `Utf8ToWide` / `WideToUtf8` including embedded NULs and invalid input,
  `CombinePath`, `PathToFileUri` (encoding, trailing slash, NUL trimming, and a
  path long enough to force its `E_POINTER` retry), `ReadUtf8FileAsWide` /
  `WriteUtf8File`, `GetModuleDirectory`.

## What is NOT covered

**`PluginController` and `WebViewHost` have no tests at all.** They need a live
Notepad++ host (`NppData`, Scintilla messages, docking windows) and a live
WebView2 environment, neither of which exists in a console process. Everything
those two classes own is therefore untested:

- the render pipeline end to end, including the debounce timer and whether an
  edit actually reaches the preview;
- `SetVirtualHostNameToFolderMapping`, navigation, and `ExecuteScript`;
- scroll synchronisation in either direction;
- the Notepad++ menu, toolbar and docking integration;
- `PreviewWindow` and every `HWND`-shaped path;
- `ClipboardUtil` (needs a window handle and the clipboard).

**A green run here does not mean the preview works.** The suite can pass while
the preview is blank, frozen, or never navigates. `SMOKE_CHECKLIST.md` is still
the only thing that checks that, and it is still manual.

Also not covered: `preview.js` and `preview.css` (no JavaScript test runner in
this repo), the installer, and anything to do with 32-bit or non-Windows builds.

Two things inside the units that *are* covered still have no reachable test:

- `PathToFileUri`'s `FAILED(result)` branch. `UrlCreateFromPathW` succeeded on
  every input tried against it, including relative paths, UNC paths, device
  paths, control characters and a 6000-character path, so nothing in the suite
  reaches that `return {}`. Mutating it to `return path` produces no failure.
- `CombinePath`'s overflow behaviour. Its buffer is `MAX_PATH`, and when
  `left + "\" + right` would exceed it `PathAppendW` fails and leaves the buffer
  **empty** - so the function returns `""`, not a truncated path. No assertion
  pins that, because pinning it would freeze a bug rather than a contract.

## Known product bugs parked in the suite

`MDPP_KNOWN_BUG_TEST` marks a test that asserts the *correct* behaviour of
something that is broken today. Its checks are expected to fail, so the run stays
green, but the runner prints a `KNOWN PRODUCT BUG` banner every time and the
`suite_carries_known_product_bugs` CTest entry keeps the count visible in CI
output. If the underlying bug is fixed, the test turns **red** with a message
telling you to promote it to a plain `MDPP_TEST` - a parked test cannot rot into
a permanent exception.

**Currently parked: none.** `MarkdownPlusPlus_tests --known-bugs` says so
explicitly rather than printing nothing, so the CTest entry that watches for that
banner stays green on the clean end state and goes red only on silence.

The mechanism has been through one full cycle already. The suite's first run
found that `WinUtil::PathToFileUri` returned an empty string for *every* input -
its sizing probe called `UrlCreateFromPathW` with a null output buffer, which
answers `E_INVALIDARG` (0x80070057) rather than the `E_POINTER` the guard
demanded, so the early return always fired. Exported HTML therefore had no
`preview.js`, no `mermaid.min.js` and no `<base href>`, with no error anywhere.
Three tests were parked against it; the fix landed the same day and all three
were promoted.

## Adding a test

```cpp
MDPP_TEST(renderer, SomethingSpecific) {
    MDPP_CHECK(condition);
    MDPP_CHECK_EQ(actual, expected);
    MDPP_CHECK_CONTAINS(haystack, needle);
    MDPP_CHECK_NOT_CONTAINS(haystack, needle);
}
```

Checks are non-fatal: a test reports every failing check, not just the first.
`std::string` and `std::wstring` operands are printed with non-ASCII escaped, so
failure output is readable regardless of the console code page.

A new suite name needs a matching entry in the `foreach` in `CMakeLists.txt`,
otherwise it builds and never runs under `ctest`.

**Break the code before you trust a new test.** Every assertion in here was
verified by mutating the production source until it failed. A test that passes no
matter what the code does is worse than no test, because it reads like coverage.
