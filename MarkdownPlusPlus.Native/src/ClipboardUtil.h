#pragma once

#include <windows.h>

#include <string>

namespace markdownplusplus {

bool CopyHtmlToClipboard(HWND owner, const std::wstring& articleHtml, const std::wstring& plainText);

}  // namespace markdownplusplus
