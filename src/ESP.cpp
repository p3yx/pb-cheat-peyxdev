#include "ESP.h"
#include "Globals.h"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <string>

// Baca langsung dari file supaya edit manual / Save Config langsung berlaku,
// tidak menunggu reload memori DLL.
static int readConfigInt(const char* key, int def) {
    char path[MAX_PATH]{};
    HMODULE hm = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&readConfigInt), &hm);
    GetModuleFileNameA(hm, path, MAX_PATH);
    std::string full(path);
    auto s = full.find_last_of("\\/");
    std::string cfg = (s == std::string::npos ? "" : full.substr(0, s + 1))
                      + "config\\offsets.cfg";
    std::ifstream f(cfg);
    if (!f.is_open()) return def;
    std::string line;
    while (std::getline(f, line)) {
        auto p = line.find('=');
        if (p == std::string::npos) continue;
        std::string k = line.substr(0, p), v = line.substr(p + 1);
        auto trim = [](std::string x) {
            auto a = x.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) return std::string("");
            auto b = x.find_last_not_of(" \t\r\n");
            return x.substr(a, b - a + 1);
        };
        k = trim(k); v = trim(v);
        if (k == key) return std::atoi(v.c_str());
    }
    return def;
}

namespace {
    HWND  g_hwnd = nullptr;
    const wchar_t* kClass = L"PBESPOverlay";

    COLORREF colBox, colEnemy, colTeam;

    void loadColors() {
        colBox = RGB(g_config.getInt("box_color_r", 255),
                     g_config.getInt("box_color_g", 60),
                     g_config.getInt("box_color_b", 60));
        colEnemy = RGB(g_config.getInt("enemy_color_r", 255),
                       g_config.getInt("enemy_color_g", 80),
                       g_config.getInt("enemy_color_b", 80));
        colTeam = RGB(g_config.getInt("team_color_r", 80),
                      g_config.getInt("team_color_g", 200),
                      g_config.getInt("team_color_b", 255));
    }

    void rect(HDC hdc, int x, int y, int w, int h) {
        Rectangle(hdc, x, y, x + w, y + h);
    }

    void text(HDC hdc, int x, int y, const char* s, COLORREF c) {
        SetTextColor(hdc, c);
        TextOutA(hdc, x, y, s, static_cast<int>(strlen(s)));
    }

    void drawEntities(HDC hdc) {
        if (!g_esp) return;
        loadColors();
        int cx = static_cast<int>(g_screenW * 0.5f);
        int cy = static_cast<int>(g_screenH * 0.5f);
        int sel = g_config.getInt("esp_select", 0);   // 0=ENEMY 1=ALL 2=TEAM

        for (const auto& e : g_entities) {
            if (!e.onScreen) continue;
            if (sel == 0 && !e.enemy) continue;
            if (sel == 2 && e.enemy) continue;
            COLORREF c = e.enemy ? colEnemy : colTeam;
            int top = static_cast<int>(e.sy);
            int bot = static_cast<int>(e.fy);
            int h = bot - top;
            if (h <= 0 || h > 2000) continue;
            int w = static_cast<int>(h * 0.32f);
            int x = static_cast<int>(e.sx) - w / 2;

            if (g_config.getBool("draw_box", true)) {
                SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
                HPEN pen = CreatePen(PS_SOLID, 2, c);
                HPEN old = (HPEN)SelectObject(hdc, pen);
                rect(hdc, x, top, w, h);
                // kepala
                int hh = static_cast<int>(h * 0.22f);
                rect(hdc, x + w / 4, top - hh, w / 2, hh);
                SelectObject(hdc, old);
                DeleteObject(pen);
            }

            if (g_config.getBool("draw_health", true) && e.health > 0) {
                int barW = w;
                int barH = 4;
                int hp = std::max(0, std::min(100, e.health));
                SelectObject(hdc, GetStockObject(DC_PEN));
                SetDCPenColor(hdc, RGB(20, 20, 20));
                rect(hdc, x, bot + 26, barW, barH);
                SetDCPenColor(hdc, hp > 50 ? RGB(60, 220, 80) : RGB(240, 60, 60));
                rect(hdc, x, bot + 26, barW * hp / 100, barH);
            }

            char buf[64];
            if (g_config.getBool("draw_distance", true)) {
                snprintf(buf, sizeof(buf), "%.0f m", e.distance);
                text(hdc, x + w + 6, top, buf, c);
            }
            if (g_config.getBool("draw_name", true)) {
                snprintf(buf, sizeof(buf), "%s [%d]", e.enemy ? "ENEMY" : "TEAM", e.health);
                text(hdc, x, bot + 10, buf, c);
            }
            if (g_config.getBool("draw_snapline", true)) {
                HPEN pen = CreatePen(PS_SOLID, 1, c);
                HPEN old = (HPEN)SelectObject(hdc, pen);
                MoveToEx(hdc, cx, cy, nullptr);
                LineTo(hdc, static_cast<int>(e.sx), top);
                SelectObject(hdc, old);
                DeleteObject(pen);
            }
        }

        if (g_config.getBool("draw_crosshair", true)) {
            int r = readConfigInt("crosshair_size", g_config.getInt("crosshair_size", 12));
            COLORREF cc = RGB(g_config.getInt("crosshair_r", 60),
                              g_config.getInt("crosshair_g", 220),
                              g_config.getInt("crosshair_b", 80));
            // titik (dot) — bukan lingkaran
            int d = std::max(2, r / 3);
            HBRUSH dot = CreateSolidBrush(cc);
            HBRUSH oldBr = (HBRUSH)SelectObject(hdc, dot);
            HPEN edge = CreatePen(PS_SOLID, 1, cc);
            HPEN oldPen = (HPEN)SelectObject(hdc, edge);
            Ellipse(hdc, cx - d, cy - d, cx + d, cy + d);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBr);
            DeleteObject(edge);
            DeleteObject(dot);
        }

    }

    LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        switch (m) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            // Hapus frame sebelumnya (hitam = transparan di layered window).
            // Tanpa ini, crosshair lama menumpuk saat ukuran diubah.
            HBRUSH clear = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &rc, clear);
            DeleteObject(clear);
            SetBkMode(hdc, TRANSPARENT);
            drawEntities(hdc);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(h, m, w, l);
        }
    }
}

bool esp::init() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = kClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    g_hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                             kClass, L"PB ESP", WS_POPUP,
                             0, 0, static_cast<int>(g_screenW), static_cast<int>(g_screenH),
                             nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_hwnd) return false;
    SetLayeredWindowAttributes(g_hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(g_hwnd, SW_SHOWNA);
    UpdateWindow(g_hwnd);
    return true;
}

void esp::render() {
    if (!g_hwnd) return;
    InvalidateRect(g_hwnd, nullptr, TRUE);
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void esp::reassertTopMost() {
    if (g_hwnd)
        SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void esp::refresh() {
    // Jangan invalidate tiap frame — itu yang bikin crosshair kedip.
    // Hanya gambar ulang kalau pengaturan crosshair benar-benar berubah.
    static bool lastOn = true;
    static int  lastR = 0, lastThick = 0;
    static COLORREF lastCol = 0;
    bool on = g_config.getBool("draw_crosshair", true);
    int r = g_config.getInt("crosshair_size", 12);
    int th = g_config.getInt("crosshair_thickness", 1);
    COLORREF col = RGB(g_config.getInt("crosshair_r", 60),
                       g_config.getInt("crosshair_g", 220),
                       g_config.getInt("crosshair_b", 80));
    if (on != lastOn || r != lastR || th != lastThick || col != lastCol) {
        lastOn = on; lastR = r; lastThick = th; lastCol = col;
        if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE);
    }
}

void esp::shutdown() {
    if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = nullptr; }
    UnregisterClassW(kClass, GetModuleHandle(nullptr));
}
