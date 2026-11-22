#include "ScreenAim.h"
#include "Globals.h"
#include <Windows.h>
#include <vector>
#include <algorithm>
#include <cmath>

namespace {
    struct Blob { int x, y, w, h; };
    std::vector<Blob> g_blobs;

    inline bool isRed(DWORD c) {
        int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
        return r > 150 && g < 85 && b < 85 && r > g + 60;
    }
}

void screenaim::scan() {
    g_blobs.clear();
    int W = (int)g_screenW, H = (int)g_screenH;
    HDC scr = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(scr);
    HBITMAP bmp = CreateCompatibleBitmap(scr, W, H);
    HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, W, H, scr, 0, 0, SRCCOPY);

    // grid 6px, hanya area tengah (buang HUD tepi)
    const int C = 6;
    int gw = W / C, gh = H / C;
    std::vector<unsigned char> hot(gw * gh, 0);
    for (int gy = H / 6; gy < H * 5 / 6; gy++) {
        for (int gx = W / 6; gx < W * 5 / 6; gx++) {
            int n = 0;
            for (int dy = 0; dy < C; dy += 2)
                for (int dx = 0; dx < C; dx += 2)
                    if (isRed(GetPixel(mem, gx*C+dx, gy*C+dy))) n++;
            if (n >= 2) hot[gy*gw+gx] = 1;
        }
    }
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); ReleaseDC(nullptr, scr);

    // cluster
    std::vector<unsigned char> seen(gw*gh, 0);
    for (int i = 0; i < gw*gh; i++) {
        if (!hot[i] || seen[i]) continue;
        int mnx=gw,mny=gh,mxx=0,mxy=0,cnt=0;
        std::vector<int> st; st.push_back(i); seen[i]=1;
        while (!st.empty()) {
            int c=st.back(); st.pop_back();
            int cx=c%gw, cy=c/gw;
            mnx=std::min(mnx,cx); mxx=std::max(mxx,cx);
            mny=std::min(mny,cy); mxy=std::max(mxy,cy); cnt++;
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
                int nx=cx+dx,ny=cy+dy;
                if(nx<0||ny<0||nx>=gw||ny>=gh)continue;
                int ni=ny*gw+nx;
                if(hot[ni]&&!seen[ni]){seen[ni]=1;st.push_back(ni);}
            }
        }
        int bw=(mxx-mnx+1)*C, bh=(mxy-mny+1)*C;
        // nama musuh: lebar 20..180, tinggi 5..28
        if(cnt>=2 && bw>=20 && bw<=180 && bh>=5 && bh<=28)
            g_blobs.push_back({mnx*C, mny*C, bw, bh});
    }
}

bool screenaim::target(float& ox, float& oy) {
    if (g_blobs.empty()) return false;
    float cx=g_screenW*0.5f, cy=g_screenH*0.5f, best=1e9f;
    Blob pick{};
    for (auto& b : g_blobs) {
        float x=b.x+b.w*0.5f, y=b.y+b.h*0.5f;
        float d=std::sqrt((x-cx)*(x-cx)+(y-cy)*(y-cy));
        if(d<best){best=d;pick=b;}
    }
    ox = pick.x + pick.w*0.5f;
    oy = pick.y + pick.h + 18.0f;   // badan di bawah nama
    return true;
}
