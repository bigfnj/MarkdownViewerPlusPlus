// MarkdownRenderer is the one production unit with no HWND, no Notepad++ and no
// WebView2 in its signature: plain strings in, three wstrings out. It is also
// the unit that decides whether the preview shows anything at all.
#include "TestHarness.h"
#include "TempFiles.h"

#include "MarkdownRenderer.h"
#include "WinUtil.h"

#include <string>

using markdownplusplus::MarkdownRenderer;
using markdownplusplus::PreviewDocumentRequest;
using markdownplusplus::PreviewRenderResult;

namespace {

PreviewDocumentRequest Request(const std::string& markdown) {
    PreviewDocumentRequest request;
    request.markdownUtf8 = markdown;
    request.title = L"Doc";
    return request;
}

}  // namespace

MDPP_TEST(renderer, HeadingsAndInlineMarkup) {
    const PreviewRenderResult result =
        MarkdownRenderer::BuildPreview(Request("# Title\n\nHello **world** and _stress_.\n"));

    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"<h1"));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L">Title</h1>"));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"<strong>world</strong>"));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"<em>stress</em>"));
    // CMARK_OPT_SOURCEPOS is what lets preview.js map a scroll position back to a
    // source line. Losing it silently breaks scroll sync.
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"data-sourcepos="));
}

MDPP_TEST(renderer, GfmTable) {
    const PreviewRenderResult result = MarkdownRenderer::BuildPreview(
        Request("| Col A | Col B |\n| --- | --- |\n| one | two |\n"));

    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"<table"));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L">Col A</th>"));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L">one</td>"));
}

MDPP_TEST(renderer, GfmTaskList) {
    const PreviewRenderResult result =
        MarkdownRenderer::BuildPreview(Request("- [x] done\n- [ ] todo\n"));

    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"<input type=\"checkbox\""));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"checked=\"\""));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"done"));
}

MDPP_TEST(renderer, GfmStrikethroughAndAutolink) {
    const PreviewRenderResult result =
        MarkdownRenderer::BuildPreview(Request("~~gone~~ and https://example.com/x\n"));

    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"<del>gone</del>"));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"<a href=\"https://example.com/x\""));
}

MDPP_TEST(renderer, FencedCodeKeepsLanguageAndEscapesBody) {
    const PreviewRenderResult result = MarkdownRenderer::BuildPreview(
        Request("```cpp\nif (a < b) { call(\"<script>\"); }\n```\n"));

    // CMARK_OPT_GITHUB_PRE_LANG puts the language on <pre>, which is what
    // preview.css and the Mermaid pass in preview.js both key off.
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"lang=\"cpp\""));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"<code>"));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"a &lt; b"));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"&lt;script&gt;"));
}

// An empty article is exactly the state that shows up as a blank preview, so the
// shell around it has to survive an empty document.
MDPP_TEST(renderer, EmptyInputStillProducesACompleteShell) {
    const PreviewRenderResult result = MarkdownRenderer::BuildPreview(Request(""));

    MDPP_CHECK(result.articleHtml.empty());
    MDPP_CHECK(!result.document.empty());
    MDPP_CHECK(result.document.size() > 400);
    MDPP_CHECK_CONTAINS(result.document, std::wstring(L"<!doctype html>"));
    MDPP_CHECK_CONTAINS(result.document, std::wstring(L"<article class=\"markdown-body\"></article>"));
    MDPP_CHECK_CONTAINS(result.document, std::wstring(L"https://markdownplusplus.local/preview.css"));
    MDPP_CHECK_CONTAINS(result.document, std::wstring(L"https://markdownplusplus.local/preview.js"));
    MDPP_CHECK_CONTAINS(result.document, std::wstring(L"</body></html>"));
    MDPP_CHECK_EQ(result.title, std::wstring(L"Doc"));
}

MDPP_TEST(renderer, WhitespaceOnlyInputBehavesLikeEmpty) {
    const PreviewRenderResult blank = MarkdownRenderer::BuildPreview(Request("   \n\t\n  \r\n"));
    const PreviewRenderResult empty = MarkdownRenderer::BuildPreview(Request(""));

    MDPP_CHECK(blank.articleHtml.empty());
    MDPP_CHECK_EQ(blank.document, empty.document);
}

MDPP_TEST(renderer, MermaidFlagOnlyChangesTheClientSideSwitch) {
    PreviewDocumentRequest on = Request("```mermaid\ngraph TD; A-->B;\n```\n");
    on.mermaidEnabled = true;
    PreviewDocumentRequest off = on;
    off.mermaidEnabled = false;

    const PreviewRenderResult enabled = MarkdownRenderer::BuildPreview(on);
    const PreviewRenderResult disabled = MarkdownRenderer::BuildPreview(off);

    MDPP_CHECK_CONTAINS(enabled.articleHtml, std::wstring(L"lang=\"mermaid\""));
    MDPP_CHECK_CONTAINS(enabled.document, std::wstring(L"mermaidEnabled:true"));
    MDPP_CHECK_CONTAINS(disabled.document, std::wstring(L"mermaidEnabled:false"));
    MDPP_CHECK_NOT_CONTAINS(enabled.document, std::wstring(L"mermaidEnabled:false"));
    MDPP_CHECK_NOT_CONTAINS(disabled.document, std::wstring(L"mermaidEnabled:true"));

    // The fence is rendered identically either way; only the runtime flag moves.
    MDPP_CHECK_EQ(enabled.articleHtml, disabled.articleHtml);
    // mermaid.min.js is always loaded in the preview; preview.js decides whether
    // to run it. Dropping the tag would break the toggle without any other signal.
    MDPP_CHECK_CONTAINS(disabled.document, std::wstring(L"mermaid/mermaid.min.js"));
}

MDPP_TEST(renderer, RawScriptTagIsNeutralised) {
    const PreviewRenderResult result = MarkdownRenderer::BuildPreview(
        Request("Before <script>alert('pwned')</script> after. AT&T\n"));

    // CMARK_OPT_UNSAFE is deliberately absent, so cmark drops raw HTML entirely
    // rather than escaping it. Adding CMARK_OPT_UNSAFE would put a live <script>
    // from the document into the same WebView that the plugin scripts.
    MDPP_CHECK_NOT_CONTAINS(result.articleHtml, std::wstring(L"<script>alert"));
    MDPP_CHECK_NOT_CONTAINS(result.document, std::wstring(L"<script>alert"));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"<!-- raw HTML omitted -->"));
    // The tag is dropped; its text content stays as inert text.
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"-->alert('pwned')<!--"));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"AT&amp;T"));
    // The Content-Security-Policy is the second line of defence for the same
    // problem and lives in the same string builder.
    MDPP_CHECK_CONTAINS(result.document, std::wstring(L"Content-Security-Policy"));
    MDPP_CHECK_CONTAINS(result.document, std::wstring(L"default-src 'none'"));
}

MDPP_TEST(renderer, TitleDefaultsAndIsHtmlEscaped) {
    PreviewDocumentRequest untitled = Request("body\n");
    untitled.title = L"";
    const PreviewRenderResult fallback = MarkdownRenderer::BuildPreview(untitled);
    MDPP_CHECK_EQ(fallback.title, std::wstring(L"Markdown++"));
    MDPP_CHECK_CONTAINS(fallback.document, std::wstring(L"<title>Markdown++</title>"));

    PreviewDocumentRequest hostile = Request("body\n");
    hostile.title = L"a<b>&\"c\"";
    const PreviewRenderResult escaped = MarkdownRenderer::BuildPreview(hostile);
    MDPP_CHECK_EQ(escaped.title, std::wstring(L"a<b>&\"c\""));
    MDPP_CHECK_CONTAINS(escaped.document, std::wstring(L"<title>a&lt;b&gt;&amp;&quot;c&quot;</title>"));
    MDPP_CHECK_NOT_CONTAINS(escaped.document, std::wstring(L"<title>a<b>"));
}

MDPP_TEST(renderer, BaseHrefOnlyAppearsWithABaseDirectory) {
    PreviewDocumentRequest none = Request("![x](img/a.png)\n");
    const PreviewRenderResult without = MarkdownRenderer::BuildPreview(none);
    MDPP_CHECK_NOT_CONTAINS(without.document, std::wstring(L"<base href="));

    PreviewDocumentRequest based = none;
    based.baseDirectory = L"C:\\notes";
    const PreviewRenderResult with = MarkdownRenderer::BuildPreview(based);
    MDPP_CHECK_CONTAINS(with.document, std::wstring(L"<base href=\"https://markdownplusplus.document/\">"));
    // The virtual host name is shared with the CSP img-src entry above it.
    MDPP_CHECK_CONTAINS(with.document, std::wstring(L"img-src data: file: https: https://markdownplusplus.document"));
}

MDPP_TEST(renderer, UnicodeSurvivesTheUtf8ToWideHop) {
    const PreviewRenderResult result = MarkdownRenderer::BuildPreview(
        Request("caf\xC3\xA9" " \xE4\xB8\xAD\xE6\x96\x87" " \xF0\x9F\x98\x80\n"));

    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"caf\u00E9"));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"\u4E2D\u6587"));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L"\U0001F600"));
}

MDPP_TEST(renderer, LargeDocumentIsNotTruncated) {
    std::string markdown;
    markdown.reserve(400000);
    for (int i = 0; i < 2000; ++i) {
        markdown += "## Section " + std::to_string(i) + "\n\nBody **" + std::to_string(i) +
                    "** with `code` and a [link](https://example.com/" + std::to_string(i) + ").\n\n";
    }

    const PreviewRenderResult result = MarkdownRenderer::BuildPreview(Request(markdown));

    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L">Section 0</h2>"));
    MDPP_CHECK_CONTAINS(result.articleHtml, std::wstring(L">Section 1999</h2>"));
    MDPP_CHECK(result.articleHtml.size() > 300000);
    MDPP_CHECK(result.document.size() > result.articleHtml.size());
    MDPP_CHECK_CONTAINS(result.document, std::wstring(L"</body></html>"));
}

MDPP_TEST(renderer, BuildDocumentMatchesBuildPreview) {
    PreviewDocumentRequest request = Request("# Same\n\ntext\n");
    request.baseDirectory = L"C:\\notes";
    request.mermaidEnabled = false;

    MDPP_CHECK_EQ(MarkdownRenderer::BuildDocument(request),
                  MarkdownRenderer::BuildPreview(request).document);
}

namespace {

// Lays out assetRoot/assets/{preview.css,preview.js,mermaid/mermaid.min.js}.
void SeedAssets(const mdpptest::TempDir& temp) {
    const std::wstring assets = temp.MakeSubDir(L"assets");
    temp.MakeSubDir(L"assets\\mermaid");
    markdownplusplus::WriteUtf8File(assets + L"\\preview.css", "body{--marker:1}");
    markdownplusplus::WriteUtf8File(assets + L"\\preview.js", "/* preview */");
    markdownplusplus::WriteUtf8File(assets + L"\\mermaid\\mermaid.min.js", "/* mermaid */");
}

}  // namespace

MDPP_TEST(renderer, StandaloneDocumentInlinesCss) {
    mdpptest::TempDir temp;
    SeedAssets(temp);

    PreviewDocumentRequest request = Request("# Export\n\ntext\n");
    request.assetRoot = temp.Path();
    request.mermaidEnabled = true;

    const std::wstring html = MarkdownRenderer::BuildStandaloneDocument(request);

    // A standalone export must not depend on the WebView2 virtual host mapping.
    MDPP_CHECK_NOT_CONTAINS(html, std::wstring(L"https://markdownplusplus.local"));
    MDPP_CHECK_NOT_CONTAINS(html, std::wstring(L"Content-Security-Policy"));
    MDPP_CHECK_CONTAINS(html, std::wstring(L"<style>body{--marker:1}</style>"));
    MDPP_CHECK_CONTAINS(html, std::wstring(L">Export</h1>"));
    MDPP_CHECK_CONTAINS(html, std::wstring(L"mermaidEnabled:true"));
    MDPP_CHECK_CONTAINS(html, std::wstring(L"</body></html>"));

    PreviewDocumentRequest noMermaid = request;
    noMermaid.mermaidEnabled = false;
    MDPP_CHECK_CONTAINS(MarkdownRenderer::BuildStandaloneDocument(noMermaid),
                        std::wstring(L"mermaidEnabled:false"));
}

// Was MDPP_KNOWN_BUG_TEST until 2026-09-11: PathToFileUri returned "" for every input, so
// an exported HTML file shipped without preview.js or mermaid.min.js. Fixed; now a real guard.
MDPP_TEST(renderer, StandaloneDocumentLinksLocalAssets) {
    mdpptest::TempDir temp;
    SeedAssets(temp);

    PreviewDocumentRequest request = Request("```mermaid\ngraph TD; A-->B;\n```\n");
    request.assetRoot = temp.Path();
    request.mermaidEnabled = true;

    const std::wstring html = MarkdownRenderer::BuildStandaloneDocument(request);
    MDPP_CHECK_CONTAINS(html, std::wstring(L"file:///"));
    MDPP_CHECK_CONTAINS(html, std::wstring(L"preview.js\"></script>"));
    MDPP_CHECK_CONTAINS(html, std::wstring(L"mermaid.min.js\"></script>"));

    PreviewDocumentRequest noMermaid = request;
    noMermaid.mermaidEnabled = false;
    const std::wstring plain = MarkdownRenderer::BuildStandaloneDocument(noMermaid);
    MDPP_CHECK_CONTAINS(plain, std::wstring(L"preview.js\"></script>"));
    MDPP_CHECK_NOT_CONTAINS(plain, std::wstring(L"mermaid.min.js"));
}

MDPP_TEST(renderer, StandaloneDocumentToleratesMissingAssets) {
    mdpptest::TempDir temp;

    PreviewDocumentRequest request = Request("# Export\n\ntext\n");
    request.assetRoot = temp.Path();  // no assets/ subtree at all

    const std::wstring html = MarkdownRenderer::BuildStandaloneDocument(request);

    MDPP_CHECK_CONTAINS(html, std::wstring(L">Export</h1>"));
    MDPP_CHECK_NOT_CONTAINS(html, std::wstring(L"<style>"));
    MDPP_CHECK_CONTAINS(html, std::wstring(L"</body></html>"));
}

// Was MDPP_KNOWN_BUG_TEST until 2026-09-11: with PathToFileUri returning "", the exported
// document had no <base href> and every relative image broke. Fixed; now a real guard.
MDPP_TEST(renderer, StandaloneDocumentBaseHrefPointsAtTheDocumentFolder) {
    mdpptest::TempDir temp;

    PreviewDocumentRequest request = Request("![x](img/a.png)\n");
    request.assetRoot = temp.Path();
    request.baseDirectory = L"C:\\notes\\sub";

    const std::wstring html = MarkdownRenderer::BuildStandaloneDocument(request);

    // The trailing slash is what makes relative image paths resolve; without it
    // the last path segment is treated as a filename and every image 404s.
    // DirectoryPathToFileUri appends the separator before calling PathToFileUri.
    MDPP_CHECK_CONTAINS(html, std::wstring(L"<base href=\"file:///C:/notes/sub/\">"));

    PreviewDocumentRequest alreadySlashed = request;
    alreadySlashed.baseDirectory = L"C:\\notes\\sub\\";
    MDPP_CHECK_CONTAINS(MarkdownRenderer::BuildStandaloneDocument(alreadySlashed),
                        std::wstring(L"<base href=\"file:///C:/notes/sub/\">"));

    PreviewDocumentRequest rooted = request;
    rooted.baseDirectory = L"";
    MDPP_CHECK_NOT_CONTAINS(MarkdownRenderer::BuildStandaloneDocument(rooted), std::wstring(L"<base href="));
}
