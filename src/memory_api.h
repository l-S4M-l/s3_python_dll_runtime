#pragma once

#include <cstdint>
#include <string>
#include <vector>

constexpr size_t kMaxMemoryTransfer = 4096;

struct MemoryResult {
    bool ok = false;
    std::string data;
    std::string error;
    uintptr_t base = 0;
};

MemoryResult get_module_base(const std::string& module);
MemoryResult read_memory(uintptr_t address, size_t size);
MemoryResult write_memory(uintptr_t address, const std::vector<uint8_t>& bytes);
MemoryResult read_module_memory(const std::string& module, uint64_t offset, size_t size);
MemoryResult write_module_memory(const std::string& module, uint64_t offset, const std::vector<uint8_t>& bytes);
bool hex_to_bytes(const std::string& hex, std::vector<uint8_t>& out, std::string& error);
std::string bytes_to_hex(const uint8_t* data, size_t size);
