#include "path_utils.h"

#include <filesystem>
#include <fstream>
#include <vector>

namespace {

std::wstring get_path_from_module(HMODULE module) {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return L"";
        }
        if (length < buffer.size() - 1) {
            return std::wstring(buffer.data(), length);
        }
        buffer.resize(buffer.size() * 2);
    }
}

} // namespace

std::wstring get_exe_path() {
    return get_path_from_module(nullptr);
}

std::wstring get_module_path(HMODULE module) {
    return get_path_from_module(module);
}

std::wstring parent_path(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return L"";
    }
    return path.substr(0, slash);
}

std::wstring join_path(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) {
        return right;
    }
    if (left.back() == L'\\' || left.back() == L'/') {
        return left + right;
    }
    return left + L"\\" + right;
}

bool ensure_directory(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }

    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    const std::wstring parent = parent_path(path);
    if (!parent.empty() && parent != path) {
        ensure_directory(parent);
    }

    if (CreateDirectoryW(path.c_str(), nullptr)) {
        return true;
    }
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool file_exists(const std::wstring& path) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool write_text_file_if_missing(const std::wstring& path, const std::string& contents) {
    if (file_exists(path)) {
        return true;
    }

    std::ofstream file{std::filesystem::path(path), std::ios::binary};
    if (!file) {
        return false;
    }
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return file.good();
}

std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size > 0 ? size - 1 : 0, '\0');
    if (size > 0) {
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    }
    return result;
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring result(size > 0 ? size - 1 : 0, L'\0');
    if (size > 0) {
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
    }
    return result;
}
