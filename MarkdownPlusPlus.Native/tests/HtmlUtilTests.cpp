// EscapeHtml guards attribute and <title> interpolation; EscapeScriptString guards
// the string literals the plugin injects into the WebView with ExecuteScript.
#include "TestHarness.h"

#include "HtmlUtil.h"

#include <string>

using markdownplusplus::EscapeHtml;
using markdownplusplus::EscapeScriptString;

MDPP_TEST(htmlutil, EscapeHtmlCoversAllFiveEntities) {
    MDPP_CHECK_EQ(EscapeHtml(L"&"), std::wstring(L"&amp;"));
    MDPP_CHECK_EQ(EscapeHtml(L"<"), std::wstring(L"&lt;"));
    MDPP_CHECK_EQ(EscapeHtml(L">"), std::wstring(L"&gt;"));
    MDPP_CHECK_EQ(EscapeHtml(L"\""), std::wstring(L"&quot;"));
    MDPP_CHECK_EQ(EscapeHtml(L"'"), std::wstring(L"&#39;"));
}

MDPP_TEST(htmlutil, EscapeHtmlLeavesOrdinaryTextAlone) {
    MDPP_CHECK_EQ(EscapeHtml(L""), std::wstring(L""));
    MDPP_CHECK_EQ(EscapeHtml(L"plain text 123"), std::wstring(L"plain text 123"));
    MDPP_CHECK_EQ(EscapeHtml(L"caf\u00E9 \u4E2D\u6587"), std::wstring(L"caf\u00E9 \u4E2D\u6587"));
}

MDPP_TEST(htmlutil, EscapeHtmlHandlesAmpersandFirstSoItDoesNotDoubleEscape) {
    // If '&' were substituted after '<', "&lt;" produced by an earlier case would
    // be rewritten into "&amp;lt;". Escaping an already-escaped entity must give
    // exactly one extra level, no more.
    MDPP_CHECK_EQ(EscapeHtml(L"&amp;"), std::wstring(L"&amp;amp;"));
    MDPP_CHECK_EQ(EscapeHtml(L"<a href=\"x\">'y'&z</a>"),
                  std::wstring(L"&lt;a href=&quot;x&quot;&gt;&#39;y&#39;&amp;z&lt;/a&gt;"));
}

MDPP_TEST(htmlutil, EscapeScriptStringNeutralisesQuotesAndBackslashes) {
    MDPP_CHECK_EQ(EscapeScriptString(L""), std::wstring(L""));
    MDPP_CHECK_EQ(EscapeScriptString(L"plain"), std::wstring(L"plain"));
    MDPP_CHECK_EQ(EscapeScriptString(L"C:\\notes\\a.md"), std::wstring(L"C:\\\\notes\\\\a.md"));
    MDPP_CHECK_EQ(EscapeScriptString(L"it's"), std::wstring(L"it\\'s"));
    // The backslash case has to run before the quote case, or "\'" would become
    // "\\'" and close the literal anyway.
    MDPP_CHECK_EQ(EscapeScriptString(L"\\'"), std::wstring(L"\\\\\\'"));
}

MDPP_TEST(htmlutil, EscapeScriptStringFlattensLineBreaks) {
    MDPP_CHECK_EQ(EscapeScriptString(L"a\r\nb"), std::wstring(L"a\\r\\nb"));
    MDPP_CHECK_EQ(EscapeScriptString(L"a\nb"), std::wstring(L"a\\nb"));
    // U+2028/U+2029 terminate a JavaScript line even inside a string literal.
    MDPP_CHECK_EQ(EscapeScriptString(L"a\u2028b\u2029c"), std::wstring(L"a\\u2028b\\u2029c"));
}

MDPP_TEST(htmlutil, EscapeScriptStringCannotCloseTheHostScriptTag) {
    const std::wstring escaped = EscapeScriptString(L"</script><img src=x onerror=alert(1)>");

    MDPP_CHECK_NOT_CONTAINS(escaped, std::wstring(L"</script>"));
    MDPP_CHECK_NOT_CONTAINS(escaped, std::wstring(L"<"));
    MDPP_CHECK_NOT_CONTAINS(escaped, std::wstring(L">"));
    MDPP_CHECK_CONTAINS(escaped, std::wstring(L"\\u003c/script\\u003e"));
    MDPP_CHECK_EQ(EscapeScriptString(L"&"), std::wstring(L"\\u0026"));
    MDPP_CHECK(escaped.find(L'\n') == std::wstring::npos);
}

MDPP_TEST(htmlutil, EscapeScriptStringIsNotHtmlEscaping) {
    // Two different escapers exist for two different contexts; collapsing them
    // would produce &amp; inside a JS string literal.
    MDPP_CHECK(EscapeScriptString(L"<&>") != EscapeHtml(L"<&>"));
    MDPP_CHECK_EQ(EscapeHtml(L"<&>"), std::wstring(L"&lt;&amp;&gt;"));
    MDPP_CHECK_EQ(EscapeScriptString(L"<&>"), std::wstring(L"\\u003c\\u0026\\u003e"));
}
