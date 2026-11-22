#pragma once
namespace menu {
    bool init();
    void render();        // dipanggil tiap tick (menggantikan/ melengkapi esp::render)
    void shutdown();
    void toggle();
    void saveAndMinimize();
    void reloadConfig();
    bool isVisible();
    bool pointerOver();
}
