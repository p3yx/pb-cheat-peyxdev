#pragma once
#include <cstdint>
#include <cmath>
#include <string>

struct Vector3 {
    float x = 0, y = 0, z = 0;
    Vector3() = default;
    Vector3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
    Vector3 operator+(const Vector3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vector3 operator-(const Vector3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vector3 operator*(float s) const { return { x * s, y * s, z * s }; }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float distTo(const Vector3& o) const { return (*this - o).length(); }
};

// Matrix 4x4, disimpan row-major 16 float
struct ViewMatrix {
    float m[4][4]{};
};

struct Entity {
    uintptr_t address = 0;
    int       index = 0;
    Vector3   origin;
    Vector3   viewOffset;
    Vector3   eye;         // origin + viewOffset
    Vector3   head;        // dari bone matrix (atau eye fallback)
    Vector3   feet;
    int       health = 0;
    int       team = 0;
    bool      dormant = true;
    bool      enemy = false;
    bool      alive = false;
    float     distance = 0.0f;
    bool      onScreen = false;
    float     sx = 0, sy = 0;      // head screen
    float     fx = 0, fy = 0;      // feet screen
    std::string name;             // opsional
};

//世界中-> layar
bool worldToScreen(const Vector3& world, const ViewMatrix& vm,
                   float& outX, float& outY, float screenW, float screenH);

float normalizeAngle(float a);
Vector3 angleTo(const Vector3& from, const Vector3& to);
