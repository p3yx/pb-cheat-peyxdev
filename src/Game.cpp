#include "Game.h"
#include <fstream>
#include <ctime>
#include <sstream>

Config g_config;
ModuleInfo g_client;
ViewMatrix g_viewMatrix;
Entity g_local;
std::vector<Entity> g_entities;
std::atomic<bool> g_running{ true };
std::atomic<bool> g_aimbot{ true };
std::atomic<bool> g_esp{ true };
std::atomic<bool> g_trigger{ false };
std::atomic<bool> g_noRecoil{ true };
std::atomic<bool> g_bhop{ false };
std::atomic<bool> g_speedhack{ false };
std::atomic<bool> g_wallhack{ true };
std::atomic<bool> g_quickSwitch{ true };
std::atomic<bool> g_macroOn{ true };
float g_screenW = 1920;
float g_screenH = 1080;

namespace {
    uintptr_t oLocal = 0, oList = 0, oView = 0, oEntitySize = 0;
    uintptr_t oOrigin = 0, oViewOff = 0, oHealth = 0, oTeam = 0, oDormant = 0,
              oBone = 0, oCrosshair = 0, oFlash = 0, oVelocity = 0, oScoped = 0,
              oEyeAngles = 0, oAttack = 0, oJump = 0;
    int boneHead = 6, boneChest = 3;
    int maxEntities = 128;
    float espMaxDist = 250.0f;

    struct Offsets {
        uintptr_t* p;
        const char* key;
    };

    uintptr_t resolveSignature(const char* key, uintptr_t fallback) {
        std::string sig = g_config.getString(std::string("sig") + key, "");
        if (sig.empty() || !g_client.valid()) return fallback;
        uintptr_t found = mem::findPattern(g_client.base, g_client.size, sig);
        if (!found) return fallback;
        // rel32: signature biasanya menunjuk instruksi; sesuaikan di sini bila perlu.
        // Default: ambil alamat relatif terhadap base modul.
        return (found - g_client.base);
    }
}

bool game::init() {
    // Cari config: ikut DLL path. Fallback ke folder kerja.
    char dllPath[MAX_PATH]{};
    HMODULE hm = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&game::init), &hm);
    GetModuleFileNameA(hm, dllPath, MAX_PATH);
    std::string full(dllPath);
    auto slash = full.find_last_of("\\/");
    std::string dir = slash == std::string::npos ? "" : full.substr(0, slash + 1);
    std::string cfg = dir + "config\\offsets.cfg";
    if (!g_config.load(cfg)) {
        if (!g_config.load("offsets.cfg")) {
            game::logStatus("GAGAL: config/offsets.cfg tidak ditemukan.");
            return false;
        }
    }

    std::string moduleName = g_config.getString("client_module", "client.dll");
    g_client = mem::getModule(moduleName.c_str());
    if (!g_client.valid()) {
        game::logStatus("GAGAL: modul client tidak ditemukan. Cek client_module di config.");
        return false;
    }

    maxEntities = g_config.getInt("max_entities", 128);
    espMaxDist  = g_config.getFloat("esp_max_distance", 250.0f);
    boneHead    = g_config.getInt("bone_head_index", 6);
    boneChest   = g_config.getInt("bone_chest_index", 3);

    oLocal      = resolveSignature("LocalPlayer", g_config.getHex("dwLocalPlayer"));
    oList       = resolveSignature("EntityList",  g_config.getHex("dwEntityList"));
    oView       = resolveSignature("ViewMatrix",  g_config.getHex("dwViewMatrix"));
    oAttack     = g_config.getHex("dwAttack");
    oJump       = g_config.getHex("dwJump");
    oEntitySize = g_config.getHex("dwEntitySize");

    oOrigin     = g_config.getHex("m_vecOrigin");
    oViewOff    = g_config.getHex("m_vecViewOffset");
    oHealth     = g_config.getHex("m_iHealth");
    oTeam       = g_config.getHex("m_iTeamNum");
    oDormant    = g_config.getHex("m_bDormant");
    oBone       = g_config.getHex("m_dwBoneMatrix");
    oCrosshair  = g_config.getHex("m_iCrosshairId");
    oFlash      = g_config.getHex("m_flFlashDuration");
    oVelocity   = g_config.getHex("m_vecVelocity");
    oScoped     = g_config.getHex("m_bIsScoped");
    oEyeAngles  = g_config.getHex("m_angEyeAngles");

    g_aimbot   = g_config.getBool("aimbot_enabled", true);
    g_esp      = g_config.getBool("esp_enabled", true);
    g_trigger  = g_config.getBool("triggerbot", false);
    g_noRecoil = g_config.getBool("no_recoil", true);
    g_bhop     = g_config.getBool("bunny_hop", false);
    g_speedhack= g_config.getBool("speedhack", false);

    g_screenW = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    g_screenH = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));

    std::ostringstream ss;
    ss << "INIT OK | modul=" << moduleName << " base=0x" << std::hex << g_client.base
       << " size=" << std::dec << g_client.size
       << " | local=0x" << std::hex << oLocal << " list=0x" << oList
       << " view=0x" << oView << std::dec;
    game::logStatus(ss.str().c_str());
    return true;
}

void game::update() {
    g_entities.clear();
    uintptr_t base = g_client.base;
    if (!base) return;

    uintptr_t local = 0;
    if (oLocal) local = mem::read<uintptr_t>(base + oLocal);
    g_local.address = local;

    int localTeam = 0;
    Vector3 localEye;
    if (local) {
        localTeam = oTeam ? mem::read<int>(local + oTeam) : 0;
        Vector3 org = oOrigin ? mem::read<Vector3>(local + oOrigin) : Vector3{};
        Vector3 vo  = oViewOff ? mem::read<Vector3>(local + oViewOff) : Vector3{};
        localEye = org + vo;
        g_local.origin = org;
        g_local.viewOffset = vo;
        g_local.eye = localEye;
        g_local.health = oHealth ? mem::read<int>(local + oHealth) : 0;
        g_local.team = localTeam;
        g_local.alive = g_local.health > 0;
    }

    if (oView) g_viewMatrix = mem::read<ViewMatrix>(base + oView);
    uintptr_t list = oList ? mem::read<uintptr_t>(base + oList) : 0;
    if (!list || !oEntitySize) return;

    for (int i = 0; i < maxEntities; ++i) {
        uintptr_t ent = mem::read<uintptr_t>(list + static_cast<uintptr_t>(i) * oEntitySize);
        if (!ent || ent == local) continue;

        Entity e;
        e.address = ent;
        e.index = i;
        e.dormant = oDormant ? (mem::read<uint8_t>(ent + oDormant) != 0) : false;
        if (e.dormant) continue;

        e.health = oHealth ? mem::read<int>(ent + oHealth) : 100;
        e.team   = oTeam   ? mem::read<int>(ent + oTeam) : 0;
        e.alive  = e.health > 0;
        if (!e.alive) continue;
        e.enemy  = (localTeam == 0) ? true : (e.team != localTeam);

        e.origin    = oOrigin  ? mem::read<Vector3>(ent + oOrigin) : Vector3{};
        e.viewOffset= oViewOff ? mem::read<Vector3>(ent + oViewOff) : Vector3{};
        e.eye       = e.origin + e.viewOffset;
        e.feet      = e.origin;

        // Bone matrix (head/chest) — fallback ke eye bila tidak ada
        e.head = e.eye;
        if (oBone) {
            uintptr_t boneBase = mem::read<uintptr_t>(ent + oBone);
            if (boneBase) {
                int idx = g_config.getInt("aim_bone", 0) == 0 ? boneHead : boneChest;
                // tiap bone = 16 float (3x4 matrix), translasi di [12],[13],[14]
                Vector3 bp = mem::read<Vector3>(boneBase + static_cast<uintptr_t>(idx) * 64 + 48);
                if (bp.x || bp.y || bp.z) e.head = bp;
            }
        }

        // No Flash: hapus durasi flash
        if (oFlash && g_config.getBool("remove_flash", false))
            mem::write<float>(ent + oFlash, 0.0f);

        e.distance = local ? e.eye.distTo(localEye) : 0.0f;
        if (espMaxDist > 0.0f && e.distance > espMaxDist) continue;

        e.onScreen = worldToScreen(e.head, g_viewMatrix, e.sx, e.sy, g_screenW, g_screenH)
                  && worldToScreen(e.feet, g_viewMatrix, e.fx, e.fy, g_screenW, g_screenH);
        g_entities.push_back(e);
    }
}

void game::logStatus(const char* msg) {
    char path[MAX_PATH];
    GetTempPathA(MAX_PATH, path);
    std::string p = std::string(path) + "pb-cheat.log";
    std::ofstream f(p, std::ios::app);
    if (f) {
        std::time_t t = std::time(nullptr);
        f << "[" << std::ctime(&t) << "] " << msg << "\n";
    }
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}
