#pragma once
#include "Config.h"
#include "Memory.h"
#include "Structs.h"
#include <vector>
#include <atomic>

extern Config   g_config;
extern ModuleInfo g_client;
extern ViewMatrix g_viewMatrix;
extern Entity   g_local;
extern std::vector<Entity> g_entities;

extern std::atomic<bool> g_running;
extern std::atomic<bool> g_aimbot;
extern std::atomic<bool> g_esp;
extern std::atomic<bool> g_trigger;
extern std::atomic<bool> g_noRecoil;
extern std::atomic<bool> g_bhop;
extern std::atomic<bool> g_speedhack;
extern std::atomic<bool> g_wallhack;
extern std::atomic<bool> g_quickSwitch;
extern std::atomic<bool> g_macroOn;

extern float g_screenW;
extern float g_screenH;
