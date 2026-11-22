#include "ScreenAim.h"
#include "Globals.h"
#include <Windows.h>

namespace {
    struct Pt { int x, y; };
    Pt g_target{ 0,0 };
    bool g_have = false;
}

// Scan area kecil di sekitar crosshair saja - ringan (<=2500 GetPixel per scan)
void screenaim::scan() {
    g_have = false;
    int W=(int)g_screenW, H=(int)g_screenH;
    HDC dc = GetDC(nullptr);
    if (!dc) return;

    // area 360x260 di tengah layar
    int x0 = W/2 - 180, x1 = W/2 + 180;
    int y0 = H/2 - 130, y1 = H/2 + 130;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W; if (y1 > H) y1 = H;

    int sx=0, sy=0, n=0;
    const int STEP = 6;
    for (int y=y0; y<y1; y+=STEP) {
        for (int x=x0; x<x1; x+=STEP) {
            COLORREF c = GetPixel(dc, x, y);
            if (GetRValue(c)>220 && GetGValue(c)<40 && GetBValue(c)<40) {
                sx+=x; sy+=y; n++;
            }
        }
    }
    ReleaseDC(nullptr, dc);

    if (n >= 3) {
        g_target = { sx/n, sy/n };
        g_have = true;
    }
}

int screenaim::count() { return g_have ? 1 : 0; }

bool screenaim::target(float& ox, float& oy) {
    if (!g_have) return false;
    ox = (float)g_target.x;
    oy = (float)g_target.y + 14.0f;
    return true;
}
