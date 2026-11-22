#include <Windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <cwctype>

static bool iequal(const std::wstring& a, const std::wstring& b) {
    return a.size() == b.size() &&
        std::equal(a.begin(), a.end(), b.begin(), [](wchar_t x, wchar_t y){
            return std::towlower(x) == std::towlower(y);
        });
}

static DWORD findProcess(const std::string& name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    std::wstring wanted(name.begin(), name.end());
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring proc(pe.szExeFile);
            if (iequal(proc, wanted)) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    (void)snap;
    CloseHandle(snap);
    return pid;
}

static bool enableDebugPrivilege() {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok))
        return false;
    TOKEN_PRIVILEGES tp{};
    LookupPrivilegeValueA(nullptr, "SeDebugPrivilege", &tp.Privileges[0].Luid);
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bool ok = AdjustTokenPrivileges(tok, FALSE, &tp, 0, nullptr, nullptr) != 0;
    CloseHandle(tok);
    return ok;
}

int main(int argc, char** argv) {
    std::string proc = argc > 1 ? argv[1] : "pointblank.exe";
    std::string dll  = argc > 2 ? argv[2] : "pb-cheat.dll";

    enableDebugPrivilege();

    char fullDll[MAX_PATH]{};
    GetFullPathNameA(dll.c_str(), MAX_PATH, fullDll, nullptr);
    std::string dllPath(fullDll);

    // Argumen pertama boleh nama process ATAU PID angka
    DWORD pid = 0;
    if (!proc.empty() && std::all_of(proc.begin(), proc.end(), [](unsigned char c){ return std::isdigit(c); })) {
        pid = static_cast<DWORD>(std::stoul(proc));
    } else {
        pid = findProcess(proc);
    }
    if (!pid) {
        std::cerr << "[!] Proses '" << proc << "' tidak ditemukan. "
                  << "Cek nama process di Task Manager.\n";
        return 1;
    }
    std::cout << "[*] PID = " << pid << "\n";

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        std::cerr << "[!] OpenProcess gagal. Coba jalankan injector sebagai Administrator.\n";
        return 1;
    }

    LPVOID remote = VirtualAllocEx(hProc, nullptr, dllPath.size() + 1,
                                   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { std::cerr << "[!] VirtualAllocEx gagal.\n"; CloseHandle(hProc); return 1; }

    if (!WriteProcessMemory(hProc, remote, dllPath.c_str(), dllPath.size() + 1, nullptr)) {
        std::cerr << "[!] WriteProcessMemory gagal.\n";
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE); CloseHandle(hProc); return 1;
    }

    FARPROC loadLib = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    HANDLE t = CreateRemoteThread(hProc, nullptr, 0,
                                  (LPTHREAD_START_ROUTINE)loadLib, remote, 0, nullptr);
    if (!t) {
        std::cerr << "[!] CreateRemoteThread gagal. Anti-cheat mungkin memblokir.\n";
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE); CloseHandle(hProc); return 1;
    }

    WaitForSingleObject(t, 5000);
    DWORD code = 0;
    GetExitCodeThread(t, &code);
    std::cout << "[+] Injected. LoadLibrary return = 0x" << std::hex << code << std::dec << "\n";
    std::cout << "[*] Cek log: %TEMP%\\pb-cheat.log\n";

    CloseHandle(t);
    VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return 0;
}
