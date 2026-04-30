#include "WinUtil.h"

#include <shlwapi.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace markdownplusplus {

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;

    if (size <= 0) {
        codePage = CP_ACP;
        flags = 0;
        size = MultiByteToWideChar(codePage, flags, text.data(), static_cast<int>(text.size()), nullptr, 0);
    }

    if (size <= 0) {
        return {};
    }

    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(codePage, flags, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring GetModuleDirectory(HINSTANCE module) {
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD length = 0;

    while (true) {
        length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }

        if (length < buffer.size() - 1) {
            break;
        }

        buffer.resize(buffer.size() * 2);
    }

    PathRemoveFileSpecW(buffer.data());
    return buffer.data();
}

std::wstring ReadUtf8FileAsWide(const std::wstring& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }

    std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return Utf8ToWide(bytes);
}

bool WriteUtf8File(const std::wstring& path, const std::string& bytes) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || !file) {
        return false;
    }

    const size_t written = bytes.empty() ? 0 : fwrite(bytes.data(), 1, bytes.size(), file);
    const bool ok = ferror(file) == 0 && written == bytes.size();
    fclose(file);
    return ok;
}

std::wstring CombinePath(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) {
        return right;
    }

    if (right.empty()) {
        return left;
    }

    wchar_t buffer[MAX_PATH] = {};
    wcsncpy_s(buffer, left.c_str(), _TRUNCATE);
    PathAppendW(buffer, right.c_str());
    return buffer;
}

std::wstring PathToFileUri(const std::wstring& path) {
    if (path.empty()) {
        return {};
    }

    std::wstring normalized = path;
    DWORD urlLength = 0;
    HRESULT probe = UrlCreateFromPathW(normalized.c_str(), nullptr, &urlLength, 0);
    if (probe != E_POINTER || urlLength == 0) {
        return {};
    }

    std::wstring uri(urlLength, L'\0');
    HRESULT result = UrlCreateFromPathW(normalized.c_str(), uri.data(), &urlLength, 0);
    if (FAILED(result)) {
        return {};
    }

    uri.assign(uri.c_str());
    return uri;
}

}  // namespace markdownplusplus
