#pragma once

#include <string>

namespace markdownplusplus {

struct PreviewDocumentRequest {
    std::string markdownUtf8;
    std::wstring title;
    std::wstring assetRoot;
    std::wstring baseDirectory;
    bool mermaidEnabled = true;
};

struct PreviewRenderResult {
    std::wstring document;
    std::wstring articleHtml;
    std::wstring title;
};

class MarkdownRenderer {
public:
    static PreviewRenderResult BuildPreview(const PreviewDocumentRequest& request);
    static std::wstring BuildDocument(const PreviewDocumentRequest& request);
    static std::wstring BuildStandaloneDocument(const PreviewDocumentRequest& request);
};

}  // namespace markdownplusplus
