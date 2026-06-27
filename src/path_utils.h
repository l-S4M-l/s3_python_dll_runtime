#pragma once

#include <Windows.h>
#include <string>

std::wstring get_exe_path();
std::wstring get_module_path(HMODULE module);
std::wstring parent_path(const std::wstring& path);
std::wstring join_path(const std::wstring& left, const std::wstring& right);
bool ensure_directory(const std::wstring& path);
bool file_exists(const std::wstring& path);
bool write_text_file_if_missing(const std::wstring& path, const std::string& contents);
std::string wide_to_utf8(const std::wstring& value);
std::wstring utf8_to_wide(const std::string& value);
