#pragma once

#include <string>

namespace markdownplusplus {

std::wstring EscapeHtml(const std::wstring& text);
std::wstring EscapeScriptString(const std::wstring& text);

}  // namespace markdownplusplus
