// Scratch directory helper for tests that must touch the filesystem
// (PluginOptionsStore writes a real .ini; BuildStandaloneDocument inlines a real
// preview.css). Created under %TEMP%, removed in the destructor.
#pragma once

#include <windows.h>

#include <string>

namespace mdpptest {

inline void RemoveTree(const std::wstring& directory) {
    std::wstring pattern = directory;
    if (!pattern.empty() && pattern.back() != L'\\') { pattern.push_back(L'\\'); }
    const std::wstring root = pattern;
    pattern += L"*";

    WIN32_FIND_DATAW found = {};
    HANDLE handle = FindFirstFileW(pattern.c_str(), &found);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring name = found.cFileName;
            if (name == L"." || name == L"..") { continue; }
            const std::wstring child = root + name;
            if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                RemoveTree(child);
            } else {
                SetFileAttributesW(child.c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(child.c_str());
            }
        } while (FindNextFileW(handle, &found));
        FindClose(handle);
    }

    RemoveDirectoryW(root.c_str());
}

class TempDir {
public:
    TempDir() {
        static long counter = 0;
        wchar_t base[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, base);

        wchar_t unique[MAX_PATH] = {};
        swprintf_s(unique, L"%smdpp-tests-%lu-%ld\\", base,
                   GetCurrentProcessId(), InterlockedIncrement(&counter));
        path_ = unique;
        RemoveTree(path_);
        CreateDirectoryW(path_.c_str(), nullptr);
    }

    ~TempDir() { RemoveTree(path_); }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    // Always ends with a backslash.
    const std::wstring& Path() const { return path_; }

    std::wstring File(const std::wstring& relative) const { return path_ + relative; }

    std::wstring MakeSubDir(const std::wstring& relative) const {
        const std::wstring full = path_ + relative;
        CreateDirectoryW(full.c_str(), nullptr);
        return full;
    }

    static bool Exists(const std::wstring& path) {
        return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

private:
    std::wstring path_;
};

}  // namespace mdpptest
