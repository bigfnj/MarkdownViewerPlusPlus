#include "MarkdownRenderer.h"

#include "HtmlUtil.h"
#include "WinUtil.h"

#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"
#include "cmark-gfm-core-extensions.h"

#include <array>
#include <utility>

namespace markdownplusplus {
namespace {

constexpr wchar_t kDocumentBaseUri[] = L"https://markdownplusplus.document/";

std::wstring DirectoryPathToFileUri(std::wstring path) {
    if (path.empty()) {
        return {};
    }

    wchar_t last = path.back();
    if (last != L'\\' && last != L'/') {
        path.push_back(L'\\');
    }

    return PathToFileUri(path);
}

void AttachExtension(cmark_parser* parser, const char* name) {
    cmark_syntax_extension* extension = cmark_find_syntax_extension(name);
    if (extension) {
        cmark_parser_attach_syntax_extension(parser, extension);
    }
}

std::string RenderMarkdownToHtml(const std::string& markdown) {
    cmark_gfm_core_extensions_ensure_registered();

    constexpr int options = CMARK_OPT_DEFAULT |
                            CMARK_OPT_SOURCEPOS |
                            CMARK_OPT_VALIDATE_UTF8 |
                            CMARK_OPT_GITHUB_PRE_LANG |
                            CMARK_OPT_FOOTNOTES |
                            CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE;

    cmark_mem* mem = cmark_get_default_mem_allocator();
    cmark_parser* parser = cmark_parser_new(options);
    if (!parser) {
        return "<p>Markdown++ could not initialize the Markdown parser.</p>";
    }

    const std::array<const char*, 5> extensions = {
        "table",
        "strikethrough",
        "autolink",
        "tagfilter",
        "tasklist",
    };

    for (const char* extension : extensions) {
        AttachExtension(parser, extension);
    }

    cmark_parser_feed(parser, markdown.data(), markdown.size());
    cmark_node* document = cmark_parser_finish(parser);
    if (!document) {
        cmark_parser_free(parser);
        return "<p>Markdown++ could not parse this document.</p>";
    }

    char* rendered = cmark_render_html(document, options, cmark_parser_get_syntax_extensions(parser));
    std::string html = rendered ? rendered : "";

    if (rendered) {
        mem->free(rendered);
    }

    cmark_node_free(document);
    cmark_parser_free(parser);
    return html;
}

}  // namespace

PreviewRenderResult MarkdownRenderer::BuildPreview(const PreviewDocumentRequest& request) {
    PreviewRenderResult result;
    result.title = request.title.empty() ? L"Markdown++" : request.title;

    const std::wstring escapedTitle = EscapeHtml(result.title);
    const std::wstring renderedHtml = Utf8ToWide(RenderMarkdownToHtml(request.markdownUtf8));
    const std::wstring baseUri = request.baseDirectory.empty() ? L"" : kDocumentBaseUri;
    result.articleHtml = renderedHtml;

    std::wstring document;
    document.reserve(renderedHtml.size() + 2048);
    document += L"<!doctype html><html><head><meta charset=\"utf-8\">";
    document += L"<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; img-src data: file: https: https://markdownplusplus.document; style-src https://markdownplusplus.local 'unsafe-inline'; script-src https://markdownplusplus.local;\">";
    document += L"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    if (!baseUri.empty()) {
        document += L"<base href=\"";
        document += EscapeHtml(baseUri);
        document += L"\">";
    }
    document += L"<title>";
    document += escapedTitle;
    document += L"</title><link rel=\"stylesheet\" href=\"https://markdownplusplus.local/preview.css\">";
    document += L"</head><body><article class=\"markdown-body\">";
    document += renderedHtml;
    document += L"</article>";
    document += L"<script src=\"https://markdownplusplus.local/mermaid/mermaid.min.js\"></script>";
    document += L"<script>window.MarkdownPlusPlusOptions={mermaidEnabled:";
    document += request.mermaidEnabled ? L"true" : L"false";
    document += L"};</script>";
    document += L"<script src=\"https://markdownplusplus.local/preview.js\"></script>";
    document += L"</body></html>";
    result.document = std::move(document);
    return result;
}

std::wstring MarkdownRenderer::BuildDocument(const PreviewDocumentRequest& request) {
    return BuildPreview(request).document;
}

std::wstring MarkdownRenderer::BuildStandaloneDocument(const PreviewDocumentRequest& request) {
    PreviewRenderResult rendered = BuildPreview(request);
    const std::wstring escapedTitle = EscapeHtml(rendered.title);
    const std::wstring assetsRoot = CombinePath(request.assetRoot, L"assets");
    const std::wstring css = ReadUtf8FileAsWide(CombinePath(assetsRoot, L"preview.css"));
    const std::wstring baseUri = DirectoryPathToFileUri(request.baseDirectory);

    std::wstring document;
    document.reserve(rendered.articleHtml.size() + css.size() + 2048);
    document += L"<!doctype html><html><head><meta charset=\"utf-8\">";
    document += L"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    if (!baseUri.empty()) {
        document += L"<base href=\"";
        document += EscapeHtml(baseUri);
        document += L"\">";
    }
    document += L"<title>";
    document += escapedTitle;
    document += L"</title>";
    if (!css.empty()) {
        document += L"<style>";
        document += css;
        document += L"</style>";
    }
    document += L"</head><body><article class=\"markdown-body\">";
    document += rendered.articleHtml;
    document += L"</article>";

    const std::wstring previewUri = PathToFileUri(CombinePath(assetsRoot, L"preview.js"));
    document += L"<script>window.MarkdownPlusPlusOptions={mermaidEnabled:";
    document += request.mermaidEnabled ? L"true" : L"false";
    document += L"};</script>";
    if (request.mermaidEnabled) {
        const std::wstring mermaidUri = PathToFileUri(CombinePath(assetsRoot, L"mermaid\\mermaid.min.js"));
        if (!mermaidUri.empty()) {
            document += L"<script src=\"";
            document += EscapeHtml(mermaidUri);
            document += L"\"></script>";
        }
    }
    if (!previewUri.empty()) {
        document += L"<script src=\"";
        document += EscapeHtml(previewUri);
        document += L"\"></script>";
    }

    document += L"</body></html>";
    return document;
}

}  // namespace markdownplusplus
