#include "Menu.h"
#include "Globals.h"
#include "ESP.h"
#include "Patches.h"
#include "Game.h"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>

namespace {
    struct Item {
        enum Type { Section, Toggle, Cycle, Action } type = Toggle;
        std::string label;
        int  section = -1;
        bool expanded = true;
        std::atomic<bool>* atom = nullptr;
        std::string key;
        std::vector<std::string> opts;
        void (*action)() = nullptr;
    };

    // ---- palet modern ----
    const COLORREF C_BG      = RGB(15, 19, 28);
    const COLORREF C_PANEL   = RGB(24, 31, 45);
    const COLORREF C_ACCENT  = RGB(59, 130, 246);
    const COLORREF C_ACCENT2 = RGB(37, 99, 235);
    const COLORREF C_TEXT    = RGB(226, 232, 240);
    const COLORREF C_MUTED   = RGB(148, 163, 184);
    const COLORREF C_GREEN   = RGB(34, 197, 94);
    const COLORREF C_GREEN_D = RGB(22, 163, 74);
    const COLORREF C_OFF     = RGB(71, 85, 105);
    const COLORREF C_OFF_D   = RGB(51, 65, 85);
    const COLORREF C_AMBER   = RGB(251, 191, 36);

    std::vector<Item> items;
    std::vector<int>  rowY;
    HWND  g_hwnd = nullptr;
    HFONT g_font = nullptr;
    HFONT g_fontBold = nullptr;
    bool  g_visible = true;
    bool  g_minimized = false;
    bool  g_dragging = false;
    int   g_hover = -1;
    POINT g_dragOffset{ 0, 0 };
    bool  g_lastEsp = true;

    const wchar_t* CLASS  = L"PBFreeMenuV6";
    const int WIN_W   = 274;
    const int TITLE_H = 26;
    const int ROW_H   = 19;
    const int SEC_H   = 21;

    void invalidate() {
        if (g_hwnd && g_visible) InvalidateRect(g_hwnd, nullptr, FALSE);
    }
    void recalcHeight();

    void addSection(const char* label, int& sec) {
        Item it; it.type = Item::Section; it.label = label; it.expanded = true;
        items.push_back(it); sec = static_cast<int>(items.size()) - 1;
    }
    void addToggle(int sec, const char* label, std::atomic<bool>* atom) {
        Item it; it.type = Item::Toggle; it.label = label; it.section = sec; it.atom = atom;
        items.push_back(it);
    }
    void addToggleCfg(int sec, const char* label, const char* key, bool def) {
        Item it; it.type = Item::Toggle; it.label = label; it.section = sec; it.key = key;
        if (!g_config.has(key)) g_config.setBool(key, def);
        items.push_back(it);
    }
    void addCycle(int sec, const char* label, const char* key,
                  std::vector<std::string> opts, const char* def) {
        Item it; it.type = Item::Cycle; it.label = label; it.section = sec; it.key = key;
        it.opts = std::move(opts);
        if (!g_config.has(key)) g_config.set(key, def);
        items.push_back(it);
    }
    void addAction(int sec, const char* label, void (*fn)()) {
        Item it; it.type = Item::Action; it.label = label; it.section = sec; it.action = fn;
        items.push_back(it);
    }

    std::string configPath() {
        char path[MAX_PATH]{};
        HMODULE hm = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&configPath), &hm);
        GetModuleFileNameA(hm, path, MAX_PATH);
        std::string full(path);
        auto slash = full.find_last_of("\\/");
        std::string dir = slash == std::string::npos ? "" : full.substr(0, slash + 1);
        return dir + "config\\offsets.cfg";
    }

    // "Minimize" = collapse jadi title bar saja (tetap terlihat, tidak di-hide)
    void minimizeMenu() {
        if (!g_hwnd || g_minimized) return;
        g_minimized = true;
        g_hover = -1;
        RECT rc; GetWindowRect(g_hwnd, &rc);
        // pakai koordinat eksplisit: SWP_NOMOVE bisa bikin Windows menaruh
        // popup di y=32767 (posisi off-screen) saat tinggi diperkecil.
        SetWindowPos(g_hwnd, HWND_TOPMOST, rc.left, rc.top, WIN_W, TITLE_H + 8,
                     SWP_NOACTIVATE);
        RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        invalidate();
    }

    void restoreMenu() {
        if (!g_hwnd || !g_minimized) return;
        g_minimized = false;
        recalcHeight();
        invalidate();
    }

    void buildItems() {
        items.clear();
        int sec = -1;

        // Hanya fitur yang benar-benar bekerja (tanpa offset).
        addSection("VISUAL", sec);
        addToggleCfg(sec, "Name Tag ESP", "draw_name", true);
        addToggleCfg(sec, "Crosshair", "draw_crosshair", true);
        addCycle(sec, "Cross Size", "crosshair_size", { "8", "12", "16", "20" }, "12");

        addSection("MENU", sec);
        addAction(sec, "Reload Config", []() { menu::reloadConfig(); });
        addAction(sec, "Save Config", []() { menu::saveAndMinimize(); });
        addAction(sec, "Minimize  [INSERT]", []() { minimizeMenu(); });
        addAction(sec, "Unload Cheat [END]", []() { g_running = false; });
    }

    void computeLayout() {
        rowY.assign(items.size(), -1);
        int y = TITLE_H + 8;
        for (size_t i = 0; i < items.size(); ++i) {
            Item& it = items[i];
            if (it.type != Item::Section && it.section >= 0 &&
                it.section < static_cast<int>(items.size()) && !items[it.section].expanded) {
                continue;
            }
            rowY[i] = y;
            y += (it.type == Item::Section ? SEC_H : ROW_H);
        }
    }

    std::string valueText(const Item& it) {
        switch (it.type) {
        case Item::Section:
            return it.expanded ? "-" : "+";
        case Item::Toggle:
            return (it.atom ? it.atom->load() : g_config.getBool(it.key, false)) ? "ON" : "OFF";
        case Item::Cycle: {
            std::string cur = g_config.getString(it.key, it.opts.empty() ? "" : it.opts[0]);
            for (const auto& o : it.opts)
                if (o == cur) {
                    if (it.key == "esp_select") return o == "0" ? "ENEMY" : (o == "1" ? "ALL" : "TEAM");
                    return o;
                }
            return cur;
        }
        case Item::Action:
            return "";
        }
        return "";
    }

    static HHOOK g_mouseHook = nullptr;
    static bool g_hookDown = false;
    static bool g_hookDrag = false;
    static POINT g_hookOff{ 0, 0 };

    static FILETIME g_cfgTime{};

    void rememberConfigTime() {
        WIN32_FILE_ATTRIBUTE_DATA a{};
        if (GetFileAttributesExA(configPath().c_str(), GetFileExInfoStandard, &a))
            g_cfgTime = a.ftLastWriteTime;
    }

    void persistNow() {
        // Simpan setiap kali ada perubahan supaya nilai menu benar-benar
        // dipakai session ini dan bertahan setelah restart.
        g_config.save(configPath());
        rememberConfigTime();
    }

    void checkConfigChanged() {
        WIN32_FILE_ATTRIBUTE_DATA a{};
        if (!GetFileAttributesExA(configPath().c_str(), GetFileExInfoStandard, &a)) return;
        if (CompareFileTime(&a.ftLastWriteTime, &g_cfgTime) != 0) {
            g_cfgTime = a.ftLastWriteTime;
            menu::reloadConfig();
        }
    }

    void activate(size_t idx) {
        Item& it = items[idx];
        switch (it.type) {
        case Item::Section:
            it.expanded = !it.expanded;
            computeLayout();
            break;
        case Item::Toggle:
            if (it.atom) {
                bool on = it.atom->load();
                it.atom->store(!on);
                if (it.key == "esp_enabled") g_config.setBool("esp_enabled", !on);
            } else {
                g_config.setBool(it.key, !g_config.getBool(it.key, false));
            }
            break;
        case Item::Cycle: {
            if (it.opts.empty()) break;
            std::string cur = g_config.getString(it.key, it.opts[0]);
            size_t p = 0;
            for (size_t i = 0; i < it.opts.size(); ++i) if (it.opts[i] == cur) { p = i; break; }
            g_config.set(it.key, it.opts[(p + 1) % it.opts.size()]);
            break;
        }
        case Item::Action:
            if (it.action) it.action();
            break;
        }
        persistNow();
    }

    static DWORD g_lastClickTick = 0;

    static LRESULT CALLBACK LowLevelMouse(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode >= 0 && g_hwnd && g_visible) {
            MSLLHOOKSTRUCT* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
            POINT p = ms->pt;
            RECT wr{}; GetWindowRect(g_hwnd, &wr);
            bool inside = p.x >= wr.left && p.x < wr.right && p.y >= wr.top && p.y < wr.bottom;
            if (wParam == WM_LBUTTONDOWN) {
                g_hookDown = true;
                DWORD tick = GetTickCount();
                if (tick - g_lastClickTick < 200) return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
                g_lastClickTick = tick;
                if (inside) {
                    int x = p.x - wr.left, y = p.y - wr.top;
                    if (y < TITLE_H) {
                        if (x > WIN_W - 52 && x < WIN_W - 34) {
                            g_visible = false; ShowWindow(g_hwnd, SW_HIDE);
                        } else if (x > WIN_W - 34) {
                            if (g_minimized) restoreMenu(); else minimizeMenu();
                        } else if (g_minimized) restoreMenu();
                        else { g_hookDrag = true; g_hookOff = { x, y }; }
                    } else if (!g_minimized) {
                        for (size_t i = 0; i < items.size(); ++i) {
                            if (rowY[i] < 0) continue;
                            int rh = (items[i].type == Item::Section ? SEC_H : ROW_H);
                            if (y >= rowY[i] && y < rowY[i] + rh) { activate(i); invalidate(); break; }
                        }
                    }
                }
            } else if (wParam == WM_LBUTTONUP) {
                g_hookDown = false; g_hookDrag = false;
            } else if (wParam == WM_MOUSEMOVE && g_hookDrag && g_hookDown) {
                SetWindowPos(g_hwnd, HWND_TOPMOST, p.x - g_hookOff.x, p.y - g_hookOff.y,
                             0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
    }

    // ---------- helper gambar modern ----------
    static COLORREF mix(COLORREF a, COLORREF b, float t) {
        auto ch = [](COLORREF c, int sh) { return (int)((c >> sh) & 0xFF); };
        int r = (int)(ch(a, 0) + (ch(b, 0) - ch(a, 0)) * t);
        int g = (int)(ch(a, 8) + (ch(b, 8) - ch(a, 8)) * t);
        int bl = (int)(ch(a, 16) + (ch(b, 16) - ch(a, 16)) * t);
        return RGB(r, g, bl);
    }

    void gradientH(HDC dc, int x, int y, int w, int h, COLORREF c1, COLORREF c2) {
        for (int i = 0; i < w; ++i) {
            HBRUSH br = CreateSolidBrush(mix(c1, c2, (float)i / (float)std::max(1, w - 1)));
            RECT r{ x + i, y, x + i + 1, y + h };
            FillRect(dc, &r, br);
            DeleteObject(br);
        }
    }

    void roundBox(HDC dc, int x, int y, int w, int h, int rad, COLORREF fill, COLORREF line) {
        HPEN pen = CreatePen(PS_SOLID, 1, line);
        HBRUSH br = CreateSolidBrush(fill);
        HGDIOBJ op = SelectObject(dc, pen);
        HGDIOBJ ob = SelectObject(dc, br);
        RoundRect(dc, x, y, x + w, y + h, rad, rad);
        SelectObject(dc, op);
        SelectObject(dc, ob);
        DeleteObject(pen);
        DeleteObject(br);
    }

    void pill(HDC dc, int x, int y, int w, int h, bool on) {
        roundBox(dc, x, y, w, h, h / 2, on ? C_GREEN : C_OFF, on ? C_GREEN_D : C_OFF_D);
        RECT r{ x, y, x + w, y + h };
        SelectObject(dc, g_fontBold);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, on ? RGB(255, 255, 255) : RGB(203, 213, 225));
        DrawTextA(dc, on ? "ON" : "OFF", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    void badgeValue(HDC dc, int x, int y, int w, int h, const char* txt) {
        roundBox(dc, x, y, w, h, 5, RGB(30, 41, 59), RGB(51, 65, 85));
        RECT r{ x, y, x + w, y + h };
        SelectObject(dc, g_font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_AMBER);
        DrawTextA(dc, txt, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        switch (m) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC screen = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            int W = rc.right, H = rc.bottom;

            HDC mem = CreateCompatibleDC(screen);
            HBITMAP bmp = CreateCompatibleBitmap(screen, W, H);
            HGDIOBJ oldBmp = SelectObject(mem, bmp);

            // background + border
            HBRUSH bg = CreateSolidBrush(C_BG);
            FillRect(mem, &rc, bg);
            DeleteObject(bg);
            HPEN border = CreatePen(PS_SOLID, 2, C_ACCENT);
            HGDIOBJ oldPen = SelectObject(mem, border);
            SelectObject(mem, GetStockObject(HOLLOW_BRUSH));
            RoundRect(mem, 0, 0, W - 1, H - 1, 10, 10);

            // ---- title bar gradient (clip ke sudut bulat dulu) ----
            HRGN rg = CreateRoundRectRgn(0, 0, W, H, 10, 10);
            SelectClipRgn(mem, rg);
            gradientH(mem, 2, 2, W - 4, TITLE_H, C_ACCENT2, C_ACCENT);
            SelectClipRgn(mem, nullptr);
            DeleteObject(rg);

            SelectObject(mem, g_fontBold);
            SetBkMode(mem, TRANSPARENT);
            SetTextColor(mem, RGB(255, 255, 255));
            TextOutA(mem, 12, 8, "CHEAT PB BY PEYXDEV", 19);

            // tombol X (tutup menu) dan minimize / restore
            roundBox(mem, W - 50, 7, 16, 15, 4, RGB(120, 30, 40), RGB(220, 80, 90));
            SetTextColor(mem, RGB(255, 255, 255));
            TextOutA(mem, W - 46, 8, "X", 1);
            roundBox(mem, W - 30, 7, 20, 15, 4, RGB(15, 23, 42), C_ACCENT);
            TextOutA(mem, W - 24, 8, g_minimized ? "+" : "_", 1);

            if (g_minimized) {
                BitBlt(screen, 0, 0, W, H, mem, 0, 0, SRCCOPY);
                SelectObject(mem, oldPen);
                SelectObject(mem, oldBmp);
                DeleteObject(border);
                DeleteObject(bmp);
                DeleteDC(mem);
                EndPaint(h, &ps);
                return 0;
            }

            for (size_t i = 0; i < items.size(); ++i) {
                if (rowY[i] < 0) continue;
                const Item& it = items[i];
                int y = rowY[i];
                int rh = (it.type == Item::Section ? SEC_H : ROW_H);

                if (it.type == Item::Section) {
                    roundBox(mem, 8, y, W - 16, SEC_H - 2, 5, C_PANEL, RGB(40, 52, 71));
                    RECT ab{ 8, y + 3, 11, y + SEC_H - 5 };
                    HBRUSH abm = CreateSolidBrush(C_ACCENT);
                    FillRect(mem, &ab, abm);
                    DeleteObject(abm);
                    SelectObject(mem, g_fontBold);
                    SetTextColor(mem, RGB(191, 219, 254));
                    TextOutA(mem, 17, y + 3, it.label.c_str(), (int)it.label.size());
                    SetTextColor(mem, C_MUTED);
                    TextOutA(mem, W - 24, y + 3, it.expanded ? "-" : "+", 1);
                } else {
                    if (g_hover == (int)i)
                        roundBox(mem, 8, y, W - 16, ROW_H - 1, 4, C_PANEL, C_PANEL);
                    SelectObject(mem, g_font);
                    SetTextColor(mem, C_TEXT);
                    TextOutA(mem, 17, y + 3, it.label.c_str(), (int)it.label.size());

                    if (it.type == Item::Toggle) {
                        bool on = it.atom ? it.atom->load() : g_config.getBool(it.key, false);
                        pill(mem, W - 62, y + 2, 44, 15, on);
                    } else if (it.type == Item::Cycle) {
                        std::string v = valueText(it);
                        int bw = 24 + (int)v.size() * 2;
                        badgeValue(mem, W - 14 - bw, y + 2, bw, 15, v.c_str());
                    }
                }
            }

            SelectObject(mem, g_font);
            SetTextColor(mem, C_MUTED);
            TextOutA(mem, 12, H - 16, "FREE - NO TOKEN", 15);

            BitBlt(screen, 0, 0, W, H, mem, 0, 0, SRCCOPY);

            SelectObject(mem, oldPen);
            SelectObject(mem, oldBmp);
            DeleteObject(border);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_SETCURSOR:
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return TRUE;
        // Klik & drag ditangani SATU kali saja lewat polling GetCursorPos/
        // GetAsyncKeyState di menu::render(). Jangan diproses di sini juga,
        // kalau tidak toggle akan berubah dua kali (12 -> 20 -> 12 ...).
        case WM_LBUTTONDOWN:
            return 0;
        case WM_MOUSEMOVE:
            return 0;
        case WM_MOUSELEAVE:
            if (g_hover != -1) { g_hover = -1; invalidate(); }
            return 0;
        case WM_LBUTTONUP:
            if (g_dragging) { g_dragging = false; ReleaseCapture(); }
            return 0;
        case WM_DESTROY:
            return 0;
        default:
            return DefWindowProcW(h, m, w, l);
        }
    }

    void recalcHeight() {
        if (!g_hwnd) return;
        computeLayout();
        int last = TITLE_H + 8;
        for (int y : rowY) if (y >= 0) last = y + ROW_H;
        int h = last + 22;
        SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, WIN_W, h,
                     SWP_NOMOVE | SWP_NOACTIVATE);
        RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
    }
}

bool menu::init() {
    g_lastEsp = g_esp.load();
    buildItems();
    rememberConfigTime();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = CLASS;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        game::logStatus("MENU: RegisterClass gagal");
        return false;
    }

    g_font = CreateFontA(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    g_fontBold = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

    int mx = g_config.getInt("menu_x", 20);
    int my = g_config.getInt("menu_y", 90);
    HMODULE hm = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&menu::init), &hm);
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouse, hm, 0);

    g_hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                             CLASS, L"CHEAT PB BY PEYXDEV", WS_POPUP | WS_VISIBLE,
                             mx, my, WIN_W, 400, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_hwnd) {
        char b[80]; snprintf(b, sizeof(b), "MENU: CreateWindow gagal err=%lu", GetLastError());
        game::logStatus(b);
        return false;
    }
    game::logStatus("MENU: window dibuat");
    SetLayeredWindowAttributes(g_hwnd, RGB(0, 0, 0), 238, LWA_ALPHA);
    recalcHeight();
    UpdateWindow(g_hwnd);
    return true;
}

void menu::render() {
    if (!g_hwnd) return;
    // Cek perubahan file config cukup 2x/detik, bukan tiap frame.
    static DWORD lastCfgCheck = 0;
    DWORD now = GetTickCount();
    if (now - lastCfgCheck > 500) { checkConfigChanged(); lastCfgCheck = now; }
    // Topmost cukup dipasang saat init; jangan SetWindowPos tiap frame
    // (1000x/detik bikin FPS game drop).
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // patch nametag ESP mengikuti toggle "Name Tag"
    patches::setNameTag(g_config.getBool("draw_name", true));

    bool esp = g_esp.load();
    if (esp != g_lastEsp) {
        g_config.setBool("esp_enabled", esp);
        g_lastEsp = esp;
        invalidate();
    }
}

void menu::shutdown() {
    if (g_mouseHook) { UnhookWindowsHookEx(g_mouseHook); g_mouseHook = nullptr; }
    if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = nullptr; }
    if (g_font) { DeleteObject(g_font); g_font = nullptr; }
    if (g_fontBold) { DeleteObject(g_fontBold); g_fontBold = nullptr; }
    UnregisterClassW(CLASS, GetModuleHandle(nullptr));
}

void menu::toggle() {
    if (!g_hwnd) return;
    if (!g_visible) {
        g_visible = true;
        ShowWindow(g_hwnd, SW_SHOWNA);
        g_minimized = false;
        recalcHeight();
        invalidate();
    } else if (g_minimized) {
        restoreMenu();
    } else {
        minimizeMenu();
    }
}

void menu::saveAndMinimize() {
    g_config.save(configPath());
    // reload dari file yang baru saja ditulis supaya memori DLL sinkron
    g_config.load(configPath());
    patches::setNameTag(g_config.getBool("draw_name", true));
    g_lastEsp = g_esp.load();
    rememberConfigTime();
    invalidate();
    minimizeMenu();
}

void menu::reloadConfig() {
    g_config.load(configPath());
    rememberConfigTime();
    patches::setNameTag(g_config.getBool("draw_name", true));
    g_lastEsp = g_esp.load();
    invalidate();
}

bool menu::isVisible() { return g_visible && !g_minimized; }
HWND menu::menuHwnd() { return g_hwnd; }

bool menu::pointerOver() {
    if (!g_hwnd || !g_visible) return false;
    POINT p{}; GetCursorPos(&p);
    RECT r{}; GetWindowRect(g_hwnd, &r);
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}
