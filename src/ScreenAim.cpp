#include "ScreenAim.h"
#include "Globals.h"
#include <Windows.h>
#include <vector>
#include <algorithm>
#include <cmath>

namespace {
    struct Blob { int x, y, w, h; };
    std::vector<Blob> g_blobs;

    inline bool isRed(unsigned char b, unsigned char g, unsigned char r) {
        return r > 150 && g < 85 && b < 85 && r > g + 60;
    }
}

void screenaim::scan() {
    g_blobs.clear();
    int W = (int)g_screenW, H = (int)g_screenH;

    HDC scr = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(scr);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;          // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    std::vector<unsigned char> bits((size_t)W * H * 4);
    if (!GetDIBits(mem, (HBITMAP)GetCurrentObject(scr, OBJ_BITMAP), 0, H,
                   bits.data(), &bi, DIB_RGB_COLORS)) {
        // fallback: ambil dari screenshot DC
        HBITMAP bmp = CreateCompatibleBitmap(scr, W, H);
        HGDIOBJ old = SelectObject(mem, bmp);
        BitBlt(mem, 0, 0, W, H, scr, 0, 0, SRCCOPY);
        GetDIBits(mem, bmp, 0, H, bits.data(), &bi, DIB_RGB_COLORS);
        SelectObject(mem, old);
        DeleteObject(bmp);
    }
    DeleteDC(mem);
    ReleaseDC(nullptr, scr);

    // grid 6px, area tengah, baca langsung dari buffer (cepat)
    const int C = 6;
    int gw = W / C, gh = H / C;
    std::vector<unsigned char> hot(gw * gh, 0);
    for (int gy = H / 6; gy < H * 5 / 6; gy++) {
        for (int gx = W / 6; gx < W * 5 / 6; gx++) {
            int n = 0;
            for (int dy = 0; dy < C; dy += 3) {
                for (int dx = 0; dx < C; dx += 3) {
                    int x = gx*C+dx, y = gy*C+dy;
                    size_t idx = ((size_t)y * W + x) * 4;
                    if (isRed(bits[idx], bits[idx+1], bits[idx+2])) n++;
                }
            }
            if (n >= 2) hot[gy*gw+gx] = 1;
        }
    }

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
    oy = pick.y + pick.h + 18.0f;
    return true;
}
