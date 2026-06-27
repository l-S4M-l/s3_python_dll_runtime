#include "logger.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

std::wstring now_string() {
    SYSTEMTIME st{};
    GetLocalTime(&st);

    std::wstringstream ss;
    ss << std::setfill(L'0')
       << L"[" << st.wYear << L"-" << std::setw(2) << st.wMonth << L"-" << std::setw(2) << st.wDay
       << L" " << std::setw(2) << st.wHour << L":" << std::setw(2) << st.wMinute << L":" << std::setw(2) << st.wSecond
       << L"." << std::setw(3) << st.wMilliseconds << L"]";
    return ss.str();
}

} // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::init(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = path;
    std::wofstream file{std::filesystem::path(path_), std::ios::app};
    if (file) {
        file << now_string() << L" log opened" << std::endl;
    }
}

void Logger::log(const std::wstring& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (path_.empty()) {
        return;
    }

    std::wofstream file{std::filesystem::path(path_), std::ios::app};
    if (file) {
        file << now_string() << L" " << message << std::endl;
    }
}

void Logger::log_last_error(const std::wstring& prefix, unsigned long error_code) {
    log(prefix + L": " + format_windows_error(error_code));
}

std::wstring format_windows_error(unsigned long error_code) {
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags, nullptr, error_code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (length == 0 || buffer == nullptr) {
        return L"Windows error " + std::to_wstring(error_code);
    }

    std::wstring message(buffer, length);
    LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) {
        message.pop_back();
    }
    return L"Windows error " + std::to_wstring(error_code) + L": " + message;
}
