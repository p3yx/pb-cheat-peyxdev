#include "ReloadFast.h"
#include "Globals.h"
#include <Windows.h>

static void tap(int vk, int ms) {
    keybd_event((BYTE)vk, 0, 0, 0);
    Sleep(ms);
    keybd_event((BYTE)vk, 0, KEYEVENTF_KEYUP, 0);
}

void reloadfast::run() {
    int rkey = g_config.getInt("reload_key", 0x52);   // R
    int a = g_config.getInt("reload_slot_a", 0x33);   // 3
    int b = g_config.getInt("reload_slot_b", 0x31);   // 1
    int hold = g_config.getInt("reload_hold_ms", 35);
    int gap = g_config.getInt("reload_gap_ms", 110);

    tap(rkey, hold);       // mulai reload
    Sleep(gap);
    tap(a, 25);            // cancel animasi lewat ganti senjata
    Sleep(40);
    tap(b, 25);            // balik ke senjata utama
}
