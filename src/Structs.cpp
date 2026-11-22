#include "Structs.h"

bool worldToScreen(const Vector3& world, const ViewMatrix& vm,
                   float& outX, float& outY, float screenW, float screenH) {
    // clip = world * matrix (engine style, column 3 = translation)
    const float* m = &vm.m[0][0];
    float cx = m[0]  * world.x + m[4]  * world.y + m[8]  * world.z + m[12];
    float cy = m[1]  * world.x + m[5]  * world.y + m[9]  * world.z + m[13];
    float cw = m[3]  * world.x + m[7]  * world.y + m[11] * world.z + m[15];
    if (cw < 0.001f) return false;
    float inv = 1.0f / cw;
    float ndcX = cx * inv;
    float ndcY = cy * inv;
    outX = (ndcX * 0.5f + 0.5f) * screenW;
    outY = (0.5f - ndcY * 0.5f) * screenH;
    return true;
}

float normalizeAngle(float a) {
    while (a > 180.0f)  a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

Vector3 angleTo(const Vector3& from, const Vector3& to) {
    Vector3 d = to - from;
    float horiz = std::sqrt(d.x * d.x + d.y * d.y);
    float yaw   = std::atan2(d.y, d.x) * 180.0f / 3.14159265f;
    float pitch = -std::atan2(d.z, horiz) * 180.0f / 3.14159265f;
    return { pitch, yaw, 0.0f };
}
