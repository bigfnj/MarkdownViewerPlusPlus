#pragma once

#include <string>

namespace markdownplusplus {

struct PluginOptions {
    bool mermaidEnabled = true;
    bool scrollSyncEnabled = true;
    bool autoOpenMarkdown = true;
    int renderDebounceMs = 140;
};

class PluginOptionsStore {
public:
    explicit PluginOptionsStore(std::wstring path = {});

    const std::wstring& Path() const { return path_; }
    PluginOptions Load() const;
    void Save(const PluginOptions& options) const;

private:
    std::wstring path_;
};

int ClampRenderDebounceMs(int value);

}  // namespace markdownplusplus
