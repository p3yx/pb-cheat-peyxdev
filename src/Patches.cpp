#include "Patches.h"
#include "Globals.h"
#include "Game.h"
#include <Windows.h>
#include <cstring>

namespace {
    const unsigned char NAME_TAG_SIG[] = {
        0xC6,0x01,0x00,0x8D,0x49,0x08,0x83,0xE8,0x01,0x75,0xF5
    };
    DWORD g_nameTagAddr = 0;
    BYTE  g_nameTagOrig = 0x00;
    bool  g_nameTagOn = false;

    void applyNameTag(bool on) {
        if (!g_nameTagAddr) return;
        BYTE* p = reinterpret_cast<BYTE*>(g_nameTagAddr + 2);
        DWORD old = 0;
        if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old)) return;
        *p = on ? 0x01 : g_nameTagOrig;
        VirtualProtect(p, 1, old, &old);
        g_nameTagOn = on;
    }
}

void patches::init() {
    if (!g_client.valid()) return;
    for (DWORD i = 0; i + sizeof(NAME_TAG_SIG) < g_client.size; i++) {
        if (memcmp((void*)(g_client.base + i), NAME_TAG_SIG,
                   sizeof(NAME_TAG_SIG)) == 0) {
            g_nameTagAddr = g_client.base + i;
            break;
        }
    }
    if (g_nameTagAddr) {
        g_nameTagOrig = *reinterpret_cast<BYTE*>(g_nameTagAddr + 2);
        applyNameTag(g_config.getBool("draw_name", true));
    }
    game::logStatus(g_nameTagAddr ? "PATCH NameTag ESP: AKTIF (0xa79eb1)"
                                  : "PATCH NameTag ESP: tidak ditemukan");
}

void patches::setNameTag(bool on) {
    if (g_nameTagOn != on) applyNameTag(on);
}

void patches::shutdown() {
    if (g_nameTagAddr) applyNameTag(false);
}

bool patches::nameTagAvailable() { return g_nameTagAddr != 0; }
