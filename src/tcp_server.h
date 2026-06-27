#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

class TcpServer {
public:
    TcpServer(std::string host, uint16_t port);
    ~TcpServer();

    bool start();

private:
    void run();
    void handle_client(uintptr_t client_socket);
    std::string handle_request(const std::string& line);

    std::string host_;
    uint16_t port_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};
