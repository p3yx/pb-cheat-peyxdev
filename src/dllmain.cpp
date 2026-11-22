#include "Globals.h"
#include "Game.h"
#include "ESP.h"
#include "Menu.h"
#include "Patches.h"
#include "ReloadFast.h"
#include <Windows.h>

namespace {
    HMODULE g_self = nullptr;

    DWORD WINAPI mainThread(LPVOID) {
        if (!game::init()) return 1;

        patches::init();      // NameTag ESP
        esp::init();          // crosshair
        menu::init();

        int menuKey = g_config.getInt("menu_toggle_key", 0x2D);  // INSERT
        int exitKey = g_config.getInt("exit_key", 0x23);         // END
        int fastReloadKey = g_config.getInt("fast_reload_key", 0x75); // F6
        bool pMenu = false, pReload = false;
        DWORD lastTop = 0, lastVis = 0;

        while (g_running) {
            bool nowMenu = (GetAsyncKeyState(menuKey) & 0x8000) != 0;
            if (nowMenu && !pMenu) menu::toggle();
            pMenu = nowMenu;

            if (GetAsyncKeyState(exitKey) & 0x8000) {
                game::logStatus("EXIT: END ditekan, unloading...");
                g_running = false;
            }

            bool nowReload = (GetAsyncKeyState(fastReloadKey) & 0x8000) != 0;
            if (nowReload && !pReload && g_config.getBool("fast_reload", false))
                reloadfast::run();
            pReload = nowReload;

            patches::setNameTag(g_config.getBool("draw_name", true));

            // Topmost & cek visual cukup periodik, bukan tiap 2ms.
            DWORD t = GetTickCount();
            if (t - lastTop > 1000) { esp::reassertTopMost(); lastTop = t; }
            if (t - lastVis > 150)  { esp::refresh();       lastVis = t; }
            menu::render();   // pump pesan ESP + menu
            Sleep(8);
        }

        menu::shutdown();
        patches::shutdown();
        esp::shutdown();
        FreeLibraryAndExitThread(g_self, 0);
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = hModule;
        DisableThreadLibraryCalls(hModule);
        HANDLE t = CreateThread(nullptr, 0, mainThread, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
