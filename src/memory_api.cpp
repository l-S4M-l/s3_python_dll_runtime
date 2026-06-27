#include "memory_api.h"

#include "logger.h"
#include "path_utils.h"

#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <sstream>

namespace {

bool is_readable(DWORD protect) {
    if (protect & PAGE_GUARD || protect & PAGE_NOACCESS) {
        return false;
    }
    protect &= 0xff;
    return protect == PAGE_READONLY || protect == PAGE_READWRITE || protect == PAGE_WRITECOPY ||
           protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
}

bool is_writable(DWORD protect) {
    if (protect & PAGE_GUARD || protect & PAGE_NOACCESS) {
        return false;
    }
    protect &= 0xff;
    return protect == PAGE_READWRITE || protect == PAGE_WRITECOPY ||
           protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
}

bool validate_range(uintptr_t address, size_t size, bool write, std::string& error) {
    if (size == 0 || size > kMaxMemoryTransfer) {
        error = "size must be between 1 and 4096 bytes";
        return false;
    }
    if (address == 0 || address + size < address) {
        error = "invalid address range";
        return false;
    }

    uintptr_t current = address;
    const uintptr_t end = address + size;
    while (current < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(current), &mbi, sizeof(mbi)) == 0) {
            error = "VirtualQuery failed";
            return false;
        }
        if (mbi.State != MEM_COMMIT || !is_readable(mbi.Protect)) {
            error = "memory page is not readable";
            return false;
        }
        if (write && !is_writable(mbi.Protect)) {
            // Non-writable readable pages may still be patched with VirtualProtect.
        }

        const uintptr_t region_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (region_end <= current) {
            error = "invalid memory region";
            return false;
        }
        current = std::min(region_end, end);
    }
    return true;
}

bool safe_copy_from_address(void* destination, uintptr_t source, size_t size) {
    __try {
        memcpy(destination, reinterpret_cast<const void*>(source), size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool safe_copy_to_address(uintptr_t destination, const void* source, size_t size) {
    __try {
        memcpy(reinterpret_cast<void*>(destination), source, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

} // namespace

MemoryResult get_module_base(const std::string& module) {
    const std::wstring module_w = module.empty() ? L"" : utf8_to_wide(module);
    HMODULE handle = module.empty() ? GetModuleHandleW(nullptr) : GetModuleHandleW(module_w.c_str());
    if (!handle) {
        return {false, "", "module not loaded", 0};
    }
    return {true, "", "", reinterpret_cast<uintptr_t>(handle)};
}

MemoryResult read_memory(uintptr_t address, size_t size) {
    std::string error;
    if (!validate_range(address, size, false, error)) {
        Logger::instance().log(L"memory read failed: " + utf8_to_wide(error));
        return {false, "", error, 0};
    }

    std::vector<uint8_t> buffer(size);
    if (!safe_copy_from_address(buffer.data(), address, size)) {
        Logger::instance().log(L"memory read failed: structured exception");
        return {false, "", "read caused an exception", 0};
    }
    return {true, bytes_to_hex(buffer.data(), buffer.size()), "", 0};
}

MemoryResult write_memory(uintptr_t address, const std::vector<uint8_t>& bytes) {
    std::string error;
    if (!validate_range(address, bytes.size(), true, error)) {
        Logger::instance().log(L"memory write failed: " + utf8_to_wide(error));
        return {false, "", error, 0};
    }

    DWORD old_protect = 0;
    bool changed_protect = false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) != 0 && !is_writable(mbi.Protect)) {
        if (!VirtualProtect(reinterpret_cast<LPVOID>(address), bytes.size(), PAGE_EXECUTE_READWRITE, &old_protect)) {
            Logger::instance().log_last_error(L"memory write failed: VirtualProtect", GetLastError());
            return {false, "", "VirtualProtect failed", 0};
        }
        changed_protect = true;
    }

    bool copied = safe_copy_to_address(address, bytes.data(), bytes.size());
    if (copied) {
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address), bytes.size());
    }

    if (changed_protect) {
        DWORD ignored = 0;
        VirtualProtect(reinterpret_cast<LPVOID>(address), bytes.size(), old_protect, &ignored);
    }

    if (!copied) {
        Logger::instance().log(L"memory write failed: structured exception");
        return {false, "", "write caused an exception", 0};
    }
    return {true, "", "", 0};
}

MemoryResult read_module_memory(const std::string& module, uint64_t offset, size_t size) {
    MemoryResult base = get_module_base(module);
    if (!base.ok) {
        return base;
    }
    return read_memory(base.base + static_cast<uintptr_t>(offset), size);
}

MemoryResult write_module_memory(const std::string& module, uint64_t offset, const std::vector<uint8_t>& bytes) {
    MemoryResult base = get_module_base(module);
    if (!base.ok) {
        return base;
    }
    return write_memory(base.base + static_cast<uintptr_t>(offset), bytes);
}

bool hex_to_bytes(const std::string& hex, std::vector<uint8_t>& out, std::string& error) {
    if (hex.size() % 2 != 0) {
        error = "hex string must have an even length";
        return false;
    }
    if (hex.size() / 2 > kMaxMemoryTransfer) {
        error = "write data exceeds 4096 bytes";
        return false;
    }

    out.clear();
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        const int hi = hex_digit(hex[i]);
        const int lo = hex_digit(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            error = "hex string contains invalid characters";
            return false;
        }
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return true;
}

std::string bytes_to_hex(const uint8_t* data, size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.resize(size * 2);
    for (size_t i = 0; i < size; ++i) {
        out[i * 2] = digits[data[i] >> 4];
        out[i * 2 + 1] = digits[data[i] & 0x0f];
    }
    return out;
}
