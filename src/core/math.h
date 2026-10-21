#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

constexpr float PI = 3.14159265358979323846f;
constexpr float TAU = PI * 2.0f;

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float sstep(float x, float e0, float e1) {
  float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
inline float smoothT(float t) { return t * t * (3.0f - 2.0f * t); }
inline float signf(float v) { return v > 0.0f ? 1.0f : (v < 0.0f ? -1.0f : 0.0f); }
inline float hypot2(float a, float b) { return std::sqrt(a * a + b * b); }
inline float hypot3(float a, float b, float c) { return std::sqrt(a * a + b * b + c * c); }

inline float wrapAngle(float a) {
  a = std::fmod(a + PI, TAU);
  if (a < 0) a += TAU;
  return a - PI;
}
inline float turnToward(float from, float to, float maxStep) {
  float d = wrapAngle(to - from);
  if (std::fabs(d) <= maxStep) return to;
  return from + signf(d) * maxStep;
}

struct Vec2 {
  float x = 0, y = 0;
  Vec2() = default;
  Vec2(float x_, float y_) : x(x_), y(y_) {}
  Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
  float length() const { return std::sqrt(x * x + y * y); }
};

struct Vec3 {
  float x = 0, y = 0, z = 0;
  Vec3() = default;
  Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator-() const { return {-x, -y, -z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
  Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
  Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
  Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
  Vec3 mul(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
  float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
  Vec3 cross(const Vec3& o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
  float lengthSq() const { return x * x + y * y + z * z; }
  float length() const { return std::sqrt(lengthSq()); }
  Vec3 normalized() const { float l = length(); return l > 1e-12f ? (*this) / l : Vec3(0, 0, 0); }
  void normalize() { *this = normalized(); }
  float distanceTo(const Vec3& o) const { return (*this - o).length(); }
  float distanceSq(const Vec3& o) const { return (*this - o).lengthSq(); }
  static Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
};

struct Vec4 {
  float x = 0, y = 0, z = 0, w = 0;
  Vec4() = default;
  Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

struct Quat {
  float x = 0, y = 0, z = 0, w = 1;
  Quat() = default;
  Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
  static Quat axisAngle(const Vec3& axis, float angle) {
    float h = angle * 0.5f, s = std::sin(h);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(h)};
  }
  static Quat fromUnitVectors(const Vec3& from, const Vec3& to) {
    float r = from.dot(to) + 1.0f;
    Quat q;
    if (r < 1e-6f) {
      r = 0;
      if (std::fabs(from.x) > std::fabs(from.z)) q = {-from.y, from.x, 0, r};
      else q = {0, -from.z, from.y, r};
    } else {
      q = {from.y * to.z - from.z * to.y, from.z * to.x - from.x * to.z, from.x * to.y - from.y * to.x, r};
    }
    return q.normalized();
  }
  static Quat fromEulerXYZ(float ex, float ey, float ez) {
    float c1 = std::cos(ex / 2), c2 = std::cos(ey / 2), c3 = std::cos(ez / 2);
    float s1 = std::sin(ex / 2), s2 = std::sin(ey / 2), s3 = std::sin(ez / 2);
    return {s1 * c2 * c3 + c1 * s2 * s3, c1 * s2 * c3 - s1 * c2 * s3, c1 * c2 * s3 + s1 * s2 * c3, c1 * c2 * c3 - s1 * s2 * s3};
  }
  static Quat fromEulerYXZ(float ex, float ey, float ez) {
    float c1 = std::cos(ex / 2), c2 = std::cos(ey / 2), c3 = std::cos(ez / 2);
    float s1 = std::sin(ex / 2), s2 = std::sin(ey / 2), s3 = std::sin(ez / 2);
    return {s1 * c2 * c3 + c1 * s2 * s3, c1 * s2 * c3 - s1 * c2 * s3, c1 * c2 * s3 - s1 * s2 * c3, c1 * c2 * c3 + s1 * s2 * s3};
  }
  Quat operator*(const Quat& b) const {
    return {x * b.w + w * b.x + y * b.z - z * b.y, y * b.w + w * b.y + z * b.x - x * b.z,
            z * b.w + w * b.z + x * b.y - y * b.x, w * b.w - x * b.x - y * b.y - z * b.z};
  }
  Quat normalized() const {
    float l = std::sqrt(x * x + y * y + z * z + w * w);
    if (l < 1e-12f) return {0, 0, 0, 1};
    return {x / l, y / l, z / l, w / l};
  }
  Vec3 rotate(const Vec3& v) const {
    float ix = w * v.x + y * v.z - z * v.y, iy = w * v.y + z * v.x - x * v.z;
    float iz = w * v.z + x * v.y - y * v.x, iw = -x * v.x - y * v.y - z * v.z;
    return {ix * w + iw * -x + iy * -z - iz * -y, iy * w + iw * -y + iz * -x - ix * -z, iz * w + iw * -z + ix * -y - iy * -x};
  }
  static Quat slerp(const Quat& a, const Quat& bIn, float t) {
    if (t <= 0) return a;
    if (t >= 1) return bIn;
    Quat b = bIn;
    float cosH = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (cosH < 0) { b = {-b.x, -b.y, -b.z, -b.w}; cosH = -cosH; }
    if (cosH >= 1.0f) return a;
    float sqrSin = 1.0f - cosH * cosH;
    if (sqrSin <= 1e-7f) {
      float s = 1 - t;
      return Quat(s * a.x + t * b.x, s * a.y + t * b.y, s * a.z + t * b.z, s * a.w + t * b.w).normalized();
    }
    float sinH = std::sqrt(sqrSin), half = std::atan2(sinH, cosH);
    float ra = std::sin((1 - t) * half) / sinH, rb = std::sin(t * half) / sinH;
    return {a.x * ra + b.x * rb, a.y * ra + b.y * rb, a.z * ra + b.z * rb, a.w * ra + b.w * rb};
  }
};

struct Mat4 {
  float m[16];
  Mat4() { identity(); }
  void identity() { for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f; }
  float& at(int row, int col) { return m[col * 4 + row]; }
  float at(int row, int col) const { return m[col * 4 + row]; }
  static Mat4 compose(const Vec3& p, const Quat& q, const Vec3& s) {
    Mat4 r;
    float x2 = q.x + q.x, y2 = q.y + q.y, z2 = q.z + q.z;
    float xx = q.x * x2, xy = q.x * y2, xz = q.x * z2, yy = q.y * y2, yz = q.y * z2, zz = q.z * z2;
    float wx = q.w * x2, wy = q.w * y2, wz = q.w * z2;
    r.m[0] = (1 - (yy + zz)) * s.x; r.m[1] = (xy + wz) * s.x; r.m[2] = (xz - wy) * s.x; r.m[3] = 0;
    r.m[4] = (xy - wz) * s.y; r.m[5] = (1 - (xx + zz)) * s.y; r.m[6] = (yz + wx) * s.y; r.m[7] = 0;
    r.m[8] = (xz + wy) * s.z; r.m[9] = (yz - wx) * s.z; r.m[10] = (1 - (xx + yy)) * s.z; r.m[11] = 0;
    r.m[12] = p.x; r.m[13] = p.y; r.m[14] = p.z; r.m[15] = 1;
    return r;
  }
  static Mat4 translate(const Vec3& p) { Mat4 r; r.m[12] = p.x; r.m[13] = p.y; r.m[14] = p.z; return r; }
  static Mat4 scale(const Vec3& s) { Mat4 r; r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; return r; }
  static Mat4 rotY(float a) { return compose({0, 0, 0}, Quat::axisAngle({0, 1, 0}, a), {1, 1, 1}); }
  Mat4 operator*(const Mat4& b) const {
    Mat4 r;
    for (int c = 0; c < 4; c++)
      for (int rr = 0; rr < 4; rr++) {
        float s = 0;
        for (int k = 0; k < 4; k++) s += m[k * 4 + rr] * b.m[c * 4 + k];
        r.m[c * 4 + rr] = s;
      }
    return r;
  }
  Vec3 transformPoint(const Vec3& v) const {
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12], m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
  }
  Vec4 transform4(const Vec3& v, float w) const {
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * w, m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * w,
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * w, m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * w};
  }
  Vec3 transformDir(const Vec3& v) const {
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z, m[1] * v.x + m[5] * v.y + m[9] * v.z, m[2] * v.x + m[6] * v.y + m[10] * v.z};
  }
  Mat4 inverse() const {
    const float* a = m;
    Mat4 r;
    float* o = r.m;
    float a00 = a[0], a01 = a[1], a02 = a[2], a03 = a[3], a10 = a[4], a11 = a[5], a12 = a[6], a13 = a[7];
    float a20 = a[8], a21 = a[9], a22 = a[10], a23 = a[11], a30 = a[12], a31 = a[13], a32 = a[14], a33 = a[15];
    float b00 = a00 * a11 - a01 * a10, b01 = a00 * a12 - a02 * a10, b02 = a00 * a13 - a03 * a10, b03 = a01 * a12 - a02 * a11;
    float b04 = a01 * a13 - a03 * a11, b05 = a02 * a13 - a03 * a12, b06 = a20 * a31 - a21 * a30, b07 = a20 * a32 - a22 * a30;
    float b08 = a20 * a33 - a23 * a30, b09 = a21 * a32 - a22 * a31, b10 = a21 * a33 - a23 * a31, b11 = a22 * a33 - a23 * a32;
    float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    if (std::fabs(det) < 1e-20f) return Mat4();
    det = 1.0f / det;
    o[0] = (a11 * b11 - a12 * b10 + a13 * b09) * det; o[1] = (a02 * b10 - a01 * b11 - a03 * b09) * det;
    o[2] = (a31 * b05 - a32 * b04 + a33 * b03) * det; o[3] = (a22 * b04 - a21 * b05 - a23 * b03) * det;
    o[4] = (a12 * b08 - a10 * b11 - a13 * b07) * det; o[5] = (a00 * b11 - a02 * b08 + a03 * b07) * det;
    o[6] = (a32 * b02 - a30 * b05 - a33 * b01) * det; o[7] = (a20 * b05 - a22 * b02 + a23 * b01) * det;
    o[8] = (a10 * b10 - a11 * b08 + a13 * b06) * det; o[9] = (a01 * b08 - a00 * b10 - a03 * b06) * det;
    o[10] = (a30 * b04 - a31 * b02 + a33 * b00) * det; o[11] = (a21 * b02 - a20 * b04 - a23 * b00) * det;
    o[12] = (a11 * b07 - a10 * b09 - a12 * b06) * det; o[13] = (a00 * b09 - a01 * b07 + a02 * b06) * det;
    o[14] = (a31 * b01 - a30 * b03 - a32 * b00) * det; o[15] = (a20 * b03 - a21 * b01 + a22 * b00) * det;
    return r;
  }
  static Mat4 perspective(float fovyDeg, float aspect, float n, float f) {
    Mat4 r;
    float t = 1.0f / std::tan(fovyDeg * PI / 360.0f);
    for (float& v : r.m) v = 0;
    r.m[0] = t / aspect; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f);
    return r;
  }
  static Mat4 ortho(float l, float r_, float b, float t, float n, float f) {
    Mat4 r;
    r.m[0] = 2 / (r_ - l); r.m[5] = 2 / (t - b); r.m[10] = -2 / (f - n);
    r.m[12] = -(r_ + l) / (r_ - l); r.m[13] = -(t + b) / (t - b); r.m[14] = -(f + n) / (f - n);
    return r;
  }
  static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    Vec3 z = (eye - target).normalized();
    if (z.lengthSq() < 1e-8f) z = {0, 0, 1};
    Vec3 x = up.cross(z).normalized();
    if (x.lengthSq() < 1e-8f) x = Vec3(1, 0, 0);
    Vec3 y = z.cross(x);
    Mat4 r;
    r.m[0] = x.x; r.m[4] = x.y; r.m[8] = x.z;
    r.m[1] = y.x; r.m[5] = y.y; r.m[9] = y.z;
    r.m[2] = z.x; r.m[6] = z.y; r.m[10] = z.z;
    r.m[12] = -x.dot(eye); r.m[13] = -y.dot(eye); r.m[14] = -z.dot(eye);
    return r;
  }
};

struct Color {
  float r = 1, g = 1, b = 1;
  Color() = default;
  Color(float r_, float g_, float b_) : r(r_), g(g_), b(b_) {}
  static float srgbToLinear(float c) { return c < 0.04045f ? c * 0.0773993808f : std::pow(c * 0.9478672986f + 0.0521327014f, 2.4f); }
  static float linearToSrgb(float c) { return c < 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 0.41666f) - 0.055f; }
  static Color hex(const char* s) {
    if (*s == '#') s++;
    unsigned v = 0;
    for (int i = 0; i < 6 && s[i]; i++) {
      char ch = s[i];
      v = v * 16 + (unsigned)(ch >= '0' && ch <= '9' ? ch - '0' : (ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch - 'A' + 10));
    }
    return {srgbToLinear(((v >> 16) & 255) / 255.0f), srgbToLinear(((v >> 8) & 255) / 255.0f), srgbToLinear((v & 255) / 255.0f)};
  }
  Color lerp(const Color& o, float t) const { return {r + (o.r - r) * t, g + (o.g - g) * t, b + (o.b - b) * t}; }
  Color operator*(float s) const { return {r * s, g * s, b * s}; }
  Color operator+(const Color& o) const { return {r + o.r, g + o.g, b + o.b}; }
  uint32_t packSrgb(uint8_t a = 255) const {
    auto q = [](float c) { return (uint32_t)std::lround(clampf(linearToSrgb(clampf(c, 0, 1)), 0, 1) * 255.0f); };
    return q(r) | (q(g) << 8) | (q(b) << 16) | ((uint32_t)a << 24);
  }
  void getHSL(float& h, float& s, float& l) const {
    float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
    l = (mn + mx) / 2;
    if (mn == mx) { h = 0; s = 0; return; }
    float d = mx - mn;
    s = l <= 0.5f ? d / (mx + mn) : d / (2 - mx - mn);
    if (mx == r) h = (g - b) / d + (g < b ? 6 : 0);
    else if (mx == g) h = (b - r) / d + 2;
    else h = (r - g) / d + 4;
    h /= 6;
  }
  static float hue2rgb(float p, float q, float t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0f / 6) return p + (q - p) * 6 * t;
    if (t < 0.5f) return q;
    if (t < 2.0f / 3) return p + (q - p) * 6 * (2.0f / 3 - t);
    return p;
  }
  void setHSL(float h, float s, float l) {
    h = std::fmod(h, 1.0f); if (h < 0) h += 1;
    s = clampf(s, 0, 1); l = clampf(l, 0, 1);
    if (s == 0) { r = g = b = l; return; }
    float p = l <= 0.5f ? l * (1 + s) : l + s - l * s, q = 2 * l - p;
    r = hue2rgb(q, p, h + 1.0f / 3); g = hue2rgb(q, p, h); b = hue2rgb(q, p, h - 1.0f / 3);
  }
  Color offsetHSL(float dh, float ds, float dl) const {
    float h, s, l;
    getHSL(h, s, l);
    Color c;
    c.setHSL(h + dh, s + ds, l + dl);
    return c;
  }
};

struct Rng {
  uint32_t a;
  explicit Rng(uint32_t seed) : a(seed) {}
  float next() {
    a += 0x6d2b79f5u;
    uint32_t t = (a ^ (a >> 15)) * (1u | a);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return (float)((t ^ (t >> 14)) / 4294967296.0);
  }
};

float frand();
