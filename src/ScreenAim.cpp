#include "ScreenAim.h"
#include "Globals.h"
#include <Windows.h>
#include <cmath>

namespace {
    struct Pt { int x, y; };
    Pt g_target{ 0,0 };
    bool g_have = false;
}

// baca langsung dari screen DC, sampling jarang supaya ringan
void screenaim::scan() {
    g_have = false;
    int W=(int)g_screenW, H=(int)g_screenH;
    HDC dc = GetDC(nullptr);
    if (!dc) return;

    int bestCount = 0, bx=0, by=0;
    const int STEP = 8;
    // area tengah layar, hindari HUD tepi
    for (int y = H/5; y < H*4/5; y += STEP) {
        for (int x = W/5; x < W*4/5; x += STEP) {
            COLORREF c = GetPixel(dc, x, y);
            int r=GetRValue(c), g=GetGValue(c), b=GetBValue(c);
            // nametag musuh = merah murni (255,0,0)
            if (r > 230 && g < 30 && b < 30) {
                // hitung seberapa banyak merah di sekitar titik ini
                int cnt=0;
                for (int dy=-8; dy<=8; dy+=4)
                    for (int dx=-8; dx<=8; dx+=4) {
                        int nx=x+dx, ny=y+dy;
                        if (nx<0||ny<0||nx>=W||ny>=H) continue;
                        COLORREF c2=GetPixel(dc,nx,ny);
                        if (GetRValue(c2)>230 && GetGValue(c2)<30 && GetBValue(c2)<30) cnt++;
                    }
                if (cnt > bestCount) { bestCount=cnt; bx=x; by=y; }
            }
        }
    }
    ReleaseDC(nullptr, dc);

    if (bestCount >= 2) {
        g_target = { bx, by };
        g_have = true;
    }
}

int screenaim::count() { return g_have ? 1 : 0; }

bool screenaim::target(float& ox, float& oy) {
    if (!g_have) return false;
    ox = (float)g_target.x;
    oy = (float)g_target.y + 14.0f;   // badan sedikit di bawah nama
    return true;
}
