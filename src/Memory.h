#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <utility>

struct ModuleInfo {
    uintptr_t base = 0;
    size_t    size = 0;
    bool valid() const { return base != 0 && size != 0; }
};

namespace mem {
    // Module info dari process saat ini (karena DLL di-inject ke game)
    ModuleInfo getModule(const wchar_t* name);
    ModuleInfo getModule(const char* name);

    bool read(uintptr_t address, void* buffer, size_t size);
    bool write(uintptr_t address, const void* buffer, size_t size);

    template <typename T>
    inline T read(uintptr_t address) {
        T v{}; read(address, &v, sizeof(v)); return v;
    }
    template <typename T>
    inline void write(uintptr_t address, const T& value) {
        write(address, &value, sizeof(value));
    }

    // Pattern scan: "AA BB ? CC" -> wildcard '?'
    uintptr_t findPattern(uintptr_t start, size_t length, const std::string& pattern);
    // Pointer chain: addr = base; for each off: addr = read(addr + off)
    uintptr_t follow(uintptr_t base, const std::vector<uintptr_t>& offsets);

    bool   setProtect(uintptr_t address, size_t size, DWORD& oldProtect,
                      DWORD newProtect = PAGE_EXECUTE_READWRITE);
    void   restoreProtect(uintptr_t address, size_t size, DWORD oldProtect);
}
