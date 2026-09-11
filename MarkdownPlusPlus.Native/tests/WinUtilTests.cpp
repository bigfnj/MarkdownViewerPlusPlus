// UTF-8 <-> UTF-16 conversion and path/URI helpers. Every string that reaches the
// WebView goes through Utf8ToWide, and every asset reference goes through
// CombinePath + PathToFileUri, so a regression here shows up as a blank preview
// or a missing stylesheet rather than as a crash.
//
// Non-ASCII is written with explicit \x / \u escapes so the tests do not depend
// on the source-file encoding MSVC happens to assume.
#include "TestHarness.h"
#include "TempFiles.h"

#include "WinUtil.h"

#include <string>

using markdownplusplus::CombinePath;
using markdownplusplus::GetModuleDirectory;
using markdownplusplus::PathToFileUri;
using markdownplusplus::ReadUtf8FileAsWide;
using markdownplusplus::Utf8ToWide;
using markdownplusplus::WideToUtf8;
using markdownplusplus::WriteUtf8File;

MDPP_TEST(winutil, Utf8RoundTripAscii) {
    const std::string utf8 = "Hello, world! <b>&amp;</b>";
    const std::wstring wide = Utf8ToWide(utf8);

    MDPP_CHECK_EQ(wide, std::wstring(L"Hello, world! <b>&amp;</b>"));
    MDPP_CHECK_EQ(WideToUtf8(wide), utf8);
}

MDPP_TEST(winutil, Utf8RoundTripNonAscii) {
    struct Pair { const char* utf8; const wchar_t* wide; size_t wideChars; };
    const Pair pairs[] = {
        {"caf\xC3\xA9", L"caf\u00E9", 4},                              // Latin-1 supplement
        {"\xE4\xB8\xAD" "\xE6\x96\x87", L"\u4E2D\u6587", 2},           // CJK, 3-byte
        {"\xF0\x9F\x98\x80", L"\U0001F600", 2},                        // emoji, surrogate pair
        {"\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82", L"\u041F\u0440\u0438\u0432\u0435\u0442", 6},
    };

    for (const Pair& pair : pairs) {
        const std::wstring wide = Utf8ToWide(pair.utf8);
        MDPP_CHECK_EQ(wide, std::wstring(pair.wide));
        MDPP_CHECK_EQ(wide.size(), pair.wideChars);
        MDPP_CHECK_EQ(WideToUtf8(wide), std::string(pair.utf8));
    }
}

MDPP_TEST(winutil, EmbeddedNulsSurviveBothDirections) {
    const std::string utf8("a\0b", 3);
    MDPP_CHECK_EQ(utf8.size(), static_cast<size_t>(3));

    const std::wstring wide = Utf8ToWide(utf8);
    MDPP_CHECK_EQ(wide.size(), static_cast<size_t>(3));
    MDPP_CHECK(wide[0] == L'a');
    MDPP_CHECK(wide[1] == L'\0');
    MDPP_CHECK(wide[2] == L'b');

    const std::string back = WideToUtf8(wide);
    MDPP_CHECK_EQ(back.size(), static_cast<size_t>(3));
    MDPP_CHECK_EQ(back, utf8);
}

MDPP_TEST(winutil, EmptyConversionsAreEmpty) {
    MDPP_CHECK(Utf8ToWide(std::string()).empty());
    MDPP_CHECK(WideToUtf8(std::wstring()).empty());
}

MDPP_TEST(winutil, InvalidUtf8FallsBackInsteadOfReturningEmpty) {
    // 0xC3 starts a 2-byte sequence, '(' is not a valid continuation byte. The
    // strict MB_ERR_INVALID_CHARS pass fails and the ACP fallback takes over; the
    // contract that matters is "never silently return an empty document".
    const std::wstring wide = Utf8ToWide(std::string("\xC3\x28", 2));
    MDPP_CHECK(!wide.empty());
    MDPP_CHECK_EQ(wide.size(), static_cast<size_t>(2));
    MDPP_CHECK(wide[1] == L'(');
}

MDPP_TEST(winutil, CombinePathJoinsWithExactlyOneSeparator) {
    MDPP_CHECK_EQ(CombinePath(L"C:\\a", L"b"), std::wstring(L"C:\\a\\b"));
    MDPP_CHECK_EQ(CombinePath(L"C:\\a\\", L"b"), std::wstring(L"C:\\a\\b"));
    MDPP_CHECK_EQ(CombinePath(L"C:\\a", L"b\\c"), std::wstring(L"C:\\a\\b\\c"));
    MDPP_CHECK_EQ(CombinePath(L"C:\\a\\", L"\\b"), std::wstring(L"C:\\a\\b"));
    MDPP_CHECK_EQ(CombinePath(L"", L"b"), std::wstring(L"b"));
    MDPP_CHECK_EQ(CombinePath(L"C:\\a", L""), std::wstring(L"C:\\a"));
    MDPP_CHECK_EQ(CombinePath(L"", L""), std::wstring(L""));
}

// Was MDPP_KNOWN_BUG_TEST until 2026-09-11. The old sizing probe passed a null buffer to
// UrlCreateFromPathW, which answers E_INVALIDARG rather than the E_POINTER the guard demanded,
// so this returned "" for every input. Fixed by probing with a real buffer; now a real guard.
MDPP_TEST(winutil, PathToFileUriEncodesAndNormalises) {
    MDPP_CHECK(PathToFileUri(L"").empty());

    MDPP_CHECK_EQ(PathToFileUri(L"C:\\temp\\a.html"), std::wstring(L"file:///C:/temp/a.html"));
    // A space must be percent-encoded or the <script src> in a standalone export
    // silently resolves to nothing.
    MDPP_CHECK_EQ(PathToFileUri(L"C:\\temp\\a b.html"), std::wstring(L"file:///C:/temp/a%20b.html"));
    MDPP_CHECK_CONTAINS(PathToFileUri(L"C:\\temp\\a#b.html"), std::wstring(L"%23"));

    // The result must be trimmed at the first NUL, not padded to the buffer size.
    const std::wstring uri = PathToFileUri(L"C:\\temp\\a.html");
    MDPP_CHECK_EQ(uri.size(), std::wstring(L"file:///C:/temp/a.html").size());

    // A directory keeps its trailing slash, which is what makes <base href> work.
    MDPP_CHECK_EQ(PathToFileUri(L"C:\\temp\\dir\\"), std::wstring(L"file:///C:/temp/dir/"));
}

MDPP_TEST(winutil, PathToFileUriGrowsPastItsInitialBuffer) {
    // Longer than kInitialUrlChars, so this only comes back whole if the E_POINTER
    // retry runs. Without it the first call fails and the URI is silently empty -
    // the same failure mode as the null-buffer probe that shipped before.
    const std::wstring longPath = L"C:\\" + std::wstring(3000, L'd') + L"\\a.html";
    const std::wstring uri = PathToFileUri(longPath);

    MDPP_CHECK(!uri.empty());
    MDPP_CHECK(uri.size() > 3000);
    MDPP_CHECK(uri.rfind(L"file:///C:/", 0) == 0);
    MDPP_CHECK_CONTAINS(uri, std::wstring(L"/a.html"));
    // Trimmed at the NUL, not padded out to whatever the retry allocated.
    MDPP_CHECK(uri.find(L'\0') == std::wstring::npos);
}

MDPP_TEST(winutil, FileRoundTripThroughUtf8) {
    mdpptest::TempDir temp;
    const std::wstring path = temp.File(L"round.txt");
    const std::string utf8 = "line1\ncaf\xC3\xA9" "\n\xE4\xB8\xAD\xE6\x96\x87";

    MDPP_CHECK(WriteUtf8File(path, utf8));
    MDPP_CHECK(mdpptest::TempDir::Exists(path));
    MDPP_CHECK_EQ(ReadUtf8FileAsWide(path), Utf8ToWide(utf8));
    MDPP_CHECK_CONTAINS(ReadUtf8FileAsWide(path), std::wstring(L"caf\u00E9"));

    MDPP_CHECK(WriteUtf8File(path, std::string()));
    MDPP_CHECK(ReadUtf8FileAsWide(path).empty());
}

MDPP_TEST(winutil, ReadingAMissingFileYieldsEmptyRatherThanThrowing) {
    mdpptest::TempDir temp;
    const std::wstring path = temp.File(L"does-not-exist.txt");
    MDPP_CHECK(!mdpptest::TempDir::Exists(path));
    MDPP_CHECK(ReadUtf8FileAsWide(path).empty());
}

MDPP_TEST(winutil, ModuleDirectoryStripsTheFileName) {
    const std::wstring directory = GetModuleDirectory(nullptr);

    MDPP_CHECK(!directory.empty());
    MDPP_CHECK(mdpptest::TempDir::Exists(directory));
    MDPP_CHECK_NOT_CONTAINS(directory, std::wstring(L".exe"));
    MDPP_CHECK(directory.back() != L'\\');
    // assets/ is resolved relative to this, so it must be a directory that the
    // plugin can append to.
    MDPP_CHECK_EQ(CombinePath(directory, L"assets"), directory + L"\\assets");
}
