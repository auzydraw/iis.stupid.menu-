// Native mirrors of core Unity structs.
// Layouts match Unity's il2cpp ABI for arm64.
#pragma once
#include <cmath>
#include <cstdint>

namespace unity {

struct Vector2 {
    float x, y;
    Vector2() : x(0), y(0) {}
    Vector2(float a, float b) : x(a), y(b) {}
};

struct Vector3 {
    float x, y, z;
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float a, float b, float c) : x(a), y(b), z(c) {}

    static Vector3 zero() { return {0, 0, 0}; }
    static Vector3 up()   { return {0, 1, 0}; }
    static Vector3 fwd()  { return {0, 0, 1}; }

    Vector3 operator+(Vector3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vector3 operator-(Vector3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vector3 operator*(float s)   const { return {x * s, y * s, z * s}; }
    Vector3& operator+=(Vector3 o) { x += o.x; y += o.y; z += o.z; return *this; }

    float sqrmag() const { return x*x + y*y + z*z; }
    float mag() const    { return std::sqrt(sqrmag()); }
    Vector3 normalized() const {
        float m = mag();
        return m > 1e-6f ? Vector3{x/m, y/m, z/m} : Vector3{0, 0, 0};
    }
};

struct Quaternion {
    float x, y, z, w;
    static Quaternion identity() { return {0, 0, 0, 1}; }
};

struct Color {
    float r, g, b, a;
    static Color rgba(float r, float g, float b, float a = 1.f) { return {r, g, b, a}; }
};

enum class XRNode : int32_t {
    LeftEye       = 0,
    RightEye      = 1,
    CenterEye     = 2,
    Head          = 3,
    LeftHand      = 4,
    RightHand     = 5,
    GameController= 6,
    TrackingReference = 7,
    HardwareTracker   = 8,
};

} // namespace unity
