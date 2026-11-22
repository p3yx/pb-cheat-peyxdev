#include "Memory.h"
#include <tlhelp32.h>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

static bool ieq(const std::string& a, const std::string& b) {
    return a.size() == b.size() &&
        std::equal(a.begin(), a.end(), b.begin(), [](char x, char y){
            return std::tolower((unsigned char)x) == std::tolower((unsigned char)y);
        });
}

static ModuleInfo getModuleImpl(const std::string& wanted) {
    ModuleInfo info;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
                                           GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return info;
    MODULEENTRY32W me{}; me.dwSize = sizeof(me);
    if (Module32FirstW(snap, &me)) {
        do {
            std::wstring ws(me.szModule);
            std::string name(ws.begin(), ws.end());
            if (ieq(name, wanted)) {
                info.base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                info.size = me.modBaseSize;
                break;
            }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return info;
}

ModuleInfo mem::getModule(const wchar_t* name) {
    std::wstring ws(name);
    return getModuleImpl(std::string(ws.begin(), ws.end()));
}
ModuleInfo mem::getModule(const char* name) {
    return getModuleImpl(name ? name : "");
}

bool mem::read(uintptr_t address, void* buffer, size_t size) {
    if (!address || !buffer) return false;
    SIZE_T out = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address),
                             buffer, size, &out) && out == size;
}

bool mem::write(uintptr_t address, const void* buffer, size_t size) {
    if (!address || !buffer) return false;
    SIZE_T out = 0;
    DWORD old = 0;
    VirtualProtect(reinterpret_cast<LPVOID>(address), size, PAGE_EXECUTE_READWRITE, &old);
    BOOL ok = WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<LPVOID>(address),
                                 buffer, size, &out);
    VirtualProtect(reinterpret_cast<LPVOID>(address), size, old, &old);
    return ok && out == size;
}

static std::pair<std::vector<uint8_t>, std::vector<uint8_t>> parsePattern(const std::string& pat) {
    std::vector<uint8_t> bytes, mask;
    std::istringstream iss(pat);
    std::string tok;
    while (iss >> tok) {
        if (tok == "?" || tok == "??") {
            bytes.push_back(0); mask.push_back(0);
        } else {
            bytes.push_back(static_cast<uint8_t>(std::strtoul(tok.c_str(), nullptr, 16)));
            mask.push_back(1);
        }
    }
    return { bytes, mask };
}

uintptr_t mem::findPattern(uintptr_t start, size_t length, const std::string& pattern) {
    auto parsed = parsePattern(pattern);
    const auto& bytes = parsed.first;
    const auto& mask = parsed.second;
    if (bytes.empty() || length < bytes.size()) return 0;
    size_t n = bytes.size();
    for (size_t i = 0; i + n <= length; ++i) {
        bool ok = true;
        for (size_t j = 0; j < n; ++j) {
            if (mask[j] &&
                *reinterpret_cast<const uint8_t*>(start + i + j) != bytes[j]) {
                ok = false; break;
            }
        }
        if (ok) return start + i;
    }
    return 0;
}

uintptr_t mem::follow(uintptr_t base, const std::vector<uintptr_t>& offsets) {
    uintptr_t addr = base;
    for (auto off : offsets) {
        addr = mem::read<uintptr_t>(addr + off);
        if (!addr) return 0;
    }
    return addr;
}

bool mem::setProtect(uintptr_t address, size_t size, DWORD& oldProtect, DWORD newProtect) {
    return VirtualProtect(reinterpret_cast<LPVOID>(address), size, newProtect, &oldProtect) != 0;
}

void mem::restoreProtect(uintptr_t address, size_t size, DWORD oldProtect) {
    DWORD tmp = 0;
    VirtualProtect(reinterpret_cast<LPVOID>(address), size, oldProtect, &tmp);
}
