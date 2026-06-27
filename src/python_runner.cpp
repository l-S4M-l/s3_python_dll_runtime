#include "python_runner.h"

#include "logger.h"
#include "path_utils.h"
#include "python_launcher.h"
#include "tcp_server.h"

#include <Windows.h>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

constexpr uint16_t kDefaultPort = 47892;

const char* kMemPy = R"PY(import json
import socket
import struct


class GameMemoryClient:
    def __init__(self, host="127.0.0.1", port=47892, timeout=5.0):
        self.host = host
        self.port = int(port)
        self.timeout = timeout
        self.sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
        self.file = self.sock.makefile("rwb", buffering=0)

    def close(self):
        try:
            self.file.close()
        finally:
            self.sock.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def request(self, payload):
        line = (json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8")
        self.file.write(line)
        response = self.file.readline()
        if not response:
            raise RuntimeError("memory server closed the connection")
        data = json.loads(response.decode("utf-8"))
        if not data.get("ok"):
            raise RuntimeError(data.get("error", "memory server request failed"))
        return data

    def ping(self):
        return self.request({"cmd": "ping"})["data"]

    def module_base(self, module=""):
        return int(self.request({"cmd": "module_base", "module": module})["base"])

    def read_bytes(self, module, offset, size):
        data = self.request({"cmd": "read", "module": module, "offset": int(offset), "size": int(size)})["data"]
        return bytes.fromhex(data)

    def write_bytes(self, module, offset, data):
        self.request({"cmd": "write", "module": module, "offset": int(offset), "data": bytes(data).hex()})

    def read_abs(self, address, size):
        data = self.request({"cmd": "read_abs", "address": int(address), "size": int(size)})["data"]
        return bytes.fromhex(data)

    def write_abs(self, address, data):
        self.request({"cmd": "write_abs", "address": int(address), "data": bytes(data).hex()})

    def read_int(self, module, offset):
        return struct.unpack("<i", self.read_bytes(module, offset, 4))[0]

    def write_int(self, module, offset, value):
        self.write_bytes(module, offset, struct.pack("<i", int(value)))

    def read_uint(self, module, offset):
        return struct.unpack("<I", self.read_bytes(module, offset, 4))[0]

    def write_uint(self, module, offset, value):
        self.write_bytes(module, offset, struct.pack("<I", int(value)))

    def read_float(self, module, offset):
        return struct.unpack("<f", self.read_bytes(module, offset, 4))[0]

    def write_float(self, module, offset, value):
        self.write_bytes(module, offset, struct.pack("<f", float(value)))

    def read_double(self, module, offset):
        return struct.unpack("<d", self.read_bytes(module, offset, 8))[0]

    def write_double(self, module, offset, value):
        self.write_bytes(module, offset, struct.pack("<d", float(value)))
)PY";

const char* kTestScriptPy = R"PY(import os
import time
from datetime import datetime

from mem import GameMemoryClient


def main():
    host = os.environ.get("SKATE_MEM_HOST", "127.0.0.1")
    port = int(os.environ.get("SKATE_MEM_PORT", "47892"))
    game_dir = os.environ.get("SKATE_MOD_GAME_DIR")
    py_dir = os.environ.get("SKATE_MOD_PY_DIR", os.path.dirname(os.path.abspath(__file__)))
    output_dir = game_dir if game_dir else py_dir
    output_path = os.path.join(output_dir, "python_test_success.txt")

    last_error = None
    result = None
    deadline = time.time() + 8.0
    while time.time() < deadline:
        try:
            with GameMemoryClient(host, port, timeout=1.0) as mem:
                result = mem.ping()
            break
        except Exception as exc:
            last_error = exc
            time.sleep(0.25)

    with open(output_path, "w", encoding="utf-8") as f:
        f.write("timestamp=" + datetime.now().isoformat() + "\n")
        if result is not None:
            f.write("ping=" + str(result) + "\n")
        else:
            f.write("error=" + repr(last_error) + "\n")


if __name__ == "__main__":
    main()
)PY";

struct Config {
    uint16_t port = kDefaultPort;
    bool auto_launch_python = true;
};

std::string trim(std::string value) {
    while (!value.empty() && isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

void create_default_config(const std::wstring& path) {
    write_text_file_if_missing(path, "port=47892\r\nauto_launch_python=true\r\n");
}

Config read_config(const std::wstring& path) {
    Config config;
    std::ifstream file{std::filesystem::path(path)};
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (key == "port") {
            try {
                int port = std::stoi(value);
                if (port > 0 && port <= 65535) {
                    config.port = static_cast<uint16_t>(port);
                }
            } catch (...) {
            }
        } else if (key == "auto_launch_python") {
            config.auto_launch_python = !(value == "false" || value == "0" || value == "no");
        }
    }
    return config;
}

void create_templates(const std::wstring& py_dir) {
    const std::wstring mem_path = join_path(py_dir, L"mem.py");
    const std::wstring test_path = join_path(py_dir, L"test_script.py");
    if (write_text_file_if_missing(mem_path, kMemPy)) {
        Logger::instance().log(L"Ensured Python helper: " + mem_path);
    } else {
        Logger::instance().log_last_error(L"Failed to create " + mem_path, GetLastError());
    }

    if (write_text_file_if_missing(test_path, kTestScriptPy)) {
        Logger::instance().log(L"Ensured Python test script: " + test_path);
    } else {
        Logger::instance().log_last_error(L"Failed to create " + test_path, GetLastError());
    }
}

} // namespace

DWORD WINAPI python_runner_thread(LPVOID parameter) {
    HMODULE module = reinterpret_cast<HMODULE>(parameter);

    const std::wstring exe_path = get_exe_path();
    const std::wstring dll_path = get_module_path(module);
    const std::wstring game_dir = parent_path(exe_path);
    const std::wstring mods_dir = join_path(game_dir, L"Mods");
    const std::wstring py_dir = join_path(mods_dir, L"py");
    const std::wstring log_path = join_path(game_dir, L"python_runner.log");
    const std::wstring config_path = join_path(mods_dir, L"python_runner.ini");

    ensure_directory(mods_dir);
    ensure_directory(py_dir);

    Logger::instance().init(log_path);
    Logger::instance().log(L"python_runner startup");
    Logger::instance().log(L"host exe path: " + exe_path);
    Logger::instance().log(L"python_runner DLL path: " + dll_path);
    Logger::instance().log(L"game folder: " + game_dir);
    Logger::instance().log(L"mods folder: " + mods_dir);
    Logger::instance().log(L"py folder: " + py_dir);
    Logger::instance().log(L"config file path: " + config_path);

    create_default_config(config_path);
    Config config = read_config(config_path);
    Logger::instance().log(L"TCP host and port: 127.0.0.1:" + std::to_wstring(config.port));
    Logger::instance().log(std::wstring(L"auto_launch_python: ") + (config.auto_launch_python ? L"true" : L"false"));

    create_templates(py_dir);

    static TcpServer* server = new TcpServer("127.0.0.1", config.port);
    server->start();

    if (config.auto_launch_python) {
        LaunchContext launch_context;
        launch_context.game_dir = game_dir;
        launch_context.mods_dir = mods_dir;
        launch_context.py_dir = py_dir;
        launch_context.port = config.port;
        launch_python_scripts(launch_context);
    } else {
        Logger::instance().log(L"auto_launch_python=false; not launching Python scripts");
    }

    return 0;
}
