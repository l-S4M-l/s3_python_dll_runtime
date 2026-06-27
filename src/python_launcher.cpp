#include "python_launcher.h"

#include "logger.h"
#include "path_utils.h"

#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

namespace {

struct PythonCommand {
    std::wstring exe;
    std::wstring prefix_args;
    std::wstring label;
};

std::wstring quote_arg(const std::wstring& value) {
    std::wstring out = L"\"";
    for (wchar_t c : value) {
        if (c == L'"') {
            out += L"\\\"";
        } else {
            out.push_back(c);
        }
    }
    out += L"\"";
    return out;
}

bool search_exe(const wchar_t* name, std::wstring& path) {
    DWORD needed = SearchPathW(nullptr, name, nullptr, 0, nullptr, nullptr);
    if (needed == 0) {
        return false;
    }

    std::wstring buffer(needed, L'\0');
    DWORD written = SearchPathW(nullptr, name, nullptr, needed, buffer.data(), nullptr);
    if (written == 0 || written >= needed) {
        return false;
    }
    buffer.resize(written);
    path = buffer;
    return true;
}

bool find_python(PythonCommand& command) {
    std::wstring path;
    if (search_exe(L"pythonw.exe", path)) {
        command = {path, L"", L"pythonw.exe"};
        return true;
    }
    if (search_exe(L"pyw.exe", path)) {
        command = {path, L"", L"pyw.exe"};
        return true;
    }
    if (search_exe(L"py.exe", path)) {
        command = {path, L"-3", L"py.exe -3"};
        return true;
    }
    if (search_exe(L"python.exe", path)) {
        command = {path, L"", L"python.exe"};
        return true;
    }
    return false;
}

std::wstring lower_name(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return value;
}

std::vector<std::wstring> discover_scripts(const std::wstring& py_dir) {
    std::vector<std::wstring> scripts;
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(join_path(py_dir, L"*.py").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        return scripts;
    }

    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        std::wstring name = data.cFileName;
        std::wstring lower = lower_name(name);
        if (lower == L"mem.py" || lower == L"__init__.py" || (!name.empty() && name[0] == L'_')) {
            continue;
        }
        scripts.push_back(join_path(py_dir, name));
    } while (FindNextFileW(find, &data));

    FindClose(find);
    std::sort(scripts.begin(), scripts.end());
    return scripts;
}

std::wstring build_environment(const LaunchContext& context) {
    LPWCH env = GetEnvironmentStringsW();
    std::wstring block;
    if (env) {
        for (LPWCH p = env; *p; p += wcslen(p) + 1) {
            block.append(p);
            block.push_back(L'\0');
        }
        FreeEnvironmentStringsW(env);
    }

    auto add = [&](const std::wstring& key, const std::wstring& value) {
        block.append(key);
        block.push_back(L'=');
        block.append(value);
        block.push_back(L'\0');
    };

    add(L"SKATE_MOD_GAME_DIR", context.game_dir);
    add(L"SKATE_MODS_DIR", context.mods_dir);
    add(L"SKATE_MOD_PY_DIR", context.py_dir);
    add(L"SKATE_MEM_HOST", L"127.0.0.1");
    add(L"SKATE_MEM_PORT", std::to_wstring(context.port));
    block.push_back(L'\0');
    return block;
}

} // namespace

void launch_python_scripts(const LaunchContext& context) {
    PythonCommand python;
    if (!find_python(python)) {
        Logger::instance().log(L"Python not found. Tried pythonw.exe, pyw.exe, py.exe, python.exe");
        return;
    }

    Logger::instance().log(L"Python executable found: " + python.label + L" at " + python.exe);

    std::vector<std::wstring> scripts = discover_scripts(context.py_dir);
    if (scripts.empty()) {
        Logger::instance().log(L"No user Python scripts found to launch");
        return;
    }

    std::wstring env = build_environment(context);
    for (const std::wstring& script : scripts) {
        std::wstring command_line = quote_arg(python.exe);
        if (!python.prefix_args.empty()) {
            command_line += L" " + python.prefix_args;
        }
        command_line += L" " + quote_arg(script);

        Logger::instance().log(L"Launching Python script: " + command_line);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi{};
        BOOL ok = CreateProcessW(
            python.exe.c_str(),
            command_line.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
            env.data(),
            context.py_dir.c_str(),
            &si,
            &pi);

        if (!ok) {
            Logger::instance().log_last_error(L"Python script launch failed for " + script, GetLastError());
            continue;
        }

        Logger::instance().log(L"Python script launch succeeded: " + script);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}
