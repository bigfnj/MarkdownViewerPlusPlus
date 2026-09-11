// PluginOptionsStore is a thin wrapper over GetPrivateProfile*/WritePrivateProfile*,
// so these tests round-trip through a real .ini in a scratch directory rather
// than mocking the Win32 API.
#include "TestHarness.h"
#include "TempFiles.h"

#include "PluginOptions.h"

#include <windows.h>

using markdownplusplus::ClampRenderDebounceMs;
using markdownplusplus::PluginOptions;
using markdownplusplus::PluginOptionsStore;

namespace {

void CheckEqualOptions(const PluginOptions& actual, const PluginOptions& expected) {
    MDPP_CHECK_EQ(actual.mermaidEnabled, expected.mermaidEnabled);
    MDPP_CHECK_EQ(actual.scrollSyncEnabled, expected.scrollSyncEnabled);
    MDPP_CHECK_EQ(actual.autoOpenMarkdown, expected.autoOpenMarkdown);
    MDPP_CHECK_EQ(actual.renderDebounceMs, expected.renderDebounceMs);
}

}  // namespace

MDPP_TEST(options, ClampRenderDebounceBounds) {
    MDPP_CHECK_EQ(ClampRenderDebounceMs(0), 0);
    MDPP_CHECK_EQ(ClampRenderDebounceMs(-1), 0);
    MDPP_CHECK_EQ(ClampRenderDebounceMs(-5000), 0);
    MDPP_CHECK_EQ(ClampRenderDebounceMs(1), 1);
    MDPP_CHECK_EQ(ClampRenderDebounceMs(140), 140);
    MDPP_CHECK_EQ(ClampRenderDebounceMs(1000), 1000);
    MDPP_CHECK_EQ(ClampRenderDebounceMs(1001), 1000);
    MDPP_CHECK_EQ(ClampRenderDebounceMs(2147483647), 1000);
}

MDPP_TEST(options, StructDefaults) {
    const PluginOptions defaults;
    MDPP_CHECK_EQ(defaults.mermaidEnabled, true);
    MDPP_CHECK_EQ(defaults.scrollSyncEnabled, true);
    MDPP_CHECK_EQ(defaults.autoOpenMarkdown, true);
    MDPP_CHECK_EQ(defaults.renderDebounceMs, 140);
}

MDPP_TEST(options, LoadReturnsDefaultsWhenTheFileIsAbsent) {
    mdpptest::TempDir temp;
    const std::wstring path = temp.File(L"absent.ini");
    MDPP_CHECK(!mdpptest::TempDir::Exists(path));

    const PluginOptionsStore store(path);
    MDPP_CHECK_EQ(store.Path(), path);
    CheckEqualOptions(store.Load(), PluginOptions{});
}

MDPP_TEST(options, EmptyPathLoadsDefaultsAndSavesNothing) {
    const PluginOptionsStore store;
    MDPP_CHECK(store.Path().empty());
    CheckEqualOptions(store.Load(), PluginOptions{});

    PluginOptions options;
    options.mermaidEnabled = false;
    options.renderDebounceMs = 500;
    store.Save(options);  // must be a no-op, not a write to the Windows directory
    CheckEqualOptions(store.Load(), PluginOptions{});
}

MDPP_TEST(options, RoundTripsEveryField) {
    // Two patterns, so that swapping any pair of the three flags fails at least
    // one of them.
    PluginOptions first;
    first.mermaidEnabled = false;
    first.scrollSyncEnabled = true;
    first.autoOpenMarkdown = true;
    first.renderDebounceMs = 250;

    PluginOptions second;
    second.mermaidEnabled = true;
    second.scrollSyncEnabled = true;
    second.autoOpenMarkdown = false;
    second.renderDebounceMs = 0;

    mdpptest::TempDir temp;
    int index = 0;
    for (const PluginOptions& expected : {first, second}) {
        const std::wstring path = temp.File(L"round" + std::to_wstring(index++) + L".ini");
        const PluginOptionsStore store(path);
        store.Save(expected);
        MDPP_CHECK(mdpptest::TempDir::Exists(path));
        CheckEqualOptions(store.Load(), expected);
    }
}

MDPP_TEST(options, SaveClampsBeforeWriting) {
    mdpptest::TempDir temp;
    const std::wstring path = temp.File(L"clamp-save.ini");
    const PluginOptionsStore store(path);

    PluginOptions tooSlow;
    tooSlow.renderDebounceMs = 99999;
    store.Save(tooSlow);
    // Read the raw key, bypassing Load()'s own clamp, so this fails if the clamp
    // moved out of Save().
    MDPP_CHECK_EQ(static_cast<int>(GetPrivateProfileIntW(L"MarkdownPlusPlus", L"RenderDebounceMs", 424242, path.c_str())), 1000);

    PluginOptions negative;
    negative.renderDebounceMs = -20;
    store.Save(negative);
    MDPP_CHECK_EQ(static_cast<int>(GetPrivateProfileIntW(L"MarkdownPlusPlus", L"RenderDebounceMs", 424242, path.c_str())), 0);
}

MDPP_TEST(options, LoadClampsAHandEditedFile) {
    mdpptest::TempDir temp;
    const std::wstring path = temp.File(L"clamp-load.ini");

    WritePrivateProfileStringW(L"MarkdownPlusPlus", L"RenderDebounceMs", L"99999", path.c_str());
    MDPP_CHECK_EQ(PluginOptionsStore(path).Load().renderDebounceMs, 1000);

    WritePrivateProfileStringW(L"MarkdownPlusPlus", L"RenderDebounceMs", L"0", path.c_str());
    MDPP_CHECK_EQ(PluginOptionsStore(path).Load().renderDebounceMs, 0);
}

MDPP_TEST(options, PartialFileKeepsDefaultsForMissingKeys) {
    mdpptest::TempDir temp;
    const std::wstring path = temp.File(L"partial.ini");

    WritePrivateProfileStringW(L"MarkdownPlusPlus", L"ScrollSyncEnabled", L"0", path.c_str());

    const PluginOptions loaded = PluginOptionsStore(path).Load();
    MDPP_CHECK_EQ(loaded.scrollSyncEnabled, false);
    MDPP_CHECK_EQ(loaded.mermaidEnabled, true);
    MDPP_CHECK_EQ(loaded.autoOpenMarkdown, true);
    MDPP_CHECK_EQ(loaded.renderDebounceMs, 140);
}

MDPP_TEST(options, KeysAreWrittenUnderTheExpectedSection) {
    mdpptest::TempDir temp;
    const std::wstring path = temp.File(L"section.ini");

    PluginOptions options;
    options.mermaidEnabled = false;
    PluginOptionsStore(path).Save(options);

    // A section rename would not be caught by a round-trip test on its own: it
    // stays self-consistent while silently discarding every existing user config.
    MDPP_CHECK_EQ(static_cast<int>(GetPrivateProfileIntW(L"MarkdownPlusPlus", L"MermaidEnabled", 424242, path.c_str())), 0);
    MDPP_CHECK_EQ(static_cast<int>(GetPrivateProfileIntW(L"MarkdownPlusPlus", L"ScrollSyncEnabled", 424242, path.c_str())), 1);
    MDPP_CHECK_EQ(static_cast<int>(GetPrivateProfileIntW(L"MarkdownPlusPlus", L"AutoOpenMarkdown", 424242, path.c_str())), 1);
}
