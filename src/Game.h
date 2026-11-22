#pragma once
#include "Globals.h"
#include <vector>

namespace game {
    // Inisialisasi: baca config, resolve modul & offset. return false kalau gagal.
    bool init();
    // Update data game (local player, entity list, view matrix)
    void update();
    // Tulis log ringkas ke file (untuk debug offset)
    void logStatus(const char* msg);
}
