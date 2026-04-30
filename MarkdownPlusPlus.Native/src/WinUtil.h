#pragma once

#include <windows.h>

#include <string>

namespace markdownplusplus {

std::wstring Utf8ToWide(const std::string& text);
std::string WideToUtf8(const std::wstring& text);
std::wstring GetModuleDirectory(HINSTANCE module);
std::wstring ReadUtf8FileAsWide(const std::wstring& path);
bool WriteUtf8File(const std::wstring& path, const std::string& bytes);
std::wstring CombinePath(const std::wstring& left, const std::wstring& right);
std::wstring PathToFileUri(const std::wstring& path);

}  // namespace markdownplusplus
