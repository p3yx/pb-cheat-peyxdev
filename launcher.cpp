#include <Windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <cwctype>

static bool isAdmin() {
    BOOL ok = FALSE;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    PSID admins = nullptr;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins)) {
        CheckTokenMembership(nullptr, admins, &ok);
        FreeSid(admins);
    }
    return ok != FALSE;
}

static bool iequal(const std::wstring& a, const std::wstring& b) {
    return a.size() == b.size() &&
        std::equal(a.begin(), a.end(), b.begin(), [](wchar_t x, wchar_t y) {
            return std::towlower(x) == std::towlower(y);
        });
}

static DWORD findProcess(const std::wstring& name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring proc(pe.szExeFile);
            if (iequal(proc, name)) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static std::string dirOfSelf() {
    char path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string full(path);
    auto s = full.find_last_of("\\/");
    return s == std::string::npos ? "" : full.substr(0, s + 1);
}

static DWORD g_targetPid = 0;
static HWND  g_targetHwnd = nullptr;

static BOOL CALLBACK enumWindowCb(HWND h, LPARAM) {
    DWORD wpid = 0;
    GetWindowThreadProcessId(h, &wpid);
    if (wpid == g_targetPid && IsWindowVisible(h) && GetWindowTextLengthW(h) > 0) {
        g_targetHwnd = h;
        return FALSE;
    }
    return TRUE;
}

static void unloadOldInstances(DWORD pid) {
    // Kirim END ke game supaya DLL lama (kalau ada) unload sebelum inject versi baru.
    g_targetPid = pid;
    g_targetHwnd = nullptr;
    EnumWindows(enumWindowCb, 0);
    HWND hwnd = g_targetHwnd;
    if (!hwnd) return;
    SetForegroundWindow(hwnd);
    Sleep(250);
    keybd_event(VK_END, 0, 0, 0);
    Sleep(60);
    keybd_event(VK_END, 0, KEYEVENTF_KEYUP, 0);
    Sleep(500);
}

static bool inject(DWORD pid, const std::string& dll) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) return false;
    LPVOID remote = VirtualAllocEx(hProc, nullptr, dll.size() + 1,
                                   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { CloseHandle(hProc); return false; }
    bool ok = WriteProcessMemory(hProc, remote, dll.c_str(), dll.size() + 1, nullptr) != FALSE;
    if (ok) {
        FARPROC load = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
        HANDLE t = CreateRemoteThread(hProc, nullptr, 0,
                    reinterpret_cast<LPTHREAD_START_ROUTINE>(load), remote, 0, nullptr);
        if (t) {
            WaitForSingleObject(t, 5000);
            DWORD code = 0; GetExitCodeThread(t, &code);
            ok = (code != 0);
            CloseHandle(t);
        } else ok = false;
    }
    VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return ok;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    if (!isAdmin()) {
        char exe[MAX_PATH]{};
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        ShellExecuteA(nullptr, "runas", exe, nullptr, nullptr, SW_SHOW);
        return 0;
    }

    std::string dir = dirOfSelf();
    std::string dll = dir + "pb-cheat.dll";

    DWORD pid = findProcess(L"PointBlank.exe");
    if (!pid) {
        // coba start game dari lokasi default
        const char* gamePath = "c:\\Zepetto\\PointBlank ID\\PointBlank.exe";
        if (GetFileAttributesA(gamePath) != INVALID_FILE_ATTRIBUTES) {
            ShellExecuteA(nullptr, "open", gamePath, nullptr, nullptr, SW_SHOW);
            for (int i = 0; i < 40; ++i) {
                Sleep(1000);
                pid = findProcess(L"PointBlank.exe");
                if (pid) break;
            }
        }
    }

    if (!pid) {
        MessageBoxA(nullptr,
            "PointBlank.exe tidak ditemukan.\n\nBuka game dulu, lalu jalankan launcher ini lagi.",
            "PB CHEAT - PEYXDEV", MB_ICONWARNING | MB_OK);
        return 1;
    }

    unloadOldInstances(pid);
    bool ok = inject(pid, dll);

    if (ok) {
        MessageBoxA(nullptr,
            "BERHASIL di-inject ke PointBlank.\n\n"
            " Tekan INSERT  = buka / minimize menu\n"
            " Klik row      = ubah ON/OFF & nilai\n"
            " Klik title    = drag / restore\n"
            " Save Config   = simpan + minimize\n\n"
            "Cek log: %TEMP%\\pb-cheat.log",
            "PB CHEAT - PEYXDEV - OK", MB_ICONINFORMATION | MB_OK);
    } else {
        MessageBoxA(nullptr,
            "Gagal inject. Pastikan game sudah jalan dan launcher dijalankan sebagai Administrator.",
            "PB CHEAT - PEYXDEV - GAGAL", MB_ICONERROR | MB_OK);
    }
    return ok ? 0 : 1;
}
