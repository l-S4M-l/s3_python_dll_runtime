#pragma once

#include <mutex>
#include <string>

class Logger {
public:
    static Logger& instance();

    void init(const std::wstring& path);
    void log(const std::wstring& message);
    void log_last_error(const std::wstring& prefix, unsigned long error_code);

private:
    Logger() = default;

    std::mutex mutex_;
    std::wstring path_;
};

std::wstring format_windows_error(unsigned long error_code);
