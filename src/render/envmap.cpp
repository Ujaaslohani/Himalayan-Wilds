#include <cstring>
#include <atomic>
#include "envmap.h"
#include "../gfx/gfx.h"
#include "../world/terrain.h"
#include "../core/noise.h"
#include "../platform/platform.h"
#include "../world/forest.h"
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <algorithm>

namespace envmap {
namespace {
const int N = 256;
GLuint g_env = 0, g_horA = 0, g_horB = 0, g_detail = 0, g_water = 0;  // g_water: RG16F 512^2, r = water surface (ground where dry), g = still(1)/flowing(0.6) water mask
const int ND = 1024;
std::vector<Stamp> g_treeStamps, g_lastOcc;
std::vector<float> g_treeOcc;  // trees and rocks never move: their shade is rasterised once and reused on every rebake
std::vector<FireLight> g_lastLights;
bool g_detailBaked = false;
std::vector<float> g_haze, g_canopyC, g_openC, g_wetC, g_flowX, g_flowZ;
std::vector<uint8_t> g_surf;  // SURF_* per texel (512 x 512, ~2.4 m)
const int NS = 512;  // CPU copies of baked channels
float g_cull = 230;
float g_camY = 0;
const float HMIN = -100, HRANGE = 1000;

// exact CPU replica of the shader's vnoise() (same 256x256 table as render::init, bilinear + smoothstep)
std::vector<uint8_t> g_ntab;
float vnoiseCPU(float px, float py) {
  if (g_ntab.empty()) {
    g_ntab.resize(256 * 256);
    for (int y = 0; y < 256; y++)
      for (int x = 0; x < 256; x++) {
        float a = std::fmod(x * 123.34f, 1.0f), b = std::fmod(y * 456.21f, 1.0f);
        float d = a * (a + 45.32f) + b * (b + 45.32f);
        a += d; b += d;
        float v = a * b;
        v -= std::floor(v);
        g_ntab[y * 256 + x] = (uint8_t)(v * 255.0f + 0.5f);
      }
  }
  float ix = std::floor(px), iy = std::floor(py), fx = px - ix, fy = py - iy;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  int x0 = ((int)ix) & 255, y0 = ((int)iy) & 255, x1 = (x0 + 1) & 255, y1 = (y0 + 1) & 255;
  auto T = [](int x, int y) { return g_ntab[y * 256 + x] / 255.0f; };
  return (T(x0, y0) * (1 - fx) + T(x1, y0) * fx) * (1 - fy) + (T(x0, y1) * (1 - fx) + T(x1, y1) * fx) * fy;
}
std::vector<uint8_t> g_grassBase;
std::atomic<bool> g_grassReady{false};  // bare-earth noise per texel (static), building footprints are added on each rebake

float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
float sst(float x, float a, float b) { float t = clamp01((x - a) / (b - a)); return t * t * (3 - 2 * t); }

float gridH(float x, float z) {
  const std::vector<float>& G = world::heightGrid();
  float gx = (x - GRID_MIN) / GRID_STEP, gz = (z - GRID_MIN) / GRID_STEP;
  gx = std::max(0.0f, std::min(GRID_N - 1.001f, gx));
  gz = std::max(0.0f, std::min(GRID_N - 1.001f, gz));
  int ix = (int)gx, iz = (int)gz;
  float fx = gx - ix, fz = gz - iz;
  const float* r0 = &G[(size_t)iz * GRID_N + ix];
  const float* r1 = r0 + GRID_N;
  return (r0[0] * (1 - fx) + r0[1] * fx) * (1 - fz) + (r1[0] * (1 - fx) + r1[1] * fx) * fz;
}
float texX(int i) { return GRID_MIN + (i + 0.5f) * GRID_SIZE / N; }

void blur(std::vector<float>& v, int r) {
  std::vector<float> t(v.size());
  for (int pass = 0; pass < 2; pass++) {
    for (int j = 0; j < N; j++)
      for (int i = 0; i < N; i++) {
        float s = 0; int c = 0;
        for (int k = -r; k <= r; k++) { int ii = pass ? i : std::max(0, std::min(N - 1, i + k)), jj = pass ? std::max(0, std::min(N - 1, j + k)) : j; s += v[(size_t)jj * N + ii]; c++; }
        t[(size_t)j * N + i] = s / c;
      }
    v.swap(t);
  }
}

GLuint makeTex(GLint ifmt, GLenum type, const void* px) {
  GLuint t;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, ifmt, N, N, 0, GL_RGBA, type, px);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return t;
}
}  // namespace

void build() {
  auto t0 = std::chrono::steady_clock::now();
  const size_t NN = (size_t)N * N;
  std::vector<float> H(NN), canopy(NN), mist(NN), haze(NN), vis(NN), hor(NN * 8);
  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) H[(size_t)j * N + i] = gridH(texX(i), texX(j));

  // horizon angles in 8 directions (precomputed terrain shadowing for any sun position) + sky visibility
  const float PI_ = 3.14159265f;
  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) {
      float x = texX(i), z = texX(j), h0 = H[(size_t)j * N + i] + 0.6f;
      float occ = 0;
      for (int k = 0; k < 8; k++) {
        float dx = std::cos(k * PI_ / 4), dz = std::sin(k * PI_ / 4), best = 0;
        for (float d = 3; d < 900; d *= 1.18f) {
          float t = (gridH(x + dx * d, z + dz * d) - h0) / d;
          if (t > best) best = t;
        }
        float a = std::atan(best);
        hor[((size_t)j * N + i) * 8 + k] = a / (PI_ * 0.5f);
        occ += std::sin(a);
      }
      vis[(size_t)j * N + i] = 1 - 0.9f * occ / 8;
    }

  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) {
      size_t id = (size_t)j * N + i;
      float x = texX(i), z = texX(j), h = H[id];
      canopy[id] = world::forestDensity(x, z);
      // distance to open water and the water's level
      float dw = 1e9f, wl = h;
      RiverHit q = world::riverQuery(x, z);
      if (q.d < 1e8f) { dw = q.d - q.hw; wl = q.wy; }
      for (const Lake& L : world::LAKES) {
        float d = world::lakeDist(L, x, z) - L.r;
        if (d < dw) { dw = d; wl = L.level; }
      }
      float wet = 1 - sst(dw, 0, 55);
      float low = 1 - sst(h - wl, 1.5f, 12);
      float marsh = sst(fbm(x * 0.018f + 3.1f, z * 0.018f - 7.7f, 2, 141), -0.5f, 0.4f);
      mist[id] = wet * low * (0.45f + 0.55f * marsh);
      float patch = sst(fbm(x * 0.0055f - 1.3f, z * 0.0055f + 8.2f, 2, 131), -0.45f, 0.45f);
      float alt = 1 - sst(h, 35, 170);
      haze[id] = std::max(0.12f, std::min(1.0f, (0.18f + 0.5f * canopy[id] + 0.35f * mist[id] + 0.32f * patch) * (0.4f + 0.6f * alt)));
    }
  // water flow: along the river's course, faster in rapids (from the river's own slope data); lakes are still
  g_flowX.assign(NN, 0);
  g_flowZ.assign(NN, 0);
  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) {
      float x = texX(i), z = texX(j);
      RiverHit q = world::riverQuery(x, z);
      if (q.d > q.hw + 1 || q.d > 1e8f) continue;
      float sp = (0.6f + 1.8f * q.flow) * (1 - 0.6f * (q.d / std::max(1.0f, q.hw)));  // slower at the banks
      g_flowX[(size_t)j * N + i] = q.tx * sp;
      g_flowZ[(size_t)j * N + i] = q.tz * sp;
    }
  // walking surfaces: water, trails and roads, scree and high rock, bare earth patches, else grass
  g_surf.assign((size_t)NS * NS, SURF_GRASS);
  for (int j = 0; j < NS; j++)
    for (int i = 0; i < NS; i++) {
      float x = GRID_MIN + (i + 0.5f) * GRID_SIZE / NS, z = GRID_MIN + (j + 0.5f) * GRID_SIZE / NS;
      float h = gridH(x, z);
      uint8_t sf = SURF_GRASS;
      if (world::waterHeightAt(x, z) > h + 0.05f) sf = SURF_WATER;
      else if (world::pathMask(x, z) > 0.4f || h > 60 || world::slopeAt(x, z) > 0.75f) sf = SURF_STONE;
      else {
        float e1 = vnoiseCPU(x * 0.05f + 11.0f, z * 0.05f + 11.0f) * 0.6f + vnoiseCPU(x * 0.27f + 2.0f, z * 0.27f + 2.0f) * 0.4f;
        if (e1 > 0.66f) sf = SURF_EARTH;
      }
      g_surf[(size_t)j * NS + i] = sf;
    }
  {
    // water surface map: lets the ground shader put moss on lake and river beds and keep dry-ground layers
    // (litter, dirt, grass texture) out from under the water
    const int NW = 512;
    std::vector<uint16_t> wpx((size_t)NW * NW * 2);
    auto half = [](float f) {  // float -> IEEE half
      uint32_t x; std::memcpy(&x, &f, 4);
      uint32_t sign = (x >> 16) & 0x8000, e = ((x >> 23) & 0xff), m = x & 0x7fffff;
      int ee = (int)e - 127 + 15;
      if (ee <= 0) return (uint16_t)sign;
      if (ee >= 31) return (uint16_t)(sign | 0x7c00);
      return (uint16_t)(sign | (ee << 10) | (m >> 13));
    };
    for (int j = 0; j < NW; j++)
      for (int i = 0; i < NW; i++) {
        float x = GRID_MIN + (i + 0.5f) * GRID_SIZE / NW, z = GRID_MIN + (j + 0.5f) * GRID_SIZE / NW;
        float g = gridH(x, z), lvl = g, mask = 0;
        for (const Lake& L : world::LAKES)
          if (world::lakeDist(L, x, z) < L.r + 1.0f) { lvl = L.level; mask = 1; }
        if (mask == 0) {
          RiverHit q = world::riverQuery(x, z);
          if (q.d < q.hw + 1.5f) { lvl = q.wy; mask = 0.6f; }
        }
        wpx[((size_t)j * NW + i) * 2] = half(lvl);
        wpx[((size_t)j * NW + i) * 2 + 1] = half(mask);
      }
    glGenTextures(1, &g_water);
    glBindTexture(GL_TEXTURE_2D, g_water);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, NW, NW, 0, GL_RG, GL_HALF_FLOAT, wpx.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    gfx::addBufferBytes((long long)NW * NW * 4);
  }
  blur(mist, 2);
  blur(haze, 4);
  blur(canopy, 2);
  for (size_t id = 0; id < NN; id++) vis[id] *= 1 - 0.38f * canopy[id];  // forest floor: the canopy blocks much of the sky
  g_haze = haze;
  g_canopyC = canopy;
  g_openC = vis;
  g_wetC = mist;

  std::vector<uint16_t> env(NN * 4);
  std::vector<uint8_t> ha(NN * 4), hb(NN * 4);
  for (size_t id = 0; id < NN; id++) {
    env[id * 4 + 0] = (uint16_t)std::lround(clamp01(haze[id]) * 65535);
    env[id * 4 + 1] = (uint16_t)std::lround(clamp01(mist[id]) * 65535);
    env[id * 4 + 2] = (uint16_t)std::lround(clamp01(vis[id]) * 65535);
    env[id * 4 + 3] = (uint16_t)std::lround(clamp01((H[id] - HMIN) / HRANGE) * 65535);
    for (int k = 0; k < 4; k++) {
      ha[id * 4 + k] = (uint8_t)std::lround(clamp01(hor[id * 8 + k]) * 255);
      hb[id * 4 + k] = (uint8_t)std::lround(clamp01(hor[id * 8 + 4 + k]) * 255);
    }
  }
  g_env = makeTex(GL_RGBA16, GL_UNSIGNED_SHORT, env.data());
  g_horA = makeTex(GL_RGBA8, GL_UNSIGNED_BYTE, ha.data());
  g_horB = makeTex(GL_RGBA8, GL_UNSIGNED_BYTE, hb.data());
  glBindTexture(GL_TEXTURE_2D, 0);
  gfx::addBufferBytes((long long)NN * 16);
  std::printf("envmap baked in %.0f ms\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
}

void bakeDetail(const std::vector<Stamp>& occ, const std::vector<FireLight>& lights) {
  auto t0 = std::chrono::steady_clock::now();
  if (g_treeStamps.empty()) {
    // trees: dark ring at the trunk, softer shade under the crown (stands in for far shadows the shadow map can't reach)
    forest::forEachItem(3, [](float x, float, float z, float sc, const std::string& f) {
      float crown = f.find("Pine") != std::string::npos ? 2.8f : f.find("birch") != std::string::npos ? 2.6f : f.find("rhodo") != std::string::npos ? 2.2f : 3.6f;
      g_treeStamps.push_back({x, z, crown * sc, crown * sc, 0, 0.34f, true});
      g_treeStamps.push_back({x, z, 0.9f * sc, 0.9f * sc, 0, 0.45f, true});
    });
    forest::forEachItem(2, [](float x, float, float z, float sc, const std::string& f) {
      float r = f.find("Log") != std::string::npos ? 2.6f : 1.1f;
      g_treeStamps.push_back({x, z, r * sc, r * sc, 0, 0.3f, true});
    });
  }
  const float texel = GRID_SIZE / ND;
  std::vector<float> occl((size_t)ND * ND, 1.0f), fire((size_t)ND * ND, 0.0f), fy((size_t)ND * ND, 0.0f);
  auto stamp = [&](const Stamp& s) {
    float R = std::max(s.hx, s.hz) * 1.5f + 1.5f;
    int i0 = std::max(0, (int)((s.x - R - GRID_MIN) / texel)), i1 = std::min(ND - 1, (int)((s.x + R - GRID_MIN) / texel));
    int j0 = std::max(0, (int)((s.z - R - GRID_MIN) / texel)), j1 = std::min(ND - 1, (int)((s.z + R - GRID_MIN) / texel));
    float c = std::cos(s.rot), sn = std::sin(s.rot);
    for (int j = j0; j <= j1; j++)
      for (int i = i0; i <= i1; i++) {
        float dx = GRID_MIN + (i + 0.5f) * texel - s.x, dz = GRID_MIN + (j + 0.5f) * texel - s.z;
        float o;
        if (s.circle) {
          float d = std::sqrt(dx * dx + dz * dz) / s.hx;
          o = 1 - sst(d, 0.35f, 1.15f);
        } else {
          // building: full shade inside the footprint, fading out ~1.5 m from the walls
          float lx = std::fabs(dx * c - dz * sn) - s.hx, lz = std::fabs(dx * sn + dz * c) - s.hz;
          float d = std::sqrt(std::max(lx, 0.0f) * std::max(lx, 0.0f) + std::max(lz, 0.0f) * std::max(lz, 0.0f));
          o = 1 - sst(d, 0.0f, 1.6f);
        }
        float& v = occl[(size_t)j * ND + i];
        v *= 1 - s.k * o;
      }
  };
  if (g_treeOcc.empty()) {
    for (const Stamp& s : g_treeStamps) stamp(s);
    g_treeOcc = occl;
  } else occl = g_treeOcc;
  for (const Stamp& s : occ) stamp(s);
  for (const FireLight& L : lights) {
    int i0 = std::max(0, (int)((L.x - L.radius - GRID_MIN) / texel)), i1 = std::min(ND - 1, (int)((L.x + L.radius - GRID_MIN) / texel));
    int j0 = std::max(0, (int)((L.z - L.radius - GRID_MIN) / texel)), j1 = std::min(ND - 1, (int)((L.z + L.radius - GRID_MIN) / texel));
    for (int j = j0; j <= j1; j++)
      for (int i = i0; i <= i1; i++) {
        float dx = GRID_MIN + (i + 0.5f) * texel - L.x, dz = GRID_MIN + (j + 0.5f) * texel - L.z;
        float d = std::sqrt(dx * dx + dz * dz) / L.radius;
        if (d >= 1) continue;
        float w = L.power * (1 - d) * (1 - d);
        size_t id = (size_t)j * ND + i;
        if (w > fire[id] * 0.5f) fy[id] = fire[id] > 0 ? std::max(fy[id], L.y) : L.y;
        fire[id] = std::min(1.0f, fire[id] + w);
      }
  }
  if (g_grassBase.empty()) {
    g_grassBase.resize((size_t)ND * ND);
    for (int j = 0; j < ND; j++)
      for (int i = 0; i < ND; i++) {
        float x = GRID_MIN + (i + 0.5f) * texel, z = GRID_MIN + (j + 0.5f) * texel;
        float e1 = vnoiseCPU(x * 0.05f + 11.0f, z * 0.05f + 11.0f) * 0.6f + vnoiseCPU(x * 0.27f + 2.0f, z * 0.27f + 2.0f) * 0.4f;
        g_grassBase[(size_t)j * ND + i] = (uint8_t)std::lround(clamp01(e1) * 255);
      }
    g_grassReady.store(true, std::memory_order_release);
  }
  std::vector<uint8_t> grass = g_grassBase;
  for (const Stamp& s : occ) {  // no grass growing through floors and walls
    if (s.circle) continue;
    float R = std::max(s.hx, s.hz) + 1;
    int i0 = std::max(0, (int)((s.x - R - GRID_MIN) / texel)), i1 = std::min(ND - 1, (int)((s.x + R - GRID_MIN) / texel));
    int j0 = std::max(0, (int)((s.z - R - GRID_MIN) / texel)), j1 = std::min(ND - 1, (int)((s.z + R - GRID_MIN) / texel));
    float c = std::cos(s.rot), sn = std::sin(s.rot);
    for (int j = j0; j <= j1; j++)
      for (int i = i0; i <= i1; i++) {
        float dx = GRID_MIN + (i + 0.5f) * texel - s.x, dz = GRID_MIN + (j + 0.5f) * texel - s.z;
        if (std::fabs(dx * c - dz * sn) < s.hx && std::fabs(dx * sn + dz * c) < s.hz) grass[(size_t)j * ND + i] = 255;
      }
  }
  std::vector<uint8_t> px((size_t)ND * ND * 4);
  for (size_t id = 0; id < (size_t)ND * ND; id++) {
    px[id * 4 + 0] = (uint8_t)std::lround(clamp01(occl[id]) * 255);
    px[id * 4 + 1] = (uint8_t)std::lround(clamp01(fire[id]) * 255);
    px[id * 4 + 2] = (uint8_t)std::lround(clamp01((fy[id] - HMIN) / HRANGE) * 255);
    px[id * 4 + 3] = grass[id];
  }
  if (!g_detail) {
    glGenTextures(1, &g_detail);
    glBindTexture(GL_TEXTURE_2D, g_detail);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ND, ND, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gfx::addBufferBytes((long long)ND * ND * 4);
  }
  glBindTexture(GL_TEXTURE_2D, g_detail);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ND, ND, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glBindTexture(GL_TEXTURE_2D, 0);
  g_detailBaked = true;
  std::printf("detail map baked in %.0f ms (%zu tree/rock stamps, %zu occluders, %zu lights)\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(), g_treeStamps.size(), occ.size(), lights.size());
}

void setDetailSources(const std::vector<Stamp>& occ, const std::vector<FireLight>& lights) {
  auto sameS = [](const Stamp& a, const Stamp& b) { return a.x == b.x && a.z == b.z && a.hx == b.hx && a.hz == b.hz && a.rot == b.rot && a.k == b.k; };
  auto sameL = [](const FireLight& a, const FireLight& b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.power == b.power && a.radius == b.radius; };
  bool same = g_detailBaked && occ.size() == g_lastOcc.size() && lights.size() == g_lastLights.size();
  for (size_t i = 0; same && i < occ.size(); i++) same = sameS(occ[i], g_lastOcc[i]);
  for (size_t i = 0; same && i < lights.size(); i++) same = sameL(lights[i], g_lastLights[i]);
  if (same) return;
  g_lastOcc = occ;
  g_lastLights = lights;
  bakeDetail(occ, lights);
}

void bind() {
  if (!g_detailBaked) bakeDetail({}, {});
  glActiveTexture(GL_TEXTURE16);
  glBindTexture(GL_TEXTURE_2D, g_detail);
  glActiveTexture(GL_TEXTURE21);
  glBindTexture(GL_TEXTURE_2D, g_water);
  glActiveTexture(GL_TEXTURE13);
  glBindTexture(GL_TEXTURE_2D, g_env);
  glActiveTexture(GL_TEXTURE14);
  glBindTexture(GL_TEXTURE_2D, g_horA);
  glActiveTexture(GL_TEXTURE15);
  glBindTexture(GL_TEXTURE_2D, g_horB);
  glActiveTexture(GL_TEXTURE0);
}

float sampleC(const std::vector<float>& v, float x, float z, float def) {
  if (v.empty()) return def;
  float gx = (x - GRID_MIN) / GRID_SIZE * N - 0.5f, gz = (z - GRID_MIN) / GRID_SIZE * N - 0.5f;
  gx = std::max(0.0f, std::min(N - 1.001f, gx));
  gz = std::max(0.0f, std::min(N - 1.001f, gz));
  int ix = (int)gx, iz = (int)gz;
  float fx = gx - ix, fz = gz - iz;
  const float* r0 = &v[(size_t)iz * N + ix];
  const float* r1 = r0 + N;
  return (r0[0] * (1 - fx) + r0[1] * fx) * (1 - fz) + (r1[0] * (1 - fx) + r1[1] * fx) * fz;
}
float bareAt(float x, float z) {
  if (!g_grassReady.load(std::memory_order_acquire)) return 0;
  float t = GRID_SIZE / ND;
  float gx = (x - GRID_MIN) / t - 0.5f, gz = (z - GRID_MIN) / t - 0.5f;
  gx = std::max(0.0f, std::min(ND - 1.001f, gx));
  gz = std::max(0.0f, std::min(ND - 1.001f, gz));
  int ix = (int)gx, iz = (int)gz;
  float fx = gx - ix, fz = gz - iz;
  const uint8_t* r0 = &g_grassBase[(size_t)iz * ND + ix];
  const uint8_t* r1 = r0 + ND;
  return ((r0[0] * (1 - fx) + r0[1] * fx) * (1 - fz) + (r1[0] * (1 - fx) + r1[1] * fx) * fz) / 255.0f;
}
void flowAt(float x, float z, float& vx, float& vz) {
  vx = sampleC(g_flowX, x, z, 0);
  vz = sampleC(g_flowZ, x, z, 0);
}
Surface surfaceAt(float x, float z) {
  if (g_surf.empty()) return SURF_GRASS;
  int i = std::max(0, std::min(NS - 1, (int)((x - GRID_MIN) / GRID_SIZE * NS)));
  int j = std::max(0, std::min(NS - 1, (int)((z - GRID_MIN) / GRID_SIZE * NS)));
  return (Surface)g_surf[(size_t)j * NS + i];
}
float canopyAt(float x, float z) { return sampleC(g_canopyC, x, z, 0); }
float openAt(float x, float z) { return sampleC(g_openC, x, z, 1); }
float wetAt(float x, float z) { return sampleC(g_wetC, x, z, 0); }

float hazeAt(float x, float z) {
  if (g_haze.empty()) return 1;
  float gx = (x - GRID_MIN) / GRID_SIZE * N - 0.5f, gz = (z - GRID_MIN) / GRID_SIZE * N - 0.5f;
  gx = std::max(0.0f, std::min(N - 1.001f, gx));
  gz = std::max(0.0f, std::min(N - 1.001f, gz));
  int ix = (int)gx, iz = (int)gz;
  float fx = gx - ix, fz = gz - iz;
  const float* r0 = &g_haze[(size_t)iz * N + ix];
  const float* r1 = r0 + N;
  return (r0[0] * (1 - fx) + r0[1] * fx) * (1 - fz) + (r1[0] * (1 - fx) + r1[1] * fx) * fz;
}

void setCameraHeight(float y) { g_camY = y; }
float hazeCullDistance(float px, float pz, float dt) {
  // the shader takes the haze density ~35% of the way to each point; find the distance at which it is ~97% opaque
  // in the clearest direction, so nothing visible is ever culled
  float D = 230;
  for (int it = 0; it < 2; it++) {
    float rmin = hazeAt(px, pz);
    for (int k = 0; k < 12; k++) {
      float a = k * 0.5236f;
      rmin = std::min(rmin, hazeAt(px + std::cos(a) * D * 0.35f, pz + std::sin(a) * D * 0.35f));
    }
    float lift = 1.0f + std::max(0.0f, g_camY - 15.0f - 25.0f) / 45.0f;  // eye high above the valley: thinner haze (same as the shader)
    D = std::max(230.0f, std::min(1200.0f, 3.6f * lift / (0.0165f * std::max(0.05f, rmin))));
  }
  float target = std::round(D / 20) * 20;
  static float lx = 1e9f, lz = 1e9f;
  bool jump = std::hypot(px - lx, pz - lz) > 30;  // teleport / load: snap instead of easing
  lx = px; lz = pz;
  float k = jump ? 1.0f : 1 - std::exp(-dt * 0.5f);
  g_cull += (target - g_cull) * k;
  g_cullQ = std::round(g_cull / 10) * 10;
  return g_cull;
}
}  // namespace envmap
