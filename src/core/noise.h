#pragma once
#include "math.h"

inline float hash2(int ix, int iz, int seed = 0) {
  uint32_t h = (uint32_t)ix * 374761393u ^ (uint32_t)iz * 668265263u ^ (uint32_t)seed * 1442695041u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return (float)(h / 4294967296.0);
}

inline float valueNoise(float x, float z, int seed = 0) {
  float fx0 = std::floor(x), fz0 = std::floor(z);
  int ix = (int)fx0, iz = (int)fz0;
  float fx = smoothT(x - fx0), fz = smoothT(z - fz0);
  float a = hash2(ix, iz, seed), b = hash2(ix + 1, iz, seed);
  float c = hash2(ix, iz + 1, seed), d = hash2(ix + 1, iz + 1, seed);
  float top = a + (b - a) * fx, bot = c + (d - c) * fx;
  return (top + (bot - top) * fz) * 2.0f - 1.0f;
}

inline float fbm(float x, float z, int octaves = 3, int seed = 0) {
  float sum = 0, amp = 1, norm = 0, f = 1;
  for (int o = 0; o < octaves; o++) {
    sum += valueNoise(x * f, z * f, seed + o * 31) * amp;
    norm += amp;
    amp *= 0.5f;
    f *= 2.03f;
  }
  return sum / norm;
}

inline float ridged(float x, float z, int octaves, int seed) {
  float sum = 0, amp = 1, norm = 0, f = 1, w = 1;
  for (int o = 0; o < octaves; o++) {
    float n = 1.0f - std::fabs(valueNoise(x * f, z * f, seed + o * 17));
    n *= n;
    sum += n * amp * w;
    w = std::min(1.0f, n * 1.6f);
    norm += amp;
    amp *= 0.5f;
    f *= 2.1f;
  }
  return sum / norm;
}

inline float angDiff(float a, float b) {
  float d = a - b;
  while (d > PI) d -= TAU;
  while (d < -PI) d += TAU;
  return d;
}
