#include "tcp_server.h"

#include "logger.h"
#include "memory_api.h"
#include "path_utils.h"

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace {

std::string json_escape(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (c == '\\' || c == '"') {
            out.push_back('\\');
        }
        if (c == '\r') out += "\\r";
        else if (c == '\n') out += "\\n";
        else out.push_back(c);
    }
    return out;
}

std::string ok_string(const std::string& key, const std::string& value) {
    return "{\"ok\":true,\"" + key + "\":\"" + json_escape(value) + "\"}\n";
}

std::string ok_base(uintptr_t base) {
    return "{\"ok\":true,\"base\":" + std::to_string(static_cast<unsigned long long>(base)) + "}\n";
}

std::string ok_empty() {
    return "{\"ok\":true}\n";
}

std::string error_response(const std::string& error) {
    return "{\"ok\":false,\"error\":\"" + json_escape(error) + "\"}\n";
}

bool find_json_value(const std::string& json, const std::string& key, size_t& pos) {
    const std::string needle = "\"" + key + "\"";
    pos = json.find(needle);
    if (pos == std::string::npos) {
        return false;
    }
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) {
        return false;
    }
    ++pos;
    while (pos < json.size() && isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    return pos < json.size();
}

bool get_string(const std::string& json, const std::string& key, std::string& out) {
    size_t pos = 0;
    if (!find_json_value(json, key, pos) || json[pos] != '"') {
        return false;
    }
    ++pos;
    out.clear();
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '"') {
            return true;
        }
        if (c == '\\' && pos < json.size()) {
            char e = json[pos++];
            if (e == 'n') out.push_back('\n');
            else if (e == 'r') out.push_back('\r');
            else out.push_back(e);
        } else {
            out.push_back(c);
        }
    }
    return false;
}

bool get_u64(const std::string& json, const std::string& key, uint64_t& out) {
    size_t pos = 0;
    if (!find_json_value(json, key, pos)) {
        return false;
    }
    size_t end = pos;
    while (end < json.size() && isdigit(static_cast<unsigned char>(json[end]))) {
        ++end;
    }
    if (end == pos) {
        return false;
    }
    try {
        out = std::stoull(json.substr(pos, end - pos));
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

TcpServer::TcpServer(std::string host, uint16_t port) : host_(std::move(host)), port_(port) {}

TcpServer::~TcpServer() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.detach();
    }
}

bool TcpServer::start() {
    if (running_.exchange(true)) {
        return true;
    }
    thread_ = std::thread(&TcpServer::run, this);
    return true;
}

void TcpServer::run() {
    WSADATA wsa{};
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (rc != 0) {
        Logger::instance().log(L"TCP startup failed: WSAStartup error " + std::to_wstring(rc));
        running_ = false;
        return;
    }

    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) {
        Logger::instance().log(L"TCP startup failed: socket error " + std::to_wstring(WSAGetLastError()));
        WSACleanup();
        running_ = false;
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);

    BOOL reuse = TRUE;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    if (bind(listen_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        Logger::instance().log(L"TCP bind failed on 127.0.0.1:" + std::to_wstring(port_) + L": WSA error " + std::to_wstring(WSAGetLastError()));
        closesocket(listen_socket);
        WSACleanup();
        running_ = false;
        return;
    }

    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        Logger::instance().log(L"TCP listen failed: WSA error " + std::to_wstring(WSAGetLastError()));
        closesocket(listen_socket);
        WSACleanup();
        running_ = false;
        return;
    }

    Logger::instance().log(L"TCP server listening on 127.0.0.1:" + std::to_wstring(port_));

    while (running_) {
        sockaddr_in client_addr{};
        int client_len = sizeof(client_addr);
        SOCKET client = accept(listen_socket, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client == INVALID_SOCKET) {
            continue;
        }

        char addr_text[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &client_addr.sin_addr, addr_text, sizeof(addr_text));
        if (std::string(addr_text) != "127.0.0.1") {
            closesocket(client);
            continue;
        }

        Logger::instance().log(L"TCP client connected");
        std::thread(&TcpServer::handle_client, this, static_cast<uintptr_t>(client)).detach();
    }

    closesocket(listen_socket);
    WSACleanup();
}

void TcpServer::handle_client(uintptr_t client_socket) {
    SOCKET client = static_cast<SOCKET>(client_socket);
    std::string pending;
    char buffer[1024];

    for (;;) {
        const int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            break;
        }
        pending.append(buffer, buffer + received);

        size_t newline = std::string::npos;
        while ((newline = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                continue;
            }

            const std::string response = handle_request(line);
            send(client, response.data(), static_cast<int>(response.size()), 0);
        }
    }

    closesocket(client);
    Logger::instance().log(L"TCP client disconnected");
}

std::string TcpServer::handle_request(const std::string& line) {
    std::string cmd;
    if (!get_string(line, "cmd", cmd)) {
        return error_response("missing cmd");
    }

    if (cmd == "ping") {
        return ok_string("data", "pong");
    }

    if (cmd == "module_base") {
        std::string module;
        get_string(line, "module", module);
        MemoryResult result = get_module_base(module);
        return result.ok ? ok_base(result.base) : error_response(result.error);
    }

    if (cmd == "read") {
        std::string module;
        uint64_t offset = 0;
        uint64_t size = 0;
        get_string(line, "module", module);
        if (!get_u64(line, "offset", offset) || !get_u64(line, "size", size)) {
            return error_response("read requires offset and size");
        }
        MemoryResult result = read_module_memory(module, offset, static_cast<size_t>(size));
        return result.ok ? ok_string("data", result.data) : error_response(result.error);
    }

    if (cmd == "write") {
        std::string module;
        std::string data;
        uint64_t offset = 0;
        get_string(line, "module", module);
        if (!get_u64(line, "offset", offset) || !get_string(line, "data", data)) {
            return error_response("write requires offset and data");
        }
        std::vector<uint8_t> bytes;
        std::string error;
        if (!hex_to_bytes(data, bytes, error)) {
            return error_response(error);
        }
        MemoryResult result = write_module_memory(module, offset, bytes);
        return result.ok ? ok_empty() : error_response(result.error);
    }

    if (cmd == "read_abs") {
        uint64_t address = 0;
        uint64_t size = 0;
        if (!get_u64(line, "address", address) || !get_u64(line, "size", size)) {
            return error_response("read_abs requires address and size");
        }
        MemoryResult result = read_memory(static_cast<uintptr_t>(address), static_cast<size_t>(size));
        return result.ok ? ok_string("data", result.data) : error_response(result.error);
    }

    if (cmd == "write_abs") {
        uint64_t address = 0;
        std::string data;
        if (!get_u64(line, "address", address) || !get_string(line, "data", data)) {
            return error_response("write_abs requires address and data");
        }
        std::vector<uint8_t> bytes;
        std::string error;
        if (!hex_to_bytes(data, bytes, error)) {
            return error_response(error);
        }
        MemoryResult result = write_memory(static_cast<uintptr_t>(address), bytes);
        return result.ok ? ok_empty() : error_response(result.error);
    }

    return error_response("unknown cmd");
}
