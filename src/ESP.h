#pragma once
namespace esp {
    bool init();
    void render();     // invalidates + pumps messages
    void shutdown();
    void reassertTopMost();
    void refresh();
}
