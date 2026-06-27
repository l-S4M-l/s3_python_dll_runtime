#pragma once

#include <cstdint>
#include <string>

struct LaunchContext {
    std::wstring game_dir;
    std::wstring mods_dir;
    std::wstring py_dir;
    std::string host = "127.0.0.1";
    uint16_t port = 47892;
};

void launch_python_scripts(const LaunchContext& context);
