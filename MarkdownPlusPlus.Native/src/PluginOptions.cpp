#include "PluginOptions.h"

#include <windows.h>

#include <algorithm>
#include <utility>

namespace markdownplusplus {
namespace {

constexpr wchar_t kSection[] = L"MarkdownPlusPlus";
constexpr wchar_t kMermaidEnabled[] = L"MermaidEnabled";
constexpr wchar_t kScrollSyncEnabled[] = L"ScrollSyncEnabled";
constexpr wchar_t kAutoOpenMarkdown[] = L"AutoOpenMarkdown";
constexpr wchar_t kRenderDebounceMs[] = L"RenderDebounceMs";

bool ReadBool(const std::wstring& path, const wchar_t* key, bool fallback) {
    return GetPrivateProfileIntW(kSection, key, fallback ? 1 : 0, path.c_str()) != 0;
}

void WriteBool(const std::wstring& path, const wchar_t* key, bool value) {
    WritePrivateProfileStringW(kSection, key, value ? L"1" : L"0", path.c_str());
}

void WriteInt(const std::wstring& path, const wchar_t* key, int value) {
    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%d", value);
    WritePrivateProfileStringW(kSection, key, buffer, path.c_str());
}

}  // namespace

PluginOptionsStore::PluginOptionsStore(std::wstring path)
    : path_(std::move(path)) {}

PluginOptions PluginOptionsStore::Load() const {
    PluginOptions options;
    if (path_.empty()) {
        return options;
    }

    options.mermaidEnabled = ReadBool(path_, kMermaidEnabled, options.mermaidEnabled);
    options.scrollSyncEnabled = ReadBool(path_, kScrollSyncEnabled, options.scrollSyncEnabled);
    options.autoOpenMarkdown = ReadBool(path_, kAutoOpenMarkdown, options.autoOpenMarkdown);
    options.renderDebounceMs = ClampRenderDebounceMs(
        GetPrivateProfileIntW(kSection, kRenderDebounceMs, options.renderDebounceMs, path_.c_str()));
    return options;
}

void PluginOptionsStore::Save(const PluginOptions& options) const {
    if (path_.empty()) {
        return;
    }

    WriteBool(path_, kMermaidEnabled, options.mermaidEnabled);
    WriteBool(path_, kScrollSyncEnabled, options.scrollSyncEnabled);
    WriteBool(path_, kAutoOpenMarkdown, options.autoOpenMarkdown);
    WriteInt(path_, kRenderDebounceMs, ClampRenderDebounceMs(options.renderDebounceMs));
}

int ClampRenderDebounceMs(int value) {
    return std::clamp(value, 0, 1000);
}

}  // namespace markdownplusplus
