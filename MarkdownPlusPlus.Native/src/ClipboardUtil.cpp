#include "ClipboardUtil.h"

#include "WinUtil.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace markdownplusplus {
namespace {

constexpr char kStartFragment[] = "<!--StartFragment-->";
constexpr char kEndFragment[] = "<!--EndFragment-->";

std::string BuildCfHtml(const std::wstring& articleHtml) {
    std::string html =
        "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">"
        "<html><body>"
        "<!--StartFragment-->"
        "<article class=\"markdown-body\">";
    html += WideToUtf8(articleHtml);
    html +=
        "</article>"
        "<!--EndFragment-->"
        "</body></html>";

    constexpr char kHeaderFormat[] =
        "Version:0.9\r\n"
        "StartHTML:%09zu\r\n"
        "EndHTML:%09zu\r\n"
        "StartFragment:%09zu\r\n"
        "EndFragment:%09zu\r\n"
        "StartSelection:%09zu\r\n"
        "EndSelection:%09zu\r\n";

    const size_t zero = 0;
    const int headerLengthProbe = std::snprintf(nullptr, 0, kHeaderFormat, zero, zero, zero, zero, zero, zero);
    if (headerLengthProbe <= 0) {
        return {};
    }

    const size_t headerLength = static_cast<size_t>(headerLengthProbe);
    const size_t startHtml = headerLength;
    const size_t endHtml = startHtml + html.size();

    const size_t startMarker = html.find(kStartFragment);
    const size_t endMarker = html.find(kEndFragment);
    if (startMarker == std::string::npos || endMarker == std::string::npos) {
        return {};
    }

    const size_t startFragment = startHtml + startMarker + std::strlen(kStartFragment);
    const size_t endFragment = startHtml + endMarker;

    char header[192] = {};
    std::snprintf(
        header,
        sizeof(header),
        kHeaderFormat,
        startHtml,
        endHtml,
        startFragment,
        endFragment,
        startFragment,
        endFragment);

    std::string cfHtml = header;
    cfHtml += html;
    return cfHtml;
}

bool SetClipboardBytes(UINT format, const void* data, size_t size) {
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!handle) {
        return false;
    }

    void* locked = GlobalLock(handle);
    if (!locked) {
        GlobalFree(handle);
        return false;
    }

    std::memcpy(locked, data, size);
    GlobalUnlock(handle);

    if (!SetClipboardData(format, handle)) {
        GlobalFree(handle);
        return false;
    }

    return true;
}

bool SetClipboardUtf16Text(const std::wstring& text) {
    return SetClipboardBytes(CF_UNICODETEXT, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
}

bool SetClipboardHtml(const std::string& cfHtml) {
    const UINT htmlFormat = RegisterClipboardFormatW(L"HTML Format");
    if (htmlFormat == 0) {
        return false;
    }

    return SetClipboardBytes(htmlFormat, cfHtml.c_str(), cfHtml.size() + 1);
}

}  // namespace

bool CopyHtmlToClipboard(HWND owner, const std::wstring& articleHtml, const std::wstring& plainText) {
    if (!OpenClipboard(owner)) {
        return false;
    }

    EmptyClipboard();
    const bool copiedHtml = SetClipboardHtml(BuildCfHtml(articleHtml));
    const bool copiedText = SetClipboardUtf16Text(plainText);
    CloseClipboard();

    return copiedHtml || copiedText;
}

}  // namespace markdownplusplus
