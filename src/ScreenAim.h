#pragma once
namespace screenaim {
    void scan();                            // panggil periodik
    bool target(float& outX, float& outY);  // target terdekat crosshair
    int  count();
}
