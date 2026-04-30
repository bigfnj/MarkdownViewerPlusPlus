#include "HtmlUtil.h"

namespace markdownplusplus {

std::wstring EscapeHtml(const std::wstring& text) {
    std::wstring escaped;
    escaped.reserve(text.size());

    for (wchar_t ch : text) {
        switch (ch) {
            case L'&':
                escaped += L"&amp;";
                break;
            case L'<':
                escaped += L"&lt;";
                break;
            case L'>':
                escaped += L"&gt;";
                break;
            case L'"':
                escaped += L"&quot;";
                break;
            case L'\'':
                escaped += L"&#39;";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }

    return escaped;
}

std::wstring EscapeScriptString(const std::wstring& text) {
    std::wstring escaped;
    escaped.reserve(text.size());

    for (wchar_t ch : text) {
        switch (ch) {
            case L'\\':
                escaped += L"\\\\";
                break;
            case L'\'':
                escaped += L"\\'";
                break;
            case L'\r':
                escaped += L"\\r";
                break;
            case L'\n':
                escaped += L"\\n";
                break;
            case L'<':
                escaped += L"\\u003c";
                break;
            case L'>':
                escaped += L"\\u003e";
                break;
            case L'&':
                escaped += L"\\u0026";
                break;
            case 0x2028:
                escaped += L"\\u2028";
                break;
            case 0x2029:
                escaped += L"\\u2029";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }

    return escaped;
}

}  // namespace markdownplusplus
