#include "../game/engine.h"
#include "../render/sky.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include "forest.h"
#include "terrain.h"
#include "../core/noise.h"
#include "../assets/assets.h"
#include "../render/render.h"
#include "../models/shape.h"
#include "../models/temple.h"
#include "../platform/platform.h"
#include "../game/season.h"
#include "shadows.h"
#include <unordered_map>
#include <list>
#include <memory>
#include <functional>
#include <thread>
#include <cstring>
#include <atomic>
#include <algorithm>
#include <mutex>
#include <condition_variable>

namespace {

constexpr float CELL = 16;
constexpr size_t CACHE_LIMIT = 800;

enum LayerId { L_GRASS, L_UNDER, L_ROCK, L_TREE };

struct LayerDef {
  LayerId id;
  uint32_t seed;
  std::vector<std::pair<std::string, float>> files;
  int slots;
  std::function<float(float, float, float)> chance;
  float scaleLo, scaleHi;
  std::function<float()> radius;
  float fadeStart;
  float farKeep;
  float colorFloor;
};

struct Item { float x, y, z, rot, scale, keep; uint16_t file; };

std::vector<LayerDef> LAYERS;

struct CellKey {
  int layer, cx, cz;
  bool operator==(const CellKey& o) const { return layer == o.layer && cx == o.cx && cz == o.cz; }
};
struct CellHash {
  size_t operator()(const CellKey& k) const { return (size_t)((uint32_t)k.cx * 73856093u ^ (uint32_t)k.cz * 19349663u ^ (uint32_t)k.layer * 83492791u); }
};
struct CellEntry {
  std::vector<Item> items;
  std::list<CellKey>::iterator lru;
};
struct CellCache {
  std::unordered_map<CellKey, CellEntry, CellHash> cells;
  std::list<CellKey> lru;
  size_t limit = 0;
  void clear() { cells.clear(); lru.clear(); }
};
CellCache g_workCache, g_mainCache;

std::vector<Blocker> g_blockers;

bool blocked(float x, float z, const std::vector<Blocker>& list = g_blockers) {
  for (const Blocker& b : list) {
    float dx = x - b.x, dz = z - b.z;
    if (dx * dx + dz * dz < b.r * b.r) return true;
  }
  return false;
}

uint32_t cellSeed(int cx, int cz, uint32_t seed) { return (uint32_t)std::floor(hash2(cx, cz, seed) * 4294967296.0); }

const std::vector<Item>& getCell(CellCache& C, int layer, int cx, int cz) {
  CellKey key{layer, cx, cz};
  auto it = C.cells.find(key);
  if (it != C.cells.end()) {
    C.lru.splice(C.lru.end(), C.lru, it->second.lru);
    return it->second.items;
  }
  const LayerDef& L = LAYERS[layer];
  Rng rng(cellSeed(cx, cz, L.seed));
  int n = L.slots;
  float slot = CELL / n;
  float totalW = 0;
  for (auto& f : L.files) totalW += f.second;
  std::vector<Item> items;
  for (int sx = 0; sx < n; sx++)
    for (int sz = 0; sz < n; sz++) {
      float jx = rng.next(), jz = rng.next(), roll = rng.next(), pick = rng.next(), rot = rng.next(), sc = rng.next(), keep = rng.next();
      float x = cx * CELL + (sx + jx) * slot;
      float z = cz * CELL + (sz + jz) * slot;
      if (std::fabs(x) > MAP_HALF - 3 || std::fabs(z) > MAP_HALF - 3) continue;
      float y = world::getTerrainHeight(x, z);
      if (roll > L.chance(x, z, y)) continue;
      float acc = 0;
      size_t fi = 0;
      float target = pick * totalW;
      for (; fi < L.files.size() - 1; fi++) {
        acc += L.files[fi].second;
        if (target < acc) break;
      }
      items.push_back({x, y, z, rot * TAU, L.scaleLo + sc * (L.scaleHi - L.scaleLo), keep, (uint16_t)fi});
    }
  items.shrink_to_fit();
  C.lru.push_back(key);
  CellEntry e;
  e.items = std::move(items);
  e.lru = std::prev(C.lru.end());
  auto res = C.cells.emplace(key, std::move(e));
  size_t lim = C.limit ? C.limit : (size_t)(CACHE_LIMIT * std::max(1.0f, g_settings.drawDist() * g_settings.drawDist()));
  if (C.cells.size() > lim) {
    CellKey old = C.lru.front();
    C.lru.pop_front();
    C.cells.erase(old);
  }
  return res.first->second.items;
}

struct PartSlot {
  const Mesh* mesh;
  const Mesh* lod = nullptr;
  AssetMaterial mat;
  bool proc = false;
  InstBatch batch;
  InstBatch shadowBatch;
  InstBatch lodBatch;
};

struct FileSlot {
  std::string file;
  int layer;
  bool heavy = false;
  std::vector<PartSlot> parts;
  std::vector<InstLite> items;
  std::vector<InstLite> shadowItems;
  struct Range { size_t start, count; Vec3 mn, mx; };
  std::vector<Range> ranges;
  std::vector<InstLite> visible;
  int imp = -1;
  float impW = 1, impBase = 0;
  std::vector<InstLite> farItems, farVisible, midItems, midVisible;
  std::vector<Range> farRanges, midRanges;
  bool hasLod = false;
  InstBatch impBatch;
  unsigned gen = 1;
  uint64_t sigN = 0, sigF = 0, sigM = 0;
};

static void uploadShared(std::vector<PartSlot>& parts, InstBatch PartSlot::*b, const std::vector<InstLite>& v) {
  if (parts.empty()) return;
  GLuint owner = (parts[0].*b).ivbo;
  (parts[0].*b).upload(v.data(), v.size());
  for (size_t k = 1; k < parts.size(); k++) {
    InstBatch& x = parts[k].*b;
    if (x.ivbo == owner) x.count = (GLsizei)v.size();
    else x.upload(v.data(), v.size());
  }
}

std::vector<std::unique_ptr<FileSlot>> g_slots;
std::vector<std::vector<FileSlot*>> g_layerSlots;
Mesh g_procMesh[3];
bool g_dirty = true;
Vec3 g_lastPos{1e9f, 0, 1e9f};
float g_lastYaw = 0;
std::vector<CastInstance> g_casts;
int g_instances = 0;

struct Garden {
  const Asset* asset;
  std::vector<InstLite> items, nearV, farV;
  std::vector<InstBatch> batches, farBatches;
  uint64_t sig = 0;
};
const float G_MID = 14, G_BAND = 6, G_OUT = 110;
std::vector<Garden> g_garden;

const char* GARDEN_FILES[5] = {"Flower Petal.glb", "Flower Petal-LqvxG9OBOU.glb", "Flower Petal-eVE0j49ux9.glb", "Flower Petal-niuBUEJdvM.glb", "Flower Petal-tzG4JcqYWs.glb"};
const float GARDEN_W[5] = {0.22f, 0.2f, 0.2f, 0.2f, 0.18f};

MeshData buildClump(int kind, uint32_t seed) {
  Rng rnd(seed);
  auto r = [&] { return rnd.next(); };
  static const Color base = Color::hex("3a5420"), green = Color::hex("7a9a3c"), green2 = Color::hex("9db04a"), straw = Color::hex("c2ac62"), straw2 = Color::hex("d9c98c"), bloom = Color::hex("f1ece0"), bloomCore = Color::hex("e8c547");
  std::vector<VtxP3C4N4> verts;
  std::vector<uint32_t> idx;
  int blades = kind == 1 ? 20 : kind == 2 ? 16 : 24;
  float dryShare = kind == 1 ? 0.75f : kind == 2 ? 0.3f : 0.18f;
  auto push = [&](float x, float y, float z, float nx, float ny, float nz, const Color& c) {
    VtxP3C4N4 v;
    v.p[0] = x; v.p[1] = y; v.p[2] = z;
    v.c = c.packSrgb();
    v.n[0] = (int8_t)std::lround(nx * 127); v.n[1] = (int8_t)std::lround(ny * 127); v.n[2] = (int8_t)std::lround(nz * 127); v.n[3] = 0;
    verts.push_back(v);
  };
  MeshData d;
  for (int b = 0; b < blades; b++) {
    float a = r() * TAU, rr = std::sqrt(r()) * 0.2f;
    float bx = std::cos(a) * rr, bz = std::sin(a) * rr;
    float h = kind == 1 ? 0.32f + r() * 0.34f : kind == 2 ? 0.16f + r() * 0.2f : 0.18f + r() * 0.26f;
    float w = 0.006f + r() * 0.006f;
    float face = r() * PI;
    float fx = std::cos(face), fz = std::sin(face);
    float lean = (0.15f + r() * 0.45f) * h;
    float la = a + (r() - 0.5f) * 1.2f;
    float lx = std::cos(la) * lean, lz = std::sin(la) * lean;
    bool dry = r() < dryShare;
    Color c = (dry ? straw : green).lerp(dry ? straw2 : green2, r());
    Color tip = c.lerp(dry ? straw2 : straw, dry ? 0.5f : 0.35f + r() * 0.3f);
    uint32_t bi = (uint32_t)verts.size();
    const int rows = 4;
    for (int k = 0; k < rows; k++) {
      float t = (float)k / (rows - 1);
      float y = h * t;
      float off = t * t;
      float cx = bx + lx * off, cz = bz + lz * off;
      float ww = k == rows - 1 ? 0 : w * (1 - t * 0.75f);
      Color shade = base.lerp(c, std::min(1.0f, t * 1.6f)).lerp(tip, std::max(0.0f, t - 0.6f) * 2.5f);
      float nx = -fz * 0.35f, nz = fx * 0.35f;
      float nl = hypot3(nx, 0.9f, nz);
      if (ww > 0) {
        push(cx - fx * ww, y, cz - fz * ww, nx / nl, 0.9f / nl, nz / nl, shade);
        push(cx + fx * ww, y, cz + fz * ww, nx / nl, 0.9f / nl, nz / nl, shade);
      } else push(cx, y, cz, nx / nl, 0.9f / nl, nz / nl, shade);
    }
    for (int k = 0; k < rows - 2; k++) {
      uint32_t i0 = bi + k * 2;
      idx.insert(idx.end(), {i0, i0 + 1, i0 + 2, i0 + 1, i0 + 3, i0 + 2});
    }
    uint32_t i0 = bi + (rows - 2) * 2;
    idx.insert(idx.end(), {i0, i0 + 1, i0 + 2});
    if (kind == 2 && b < 4) {
      float tx = bx + lx, tz = bz + lz;
      uint32_t hb = (uint32_t)verts.size();
      for (int p = 0; p < 5; p++) {
        float pa = (p / 5.0f) * TAU;
        push(tx + std::cos(pa) * 0.022f, h + 0.004f, tz + std::sin(pa) * 0.022f, 0, 1, 0, bloom);
      }
      push(tx, h + 0.008f, tz, 0, 1, 0, bloomCore);
      for (int p = 0; p < 5; p++) idx.insert(idx.end(), {hb + 5, hb + p, hb + (uint32_t)((p + 1) % 5)});
    }
  }
  d.fmt = VFmt::P3C4N4;
  d.vertexCount = (int)verts.size();
  d.verts.resize(verts.size() * sizeof(VtxP3C4N4));
  std::memcpy(d.verts.data(), verts.data(), d.verts.size());
  d.idx = idx;
  for (auto& v : verts) {
    d.bmin = Vec3(std::min(d.bmin.x, v.p[0]), std::min(d.bmin.y, v.p[1]), std::min(d.bmin.z, v.p[2]));
    d.bmax = Vec3(std::max(d.bmax.x, v.p[0]), std::max(d.bmax.y, v.p[1]), std::max(d.bmax.z, v.p[2]));
  }
  return d;
}

void defineLayers() {
  LAYERS.clear();
  LAYERS.push_back({L_GRASS, 101, {{"proc:lush", 0.5f}, {"proc:dry", 0.36f}, {"proc:bloom", 0.14f}}, 22,
                    [](float x, float z, float h) -> float {
                      if (h < -0.7f || h > 96 || world::slopeAt(x, z) > 0.9f || world::nearRoute(x, z) < 3.4f) return 0;
                      if (world::riverMask(x, z) > 0.6f || world::lakeAt(x, z) >= 0) return 0;
                      float village = clampf((hypot2(x, z) - 6) / 6, 0, 1);
                      return (1 - world::pathMask(x, z)) * village;
                    },
                    0.75f, 1.2f, [] { return g_settings.fullMap ? 140.0f : g_settings.grassRadius; }, 12, 0.2f, 0.45f});
  LAYERS.push_back({L_UNDER, 202,
                    {{"Fern.glb", 0.22f}, {"Plant.glb", 0.14f}, {"Plant Big.glb", 0.1f}, {"Plant-xH5gNlQxAZ.glb", 0.06f}, {"Bush.glb", 0.1f}, {"Bush with Flowers.glb", 0.08f},
                     {"Flower Group.glb", 0.1f}, {"Flower Group-LqTljN6Wg2.glb", 0.05f}, {"Flower Single.glb", 0.07f}, {"Mushroom.glb", 0.015f}, {"Mushroom Laetiporus.glb", 0.01f}},
                    5,
                    [](float x, float z, float h) -> float {
                      if (h < -0.5f || h > 88 || world::pathDistance(x, z) < 2 || world::nearRoute(x, z) < 3.6f || hypot2(x, z) < SETTLEMENT_RADIUS || world::slopeAt(x, z) > 0.8f) return 0;
                      if (world::riverMask(x, z) > 0.3f || world::lakeAt(x, z, 1) >= 0) return 0;
                      return 0.35f + 0.5f * world::forestDensity(x, z);
                    },
                    0.45f, 0.9f, [] { return g_settings.fullMap ? 260.0f : g_settings.undergrowthRadius; }, 32, 0.3f, 0.6f});
  LAYERS.push_back({L_ROCK, 303, {{"Rock Medium.glb", 1}}, 2,
                    [](float x, float z, float h) -> float {
                      if (h < -0.8f || world::pathDistance(x, z) < 3.5f || hypot2(x, z) < 14) return 0;
                      if (world::nearRoute(x, z) < 5.5f) return 0;
                      for (const Shrine& sh : world::SHRINES) if (hypot2(x - sh.x, z - sh.z) < COURT + 10) return 0;
                      if (world::lakeAt(x, z, 1) >= 0) return 0;
                      return (world::pathDistance(x, z) < 6 ? 0.45f : 0.22f) + std::min(0.45f, world::slopeAt(x, z) * 0.35f);
                    },
                    0.3f, 1.0f, [] { return g_settings.rockRadius * g_settings.drawDist(); }, 70, 0.6f, -1});
  LAYERS.push_back({L_TREE, 404,
                    {{"ptree:oak", 0.24f}, {"ptree:maple", 0.2f}, {"ptree:birch", 0.16f}, {"ptree:Pine-cedar", 0.28f}, {"ptree:rhodo", 0.12f}},
                    3,
                    [](float x, float z, float h) -> float {
                      if (h < -0.3f || h > 84 || world::slopeAt(x, z) > 0.85f || world::nearRoute(x, z) < 4.5f) return 0;
                      if (world::pathDistance(x, z) < 3.5f) return 0;
                      if (world::riverMask(x, z) > 0.05f) return 0;
                      if (world::lakeAt(x, z, 3) >= 0) return 0;
                      if (hypot2(x, z) < SETTLEMENT_RADIUS + 6) return 0;
                      return 0.05f + world::forestDensity(x, z) * 0.3f;
                    },
                    0.75f, 1.1f, [] { return g_settings.treeRadius * g_settings.drawDist(); }, 55, 0.55f, -1});
}

const char* LAYER_NAMES[4] = {"grass", "under", "rock", "tree"};

void gardenSpots(std::vector<std::array<float, 6>>& out) {
  Rng rnd(4242);
  auto r = [&] { return rnd.next(); };
  auto pick = [&] {
    float v = r(), acc = 0;
    for (int i = 0; i < 5; i++) { acc += GARDEN_W[i]; if (v < acc) return i; }
    return 0;
  };
  for (const Shrine& sh : world::SHRINES) {
    float c = std::cos(sh.yaw), sn = std::sin(sh.yaw);
    auto put = [&](float lx, float lz, bool inside) {
      float x = sh.x + lx * c + lz * sn, z = sh.z - lx * sn + lz * c;
      float y = inside ? sh.y + 0.12f : world::getTerrainHeight(x, z);
      float ox = (r() - 0.5f) * 0.4f, oz = (r() - 0.5f) * 0.4f, rot = r() * TAU, s = 1.1f + r() * 0.6f;
      int f = pick();
      out.push_back({x + ox, y, z + oz, rot, s, (float)f});
    };
    float E = COURT - 1.3f;
    for (float t = -E + 4; t <= E - 4; t += 0.7f)
      for (float d : {0.0f, 0.7f}) {
        put(-E + d + (r() - 0.5f) * 0.4f, t, true);
        put(E - d + (r() - 0.5f) * 0.4f, t, true);
        if (std::fabs(t) < 8.5f) put(t, -E + d + (r() - 0.5f) * 0.4f, true);
      }
    for (int sx = -1; sx <= 1; sx += 2)
      for (float t = 4.2f; t <= E - 4; t += 0.7f) put(sx * t, E - 0.3f + (r() - 0.5f) * 0.4f, true);
    for (int k = 0; k < 90; k++) {
      float a = (k / 44.0f) * TAU + r() * 0.1f;
      float rr = COURT * 1.12f + 1 + r() * 3;
      float lx = std::cos(a) * rr, lz = std::sin(a) * rr;
      if (lz > 0 && std::fabs(lx) < 4.5f) continue;
      put(lx, lz, false);
    }
  }
}

std::atomic<bool> g_impReady{false};
const float IMP_BAND = 16;
const float MID_BAND = 5, CANOPY_BAND = 30;
std::atomic<float> g_budgetScale{1.0f};
float effScale() { return std::min(g_settings.detailScale, g_budgetScale.load()); }
float midNearS(float ds) { return clampf(15 * g_settings.drawDist(), 20, 55) * ds; }
float impostorNearS(float ds) { return clampf(20 * g_settings.drawDist(), 30, 110) * (0.5f + 0.5f * ds); }
float midNear() { return midNearS(effScale()); }
float impostorNear() { return impostorNearS(effScale()); }
float canopyNear() { return clampf(140 * g_settings.drawDist(), 200, 420); }
float impostorOuter() { return canopyNear() + CANOPY_BAND; }

struct LodCfg { float mid, imp, outer; bool canopy; };
LodCfg lodForS(int layer, float ds) {
  float dd = g_settings.drawDist();
  if (layer == L_TREE) return {midNearS(ds), impostorNearS(ds), impostorOuter(), true};
  if (layer == L_ROCK) {
    float o = g_settings.fullMap ? 700.0f : std::max(260.0f, g_settings.rockRadius * dd * 1.6f);
    return {clampf(14 * dd, 18, 45) * ds, clampf(40 * dd, 50, 140) * (0.5f + 0.5f * ds), o, false};
  }
  float o = g_settings.fullMap ? 320.0f : std::max(90.0f, g_settings.undergrowthRadius * std::max(1.0f, dd));
  return {std::max(10.0f, clampf(8 * dd, 10, 24) * ds), std::max(16.0f, clampf(20 * dd, 25, 60) * (0.5f + 0.5f * ds)), o, false};
}
LodCfg lodFor(int layer) { return lodForS(layer, effScale()); }

struct BuildJob {
  float px, pz, fx, fz;
  std::vector<Blocker> blockers;
  std::vector<std::vector<InstLite>> items, shadow, far, mid;
  std::vector<CastInstance> casts;
  std::vector<std::vector<FileSlot::Range>> rN, rF, rM;
};

std::thread g_worker;
std::mutex g_jobMtx;
std::condition_variable g_jobCv;
bool g_jobPending = false, g_jobDone = false, g_quit = false;
BuildJob g_job;

void compute(BuildJob& J) {
  size_t ns = g_slots.size();
  J.items.assign(ns, {});
  J.shadow.assign(ns, {});
  J.far.assign(ns, {});
  J.mid.assign(ns, {});
  J.casts.clear();
  std::unordered_map<const FileSlot*, size_t> index;
  for (size_t i = 0; i < ns; i++) index[g_slots[i].get()] = i;
  float px = J.px, pz = J.pz;
  for (const LayerDef& layer : LAYERS) {
    if (layer.id == L_GRASS) continue;
    float R = layer.id == L_GRASS ? std::max(16.0f, layer.radius() * (1 - 0.45f * season.snow)) : layer.radius();
    bool impLayer = layer.id != L_GRASS && g_impReady;
    LodCfg lc = lodFor(layer.id);
    if (impLayer) R = lc.outer;
    float nearD = lc.imp, midD = lc.mid;
    float F = std::min(layer.fadeStart, R * 0.8f);
    float R2 = R * R;
    int c0x = (int)std::floor((px - R) / CELL), c1x = (int)std::floor((px + R) / CELL);
    int c0z = (int)std::floor((pz - R) / CELL), c1z = (int)std::floor((pz + R) / CELL);
    bool caster = layer.id == L_TREE || layer.id == L_ROCK;
    const auto& slots = g_layerSlots[layer.id];
    for (int cx = c0x; cx <= c1x; cx++)
      for (int cz = c0z; cz <= c1z; cz++) {
        float ccx = (cx + 0.5f) * CELL - px, ccz = (cz + 0.5f) * CELL - pz;
        if (ccx * ccx + ccz * ccz > (R + CELL) * (R + CELL)) continue;
        const auto& cell = getCell(g_workCache, layer.id, cx, cz);
        for (const Item& it : cell) {
          float dx = it.x - px, dz = it.z - pz;
          float d2 = dx * dx + dz * dz;
          if (d2 > R2) continue;
          float d = std::sqrt(d2);
          FileSlot* slot = it.file < slots.size() ? slots[it.file] : nullptr;
          if (!slot) continue;
          float rot = it.rot;
          if (layer.id == L_UNDER) {
            float keepT = d <= F ? 1 : 1 - (1 - layer.farKeep) * ((d - F) / (R - F));
            float fade = clampf((keepT - it.keep) / 0.05f, 0, 1);
            if (fade <= 0.04f) continue;
            rot += (15 - std::round(fade * 15)) * TAU;
          }
          if (blocked(it.x, it.z, J.blockers)) continue;
          InstLite inst{it.x, it.y, it.z, it.scale, rot};
          size_t si = index[slot];
          if (impLayer && slot->imp >= 0) {
            const float M = 20.0f;
            if (d > nearD - M) J.far[si].push_back(inst);
            if (slot->hasLod) {
              if (d > midD - M && d < nearD + IMP_BAND + M) J.mid[si].push_back(inst);
              if (d > midD + MID_BAND + M) {
                if (caster && d < 46) J.shadow[si].push_back(inst);
                continue;
              }
            } else if (d > nearD + IMP_BAND + M) continue;
          }
          if (caster && d < 46) J.shadow[si].push_back(inst);
          if (caster && d < 85 && J.casts.size() < 700) {
            bool tree = layer.id == L_TREE;
            J.casts.push_back(castInstance(it.x, it.y, it.z, tree ? 2.3f * inst.s : 1.0f * inst.s, tree ? 8 * inst.s : 0.8f * inst.s));
          }
          J.items[si].push_back(inst);
        }
      }
  }
}

const float CHUNK = 32;
inline int chunkKey(float x, float z) { return ((int)std::floor(x / CHUNK) + 64) * 256 + ((int)std::floor(z / CHUNK) + 64); }

void sortChunks(BuildJob& J) {
  for (auto& v : J.mid)
    std::sort(v.begin(), v.end(), [](const InstLite& a, const InstLite& b) { return chunkKey(a.x, a.z) < chunkKey(b.x, b.z); });
  for (auto& v : J.far)
    std::sort(v.begin(), v.end(), [](const InstLite& a, const InstLite& b) { return chunkKey(a.x, a.z) < chunkKey(b.x, b.z); });
  for (auto& v : J.items)
    std::sort(v.begin(), v.end(), [](const InstLite& a, const InstLite& b) { return chunkKey(a.x, a.z) < chunkKey(b.x, b.z); });
}

void prewarm() {
  int n = (int)std::ceil(MAP_HALF / CELL);
  for (int layer : {L_TREE, L_ROCK, L_UNDER})
    for (int cx = -n; cx < n; cx++)
      for (int cz = -n; cz < n; cz++) {
        {
          std::lock_guard<std::mutex> lk(g_jobMtx);
          if (g_jobPending || g_quit) return;
        }
        getCell(g_workCache, layer, cx, cz);
      }
}

void prepRanges(BuildJob& J);
void workerLoop() {
  g_workCache.limit = 60000;
  bool warmed = false;
  std::unique_lock<std::mutex> lk(g_jobMtx);
  while (true) {
    if (g_quit) return;
    if (!warmed && !g_jobPending) {
      lk.unlock();
      prewarm();
      lk.lock();
      int n = (int)std::ceil(MAP_HALF / CELL);
      warmed = g_workCache.cells.size() >= (size_t)(3 * 4 * n * n);
      continue;
    }
    g_jobCv.wait(lk, [] { return g_jobPending || g_quit; });
    if (g_quit) return;
    BuildJob J = std::move(g_job);
    lk.unlock();
    compute(J);
    sortChunks(J);
    prepRanges(J);
    lk.lock();
    g_job = std::move(J);
    g_jobPending = false;
    g_jobDone = true;
  }
}

void buildRanges(const std::vector<InstLite>& items, std::vector<FileSlot::Range>& ranges, float hgt) {
  ranges.clear();
  for (size_t k = 0; k < items.size();) {
    int key = chunkKey(items[k].x, items[k].z);
    FileSlot::Range r{k, 0, {1e9f, 1e9f, 1e9f}, {-1e9f, -1e9f, -1e9f}};
    while (k < items.size() && chunkKey(items[k].x, items[k].z) == key) {
      const InstLite& it = items[k];
      float pad = 10 * it.s;
      r.mn = {std::min(r.mn.x, it.x - pad), std::min(r.mn.y, it.y - 1), std::min(r.mn.z, it.z - pad)};
      r.mx = {std::max(r.mx.x, it.x + pad), std::max(r.mx.y, it.y + hgt * it.s), std::max(r.mx.z, it.z + pad)};
      k++;
    }
    r.count = k - r.start;
    ranges.push_back(r);
  }
}

void prepRanges(BuildJob& J) {
  size_t n = std::min(g_slots.size(), J.items.size());
  J.rN.assign(n, {}); J.rF.assign(n, {}); J.rM.assign(n, {});
  for (size_t i = 0; i < n; i++) {
    int L = g_slots[i]->layer;
    float hgt = L == L_TREE ? 26.0f : L == L_ROCK ? 4.0f : 2.0f;
    buildRanges(J.items[i], J.rN[i], hgt);
    if (i < J.far.size()) buildRanges(J.far[i], J.rF[i], hgt);
    if (i < J.mid.size()) buildRanges(J.mid[i], J.rM[i], hgt);
  }
}

void apply(BuildJob& J) {
  sky::invalidateStaticShadow();
  g_casts = std::move(J.casts);
  g_instances = 0;
  for (size_t i = 0; i < g_slots.size() && i < J.items.size(); i++) {
    auto& s = g_slots[i];
    s->items = std::move(J.items[i]);
    s->shadowItems = std::move(J.shadow[i]);
    s->ranges = std::move(J.rN[i]);
    if (i < J.far.size()) { s->farItems = std::move(J.far[i]); s->farRanges = std::move(J.rF[i]); }
    else { s->farItems.clear(); s->farRanges.clear(); }
    if (i < J.mid.size()) { s->midItems = std::move(J.mid[i]); s->midRanges = std::move(J.rM[i]); }
    else { s->midItems.clear(); s->midRanges.clear(); }
    uploadShared(s->parts, &PartSlot::shadowBatch, s->shadowItems);
    s->gen++;
    g_instances += (int)s->items.size();
  }
}

void rebuild(float px, float pz, float fx, float fz) {
  BuildJob J{px, pz, fx, fz, g_blockers, {}, {}, {}, {}, {}};
  compute(J);
  sortChunks(J);
  prepRanges(J);
  apply(J);
}

std::unordered_map<int, char> g_occ;
int g_occFrame = -1;
int g_culledOcc = 0;

bool chunkVisible(const FileSlot::Range& r) {
  const Camera& cam = render::cam;
  if (!cam.box(r.mn, r.mx)) return false;
  float cx = (r.mn.x + r.mx.x) * 0.5f, cz = (r.mn.z + r.mx.z) * 0.5f;
  float dist = hypot2(cx - cam.pos.x, cz - cam.pos.z);
  if (dist < 90) return true;
  int key = chunkKey(cx, cz);
  auto it = g_occ.find(key);
  if (it != g_occ.end()) return it->second != 0;
  Vec3 pts[5] = {{cx, r.mx.y, cz}, {r.mn.x, r.mx.y, r.mn.z}, {r.mx.x, r.mx.y, r.mx.z}, {r.mn.x, r.mx.y, r.mx.z}, {r.mx.x, r.mx.y, r.mn.z}};
  bool vis = false;
  for (auto& q : pts) {
    int steps = (int)clampf(dist / 10, 6, 48);
    bool blocked = false;
    for (int k = 1; k < steps && !blocked; k++) {
      float t = (float)k / steps;
      float x = cam.pos.x + (q.x - cam.pos.x) * t, y = cam.pos.y + (q.y - cam.pos.y) * t, z = cam.pos.z + (q.z - cam.pos.z) * t;
      if (world::getTerrainHeight(x, z) > y + 4.0f) blocked = true;
    }
    if (!blocked) { vis = true; break; }
  }
  if (!vis) g_culledOcc++;
  g_occ[key] = vis ? 1 : 0;
  return vis;
}

bool gatherVisible(const std::vector<InstLite>& items, const std::vector<FileSlot::Range>& ranges, std::vector<InstLite>& out, unsigned gen, uint64_t& sig, float minD = 0, float maxD = 1e9f) {
  static std::vector<uint32_t> vis;
  vis.clear();
  uint64_t h = 1469598103934665603ull ^ gen;
  const Vec3& cp = render::cam.pos;
  for (size_t i = 0; i < ranges.size(); i++) {
    const auto& r = ranges[i];
    float nx = clampf(cp.x, r.mn.x, r.mx.x) - cp.x, nz = clampf(cp.z, r.mn.z, r.mx.z) - cp.z;
    if (nx * nx + nz * nz > maxD * maxD) continue;
    float fx = std::max(std::fabs(r.mn.x - cp.x), std::fabs(r.mx.x - cp.x)), fz = std::max(std::fabs(r.mn.z - cp.z), std::fabs(r.mx.z - cp.z));
    if (minD > 0 && fx * fx + fz * fz < minD * minD) continue;
    if (chunkVisible(r)) { h = (h ^ (i + 1)) * 1099511628211ull; vis.push_back((uint32_t)i); }
  }
  int qx = (int)std::floor(cp.x * 2), qz = (int)std::floor(cp.z * 2);
  h = (h ^ ((uint64_t)(uint32_t)qx * 73856093ull)) * 1099511628211ull;
  h = (h ^ ((uint64_t)(uint32_t)qz * 19349663ull)) * 1099511628211ull;
  h = (h ^ (uint64_t)(uint32_t)(int)(minD * 4)) * 1099511628211ull;
  h = (h ^ (uint64_t)(uint32_t)(int)(std::min(maxD, 1e6f) * 4)) * 1099511628211ull;
  if (h == sig) return false;
  sig = h;
  out.clear();
  float lo2 = minD > 2 ? (minD - 2) * (minD - 2) : 0, hi2 = maxD < 1e8f ? (maxD + 2) * (maxD + 2) : 1e18f;
  for (uint32_t i : vis)
    for (size_t k = ranges[i].start; k < ranges[i].start + ranges[i].count; k++) {
      const InstLite& it = items[k];
      float dx = it.x - cp.x, dz = it.z - cp.z, d2 = dx * dx + dz * dz;
      if (d2 >= lo2 && d2 <= hi2) out.push_back(it);
    }
  if (minD <= 0 && out.size() > 1 && out.size() < 20000) {
    std::sort(out.begin(), out.end(), [&](const InstLite& a, const InstLite& b) {
      float da = (a.x - cp.x) * (a.x - cp.x) + (a.z - cp.z) * (a.z - cp.z), db = (b.x - cp.x) * (b.x - cp.x) + (b.z - cp.z) * (b.z - cp.z);
      return da < db;
    });
  }
  return true;
}

Vec3 g_occPos{1e9f, 0, 0};
int g_occAge = 0;

extern bool g_impMergeDirty;
extern Mesh g_impQuad;
void rebuildImpostorMerge(int layer);
void cullVisible() {
  const Vec3& cp = render::cam.pos;
  if (++g_occAge > 20 || hypot2(cp.x - g_occPos.x, cp.z - g_occPos.z) > 3 || std::fabs(cp.y - g_occPos.y) > 2) {
    g_occ.clear();
    g_occPos = cp;
    g_occAge = 0;
  }
  g_culledOcc = 0;
  g_instances = 0;
  bool impDirty[4] = {g_impMergeDirty, g_impMergeDirty, g_impMergeDirty, g_impMergeDirty};
  g_impMergeDirty = false;
  for (auto& s : g_slots) {
    LodCfg lc = lodFor(s->layer);
    bool useImp = g_impReady && s->imp >= 0;
    float nearMax = !useImp ? 1e9f : s->hasLod ? lc.mid + MID_BAND : lc.imp + IMP_BAND;
    if (gatherVisible(s->items, s->ranges, s->visible, s->gen, s->sigN, 0, nearMax)) uploadShared(s->parts, &PartSlot::batch, s->visible);
    g_instances += (int)s->visible.size();
    if (s->imp >= 0) {
      if (gatherVisible(s->farItems, s->farRanges, s->farVisible, s->gen, s->sigF, lc.imp)) impDirty[s->layer] = true;
      if (s->hasLod) {
        if (gatherVisible(s->midItems, s->midRanges, s->midVisible, s->gen, s->sigM, lc.mid, lc.imp + IMP_BAND)) uploadShared(s->parts, &PartSlot::lodBatch, s->midVisible);
        g_instances += (int)s->midVisible.size();
      }
    }
  }
  if (g_impQuad.vao)
    for (int L = 0; L < 4; L++) if (impDirty[L] && L != L_GRASS) rebuildImpostorMerge(L);
  int budget = g_settings.triBudget;
  if (budget <= 0 || !g_impReady) { g_budgetScale = 1.0f; return; }
  const int NB = 200;
  static std::vector<double> full, lod;
  full.assign(4 * NB, 0); lod.assign(4 * NB, 0);
  const Vec3& cp2 = render::cam.pos;
  for (auto& s : g_slots) {
    if (s->layer == L_GRASS) continue;
    double ft = 0, lt = 0;
    for (auto& p : s->parts) { ft += p.mesh->count / 3 * (p.mat.cutout && !p.proc ? 2 : 1); lt += (p.lod ? p.lod->count : p.mesh->count) / 3; }
    int L = s->layer;
    for (auto& it : s->visible) { int k = std::min(NB - 1, (int)hypot2(it.x - cp2.x, it.z - cp2.z)); full[L * NB + k] += ft; }
    if (s->hasLod) for (auto& it : s->midVisible) { int k = std::min(NB - 1, (int)hypot2(it.x - cp2.x, it.z - cp2.z)); lod[L * NB + k] += lt; }
  }
  float cap = std::min(1.0f, g_settings.detailScale), pick = 0.3f;
  for (float sc = cap; sc >= 0.3f; sc -= 0.05f) {
    double T = 0;
    for (int L = 1; L < 4; L++) {
      LodCfg c = lodForS(L, sc);
      int m = (int)(c.mid + MID_BAND * 0.5f), im = (int)(c.imp + IMP_BAND * 0.5f);
      for (int k = 0; k < NB; k++) {
        if (k < m) T += full[L * NB + k];
        else if (k < im) T += lod[L * NB + k];
      }
    }
    if (std::getenv("HW_BUDGET") && sc == cap) std::printf("budget T(%.2f)=%.0f\n", sc, T);
    if (T <= budget) { pick = sc; if (std::getenv("HW_BUDGET")) std::printf("budget pick %.2f T=%.0f\n", sc, T); break; }
  }
  float cur = g_budgetScale.load();
  float next = pick < cur ? std::max(pick, cur - 0.05f) : std::min(pick, cur + 0.01f);
  if (std::fabs(next - cur) > 0.001f) {
    g_budgetScale = next;
    static float lastBuilt = 1.0f;
    if (std::fabs(next - lastBuilt) > 0.1f) { lastBuilt = next; g_dirty = true; }
  }
}


const int IMP_CELL_W = 128, IMP_CELL_H = 256, IMP_VIEWS = 16, IMP_ATLAS = IMP_CELL_W * IMP_VIEWS;
int IMP_ATLAS_H = 256, g_impRows = 1;
GLuint g_impFbo = 0, g_impTex = 0, g_impDepth = 0, g_impNrm = 0;
Mesh g_impQuad;
float g_impSig = -1, g_capSig = 0;
int g_capNext = -1, g_impTypes = 0;

const char* IMP_VS = R"(
layout(location=0) in vec3 a_pos;
layout(location=6) in vec4 a_i0;
layout(location=7) in float a_i1;
uniform vec4 u_cell;
uniform vec4 u_range;
out vec2 v_uv0;
out vec2 v_uv1;
out float v_blend;
out vec2 v_rot;
out vec3 v_wpos;
out float v_depth;
out vec2 v_fade;
void main(){
  vec3 base = a_i0.xyz;
  float s = a_i0.w, yaw = mod(a_i1, 6.2831853);
  vec2 tc = u_camPos.xz - base.xz;
  float d = max(length(tc), 1e-3);
  vec2 dir = tc / d;
  float cy = cos(yaw), sy = sin(yaw);
  float lx = cy * dir.x - sy * dir.y, lz = sy * dir.x + cy * dir.y;
  float a = atan(lx, lz);
  if (a < 0.0) a += 6.2831853;
  float fv = a / (6.2831853 / 16.0);
  float v0 = mod(floor(fv), 16.0), v1 = mod(v0 + 1.0, 16.0);
  v_blend = fract(fv);
  vec3 right = vec3(dir.y, 0.0, -dir.x);
  float W = s;
  vec3 p = base + right * (a_pos.x * W) + vec3(0.0, a_pos.y * 2.0 * W, 0.0);
  float row = floor(a_i1 / 6.2831853), rows = u_cell.w;
  float vv = (row + a_pos.y) / rows;
  v_uv0 = vec2((v0 + a_pos.x + 0.5) / 16.0, vv);
  v_uv1 = vec2((v1 + a_pos.x + 0.5) / 16.0, vv);
  v_rot = vec2(cy, sy);
  v_wpos = p;
  v_fade = vec2(1.0 - smoothstep(u_range.x, mix(u_range.x, u_range.y, 0.5), d), 1.0 - smoothstep(mix(u_range.z, u_range.w, 0.5), u_range.w, d));
  vec4 vp = u_view * vec4(p, 1.0);
  v_depth = -vp.z;
  gl_Position = u_proj * vp;
}
)";

const char* IMP_FS = R"(
in vec2 v_uv0;
in vec2 v_uv1;
in float v_blend;
in vec2 v_rot;
in vec3 v_wpos;
in float v_depth;
in vec2 v_fade;
uniform sampler2D u_atlas;
uniform sampler2D u_nrm;
uniform int u_dbg;
out vec4 fragColor;
void main(){
  vec4 t0 = texture(u_atlas, v_uv0), t1 = texture(u_atlas, v_uv1);
  float dh = FADE_NOISE_STATIC();
  vec4 t = mix(t0, t1, v_blend);
  if (t.a < 0.5) {
    // fill pinholes inside the crown from a blurrier mip so sky doesn't speckle through
    vec4 c = mix(texture(u_atlas, v_uv0, 2.0), texture(u_atlas, v_uv1, 2.0), v_blend);
    if (c.a < 0.62) discard;
    t = vec4((c.rgb - vec3(0.42, 0.38, 0.26) * (1.0 - c.a)) / c.a, 1.0);
  }
  if (dh < v_fade.x || dh >= v_fade.y) discard;
  if (u_dbg == 1) { fragColor = vec4(1.0, 0.0, 1.0, 1.0); return; }
  vec3 nl = mix(texture(u_nrm, v_uv0).xyz, texture(u_nrm, v_uv1).xyz, v_blend) * 2.0 - 1.0;
  vec3 n = normalize(vec3(v_rot.x * nl.x + v_rot.y * nl.z, nl.y, -v_rot.y * nl.x + v_rot.x * nl.z));
  n = normalize(mix(n, vec3(0.0, 1.0, 0.0), 0.75));
  vec3 alb = t.rgb * t.rgb;
  float sh = mix(mix(0.42, 0.9, shadowAt(v_wpos, n)) * mix(0.75, 1.0, smoothstep(0.08, 0.45, u_sunDir.y)), 0.62, 0.85);
  vec3 c = outColor(lambert(alb, n, sh));
  c = mix(c, fogTint(v_wpos), mix(fogObject(v_wpos, v_depth), fogGround(v_wpos, v_depth), smoothstep(60.0, 160.0, v_depth)));
  fragColor = vec4(c, 1.0);
}
)";

float seasonSig() {
  return std::round(season.autumn * 10) + std::round(season.snow * 10) * 31 + std::round(season.bare * 10) * 961 + std::round(season.spring * 10) * 29791 + std::round(season.dry * 10) * 923521 + std::round(season.leaf * 10) * 7;
}

InstBatch g_impAll[4];
bool g_impMergeDirty = true;
std::vector<InstLite> g_impTmp;
void rebuildImpostorMerge(int layer) {
  g_impTmp.clear();
  for (auto& s : g_slots) {
    if (s->layer != layer || s->imp < 0) continue;
    for (const InstLite& q : s->farVisible) {
      float yaw = std::fmod(q.yaw, 6.2831853f);
      if (yaw < 0) yaw += 6.2831853f;
      g_impTmp.push_back({q.x, q.y + s->impBase * q.s, q.z, s->impW * q.s, yaw + 6.2831853f * (float)s->imp});
    }
  }
  if (!g_impAll[layer].vao) g_impAll[layer].create(&g_impQuad, InstKind::Lite);
  g_impAll[layer].upload(g_impTmp.data(), g_impTmp.size());
}

void captureImpostors(int only = -1, bool mips = true, int vFrom = 0, int vTo = 16) {
  g_impMergeDirty = true;
  if (!g_impFbo) {
    int types = 0;
    for (auto& sp : g_slots) if (sp->layer != L_GRASS && !sp->parts.empty()) types++;
    g_impRows = std::max(1, std::min(32, types));
    IMP_ATLAS_H = g_impRows * IMP_CELL_H;
    auto mk = [](GLuint& tex) {
      glGenTextures(1, &tex);
      glBindTexture(GL_TEXTURE_2D, tex);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, IMP_ATLAS, IMP_ATLAS_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 4);
      gfx::addBufferBytes((long long)IMP_ATLAS * IMP_ATLAS_H * 4 * 4 / 3);
    };
    mk(g_impTex);
    mk(g_impNrm);
    glGenRenderbuffers(1, &g_impDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, g_impDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, IMP_ATLAS, IMP_ATLAS_H);
    glGenFramebuffers(1, &g_impFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_impFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_impTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g_impNrm, 0);
    const GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, bufs);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_impDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      return;
    }
    MeshData md;
    md.fmt = VFmt::P3C4;
    const float Q[6][2] = {{-0.5f, 0}, {0.5f, 0}, {0.5f, 1}, {-0.5f, 0}, {0.5f, 1}, {-0.5f, 1}};
    for (auto& q : Q) {
      VtxP3C4 v{{q[0], q[1], 0}, 0xffffffffu};
      const uint8_t* b = (const uint8_t*)&v;
      md.verts.insert(md.verts.end(), b, b + sizeof v);
    }
    for (uint32_t i = 0; i < 6; i++) md.idx.push_back(i);
    md.vertexCount = 6;
    md.bmin = {-0.5f, 0, 0};
    md.bmax = {0.5f, 1, 0};
    g_impQuad = uploadMesh(md);
  }
  GLint vp[4];
  glGetIntegerv(GL_VIEWPORT, vp);
  GLint prevFbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, g_impFbo);
  glViewport(0, 0, IMP_ATLAS, IMP_ATLAS_H);
  const GLfloat cc0[4] = {0.42f, 0.38f, 0.26f, 0}, cc1[4] = {0.5f, 1.0f, 0.5f, 0};
  auto clr = [&]() { glClearBufferfv(GL_COLOR, 0, cc0); glClearBufferfv(GL_COLOR, 1, cc1); glClear(GL_DEPTH_BUFFER_BIT); };
  if (only < 0) clr();
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glEnable(GL_SCISSOR_TEST);
  FrameUBO saved = render::frame;
  render::frame.season2[3] = 0;
  int t = 0;
  for (auto& sp : g_slots) {
    if (sp->layer == L_GRASS || sp->parts.empty() || t >= g_impRows) continue;
    if (only >= 0 && t != only) { t++; continue; }
    Vec3 mn{1e9f, 1e9f, 1e9f}, mx{-1e9f, -1e9f, -1e9f};
    for (auto& p : sp->parts) {
      mn = {std::min(mn.x, p.mesh->bmin.x), std::min(mn.y, p.mesh->bmin.y), std::min(mn.z, p.mesh->bmin.z)};
      mx = {std::max(mx.x, p.mesh->bmax.x), std::max(mx.y, p.mesh->bmax.y), std::max(mx.z, p.mesh->bmax.z)};
    }
    float rxz = std::max(std::max(std::fabs(mn.x), std::fabs(mx.x)), std::max(std::fabs(mn.z), std::fabs(mx.z)));
    float h = mx.y - mn.y;
    float W = std::max(2 * rxz, h * 0.5f) * 1.02f;
    sp->imp = t;
    sp->impW = W;
    sp->impBase = mn.y;
    Vec3 c{0, mn.y + W, 0};
    for (int v = only >= 0 ? vFrom : 0; v < (only >= 0 ? vTo : IMP_VIEWS); v++) {
      float a = v * TAU / IMP_VIEWS;
      Vec3 d{std::sin(a), 0, std::cos(a)};
      Mat4 view = Mat4::lookAt(c + d * 60.0f, c, {0, 1, 0});
      Mat4 proj = Mat4::ortho(-W / 2, W / 2, -W, W, 1, 160);
      Mat4 vpM = proj * view;
      std::memcpy(render::frame.view, view.m, 64);
      std::memcpy(render::frame.proj, proj.m, 64);
      std::memcpy(render::frame.viewProj, vpM.m, 64);
      Vec3 eye = c + d * 60.0f;
      render::frame.cam[0] = eye.x; render::frame.cam[1] = eye.y; render::frame.cam[2] = eye.z;
      render::uploadFrame();
      int col = v, row = t;
      glViewport(col * IMP_CELL_W, row * IMP_CELL_H, IMP_CELL_W, IMP_CELL_H);
      glScissor(col * IMP_CELL_W, row * IMP_CELL_H, IMP_CELL_W, IMP_CELL_H);
      if (only >= 0) clr();
      for (auto& p : sp->parts) {
        Shader& s = render::objectFor(p.mat, IM_NONE, OF_CAPTURE);
        render::setModel(s, Mat4());
        p.mesh->draw();
      }
    }
    if (!sp->impBatch.vao) sp->impBatch.create(&g_impQuad, InstKind::Lite);
    t++;
  }
  glDisable(GL_SCISSOR_TEST);
  render::frame = saved;
  render::uploadFrame();
  if (mips) {
    glBindTexture(GL_TEXTURE_2D, g_impTex);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_impNrm);
    glGenerateMipmap(GL_TEXTURE_2D);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
  glViewport(vp[0], vp[1], vp[2], vp[3]);
  if (only < 0) g_impSig = seasonSig();
  g_impTypes = t;
}


Mesh g_canopy;
const float CANOPY_STEP = 5.0f;

const char* CANOPY_VS = R"(
layout(location=0) in vec3 a_pos;
layout(location=1) in vec4 a_col;
layout(location=2) in vec4 a_nrm;
out vec3 v_wpos;
out vec4 v_c;
out vec3 v_n;
out float v_depth;
flat out float v_t;
void main(){
  v_wpos = a_pos;
  v_c = a_col;
  v_t = a_col.b;
  v_n = normalize(a_nrm.xyz);
  vec4 vp = u_view * vec4(a_pos, 1.0);
  v_depth = -vp.z;
  gl_Position = u_proj * vp;
}
)";

const char* CANOPY_FS = R"(
in vec3 v_wpos;
in vec4 v_c;
in vec3 v_n;
in float v_depth;
flat in float v_t;
uniform vec2 u_in;
uniform float u_row[16];
uniform float u_rows;
uniform sampler2D u_atlas;
out vec4 fragColor;
void main(){
  float cov = v_c.r;
  if (cov < 0.24 + 0.16 * vnoise(v_wpos.xz * 0.22)) discard;
  float d = distance(v_wpos.xz, u_camPos.xz);
  float fin = smoothstep(u_in.x, u_in.y, d);
  float dh = FADE_NOISE_STATIC();
  if (dh < 1.0 - fin) discard;
  float pine = v_c.g, rnd = v_c.b;
  float n1 = fract(sin(dot(floor(v_wpos.xz * 0.35), vec2(41.3, 289.1))) * 9631.7);
  vec3 summer = mix(vec3(0.16, 0.27, 0.07), vec3(0.24, 0.33, 0.09), n1);
  vec3 autumn = mix(mix(vec3(0.55, 0.16, 0.04), vec3(0.62, 0.32, 0.05), rnd), vec3(0.5, 0.42, 0.08), n1 * 0.5);
  vec3 bare = vec3(0.13, 0.1, 0.085);
  vec3 dec = mix(summer, autumn, u_autumn);
  dec = mix(dec, bare, u_bare * 0.8);
  dec = mix(dec, dec * vec3(0.9, 1.15, 0.8), u_spring);
  vec3 pin = mix(vec3(0.06, 0.15, 0.08), vec3(0.09, 0.19, 0.1), n1);
  vec3 alb = mix(dec, pin, pine);
  // match the real tree colour: average of this tree type's impostor (captured with current season)
  int ti = int(v_t * 255.0);
  float row = ti < 16 ? u_row[ti] : -1.0;
  if (row >= 0.0) {
    vec4 a = textureLod(u_atlas, vec2(0.5, (row + 0.62) / u_rows), 6.0);
    if (a.a > 0.05) { vec3 tc = (a.rgb - vec3(0.42, 0.38, 0.26) * (1.0 - a.a)) / a.a; tc = tc * tc * 0.85; alb = mix(alb, tc, 0.75); }
  }
  // individual crowns: lumpy tops with sunlit and shaded sides
  vec2 cq = v_wpos.xz * 0.28;
  float crown = vnoise(cq) * 0.6 + vnoise(cq * 2.3 + 4.0) * 0.4;
  vec3 n = normalize(v_n);
  vec3 cg = vec3(vnoise(cq + vec2(0.35, 0.0)) - vnoise(cq - vec2(0.35, 0.0)), 0.0, vnoise(cq + vec2(0.0, 0.35)) - vnoise(cq - vec2(0.0, 0.35)));
  n = normalize(n - cg * 1.6);
  alb *= mix(0.72, 1.12, crown);
  float snowP = smoothstep(0.35, 0.75, fract(sin(dot(floor(v_wpos.xz * 0.6), vec2(17.1, 93.7))) * 3758.5));
  alb = mix(alb, vec3(0.8, 0.83, 0.88), u_snow * smoothstep(0.55, 0.95, n.y) * mix(0.15, 0.45, snowP) * (1.0 - 0.5 * pine));
  alb *= (0.62 + 0.3 * n1);
  vec3 c = outColor(lambert(alb, n, 1.0) * mix(0.7, 1.0, cov));
  c = mix(c, fogTint(v_wpos), fogGround(v_wpos, v_depth));
  fragColor = vec4(c, 1.0);
}
)";

void buildCanopy() {
  const int N = (int)(MAP_HALF * 2 / CANOPY_STEP) + 1;
  std::vector<float> cov((size_t)N * N, 0), hgt((size_t)N * N, 0), pine((size_t)N * N, 0), rnd((size_t)N * N, 0);
  CellCache tmp;
  tmp.limit = 1 << 30;
  int nc = (int)std::ceil(MAP_HALF / CELL);
  const auto& slots = g_layerSlots[L_TREE];
  std::vector<float> slotH(slots.size(), 10), slotR(slots.size(), 3);
  std::vector<bool> slotPine(slots.size(), false);
  for (size_t i = 0; i < slots.size(); i++) {
    if (!slots[i]) continue;
    float mx = 0, rr = 0;
    for (auto& p : slots[i]->parts) {
      mx = std::max(mx, p.mesh->bmax.y);
      rr = std::max(rr, std::max(std::max(std::fabs(p.mesh->bmin.x), p.mesh->bmax.x), std::max(std::fabs(p.mesh->bmin.z), p.mesh->bmax.z)));
    }
    slotH[i] = mx;
    slotR[i] = rr;
    slotPine[i] = slots[i]->file.find("Pine") != std::string::npos;
  }
  for (int cx = -nc; cx < nc; cx++)
    for (int cz = -nc; cz < nc; cz++)
      for (const Item& it : getCell(tmp, L_TREE, cx, cz)) {
        if (it.file >= slots.size() || !slots[it.file] || blocked(it.x, it.z)) continue;
        float r = std::max(4.0f, slotR[it.file] * it.scale * 1.25f), h = slotH[it.file] * it.scale;
        int i0 = (int)std::floor((it.x - r + MAP_HALF) / CANOPY_STEP), i1 = (int)std::ceil((it.x + r + MAP_HALF) / CANOPY_STEP);
        int j0 = (int)std::floor((it.z - r + MAP_HALF) / CANOPY_STEP), j1 = (int)std::ceil((it.z + r + MAP_HALF) / CANOPY_STEP);
        for (int j = std::max(0, j0); j <= std::min(N - 1, j1); j++)
          for (int i = std::max(0, i0); i <= std::min(N - 1, i1); i++) {
            float x = -MAP_HALF + i * CANOPY_STEP, z = -MAP_HALF + j * CANOPY_STEP;
            float q = 1 - ((x - it.x) * (x - it.x) + (z - it.z) * (z - it.z)) / (r * r);
            if (q <= 0) continue;
            size_t k = (size_t)j * N + i;
            float w = std::sqrt(q);
            if (w > cov[k]) { pine[k] = slotPine[it.file] ? 1.0f : 0.0f; rnd[k] = (it.file + 0.5f) / 255.0f; }
            cov[k] = std::max(cov[k], w);
            hgt[k] = std::max(hgt[k], h * (0.78f + 0.22f * w));
          }
      }
  for (int pass = 0; pass < 3; pass++) {
    std::vector<float> hc = hgt, cc = cov;
    for (int j = 1; j + 1 < N; j++)
      for (int i = 1; i + 1 < N; i++) {
        size_t k = (size_t)j * N + i;
        float sh = 0, sc = 0, sw = 0;
        for (int dj = -1; dj <= 1; dj++)
          for (int di = -1; di <= 1; di++) {
            size_t q = (size_t)(j + dj) * N + (i + di);
            float w = (di == 0 && dj == 0) ? 2.0f : 1.0f;
            sc += cc[q] * w;
            if (cc[q] > 0.05f) { sh += hc[q] * w; sw += w; }
          }
        cov[k] = sc / 10.0f;
        if (sw > 0 && cc[k] > 0.05f) hgt[k] = sh / sw;
      }
  }
  std::vector<float> ys((size_t)N * N);
  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) {
      size_t k = (size_t)j * N + i;
      float x = -MAP_HALF + i * CANOPY_STEP, z = -MAP_HALF + j * CANOPY_STEP;
      float g = world::getTerrainHeight(x, z);
      ys[k] = cov[k] > 0.05f ? g + hgt[k] * 0.85f : g - 3;
    }
  MeshData md;
  md.fmt = VFmt::P3C4N4;
  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) {
      size_t k = (size_t)j * N + i;
      float x = -MAP_HALF + i * CANOPY_STEP, z = -MAP_HALF + j * CANOPY_STEP;
      float hl = ys[(size_t)j * N + std::max(0, i - 1)], hr = ys[(size_t)j * N + std::min(N - 1, i + 1)];
      float hd = ys[(size_t)std::max(0, j - 1) * N + i], hu = ys[(size_t)std::min(N - 1, j + 1) * N + i];
      Vec3 n = Vec3(hl - hr, 2 * CANOPY_STEP, hd - hu).normalized();
      VtxP3C4N4 v;
      v.p[0] = x; v.p[1] = ys[k]; v.p[2] = z;
      v.c = (uint32_t)(clampf(cov[k], 0, 1) * 255) | ((uint32_t)(pine[k] * 255) << 8) | ((uint32_t)(rnd[k] * 255) << 16) | (255u << 24);
      v.n[0] = (int8_t)(n.x * 127); v.n[1] = (int8_t)(n.y * 127); v.n[2] = (int8_t)(n.z * 127); v.n[3] = 0;
      const uint8_t* b = (const uint8_t*)&v;
      md.verts.insert(md.verts.end(), b, b + sizeof v);
      md.bmin = {std::min(md.bmin.x, x), std::min(md.bmin.y, ys[k]), std::min(md.bmin.z, z)};
      md.bmax = {std::max(md.bmax.x, x), std::max(md.bmax.y, ys[k]), std::max(md.bmax.z, z)};
    }
  md.vertexCount = N * N;
  for (int j = 0; j + 1 < N; j++)
    for (int i = 0; i + 1 < N; i++) {
      uint32_t a = j * N + i, b = a + 1, c = a + N, d = c + 1;
      if (cov[a] < 0.05f && cov[b] < 0.05f && cov[c] < 0.05f && cov[d] < 0.05f) continue;
      md.idx.insert(md.idx.end(), {a, c, b, b, c, d});
    }
  if (!md.idx.empty()) g_canopy = uploadMesh(md);
}
}  // namespace

namespace forest {

float grassDensity(float x, float z) {
  if (LAYERS.empty()) defineLayers();
  return LAYERS[L_GRASS].chance(x, z, world::getTerrainHeight(x, z));
}


std::vector<AssetRequest> requiredFiles() {
  if (LAYERS.empty()) defineLayers();
  const char* names[4] = {"grass", "under", "rock", "tree"};
  std::vector<AssetRequest> out;
  for (auto& L : LAYERS)
    for (auto& f : L.files)
      if (f.first.rfind("proc:", 0) != 0 && f.first.rfind("ptree:", 0) != 0) out.push_back({f.first, names[L.id], L.colorFloor});
  for (auto f : GARDEN_FILES) out.push_back({f, "under", -1});
  return out;
}

void init() {
  if (LAYERS.empty()) defineLayers();
  for (int k = 0; k < 3; k++) {
    const char* names[3] = {"lush", "dry", "bloom"};
    g_procMesh[k] = uploadMesh(buildClump(k, (uint32_t)(std::strlen(names[k]) * 7919 + 17)));
  }
  g_layerSlots.assign(4, {});
  for (const LayerDef& L : LAYERS) {
    for (size_t fi = 0; fi < L.files.size(); fi++) {
      const std::string& file = L.files[fi].first;
      auto slot = std::make_unique<FileSlot>();
      slot->file = file;
      slot->layer = L.id;
      if (file.rfind("proc:", 0) == 0) {
        int k = file == "proc:lush" ? 0 : file == "proc:dry" ? 1 : 2;
        PartSlot p;
        p.mesh = &g_procMesh[k];
        p.mat.name = "GrassBlades";
        p.mat.kind = SeasonKind::Grass;
        p.mat.cutout = false;
        p.proc = true;
        slot->parts.push_back(p);
      } else {
        const Asset* a = assets::get(file);
        if (!a) {
          g_layerSlots[L.id].push_back(nullptr);
          continue;
        }
        slot->heavy = a->tris > 5000;
        for (const AssetPart& ap : a->parts) {
          PartSlot p;
          p.mesh = &ap.mesh;
          p.lod = ap.lod.vao ? &ap.lod : &ap.mesh;
          p.mat = ap.mat;
          p.mat.kind = assets::kindFor(ap.mat.name, LAYER_NAMES[L.id]);
          slot->parts.push_back(p);
        }
      }
      for (size_t k = 0; k < slot->parts.size(); k++) {
        auto& p = slot->parts[k];
        auto& o = slot->parts[0];
        p.batch.create(p.mesh, InstKind::Lite, k ? o.batch.ivbo : 0);
        p.shadowBatch.create(p.lod && p.lod->vao ? p.lod : p.mesh, InstKind::Lite, k ? o.shadowBatch.ivbo : 0);
        if (L.id != L_GRASS && p.lod) {
          p.lodBatch.create(p.lod, InstKind::Lite, k && o.lodBatch.ivbo ? o.lodBatch.ivbo : 0);
          if (p.lod != p.mesh) slot->hasLod = true;
        }
      }
      if (std::getenv("HW_FPROF")) for (auto& p : slot->parts) std::printf("lod %-36s full %6d lod %6d\n", slot->file.c_str(), p.mesh->count / 3, p.lod ? p.lod->count / 3 : -1);
      g_layerSlots[L.id].push_back(slot.get());
      g_slots.push_back(std::move(slot));
    }
  }
  std::vector<std::array<float, 6>> spots;
  gardenSpots(spots);
  for (int fi = 0; fi < 5; fi++) {
    const Asset* a = assets::get(GARDEN_FILES[fi]);
    if (!a) continue;
    Garden g;
    g.asset = a;
    for (auto& s : spots)
      if ((int)s[5] == fi) g.items.push_back({s[0], s[1], s[2], s[4], s[3]});
    for (const AssetPart& ap : a->parts) {
      InstBatch b, f;
      b.create(&ap.mesh, InstKind::Lite);
      f.create(ap.lod.vao ? &ap.lod : &ap.mesh, InstKind::Lite);
      g.batches.push_back(b);
      g.farBatches.push_back(f);
    }
    g_garden.push_back(std::move(g));
  }
  buildCanopy();
}

void setBlockers(const std::vector<Blocker>& b) {
  g_blockers = b;
  g_dirty = true;
}

void markDirty() { g_dirty = true; }

void update(const Vec3& cam, const Vec3& fwd) {
  if (g_impSig < 0) {
    captureImpostors();
    if (g_impFbo) { g_impReady = true; g_dirty = true; }
  } else if (g_capNext >= 0) {
    int type = g_capNext / 4, part = g_capNext % 4;
    bool last = type + 1 >= g_impTypes && part == 3;
    captureImpostors(type, last, part * 4, part * 4 + 4);
    g_capNext++;
    if (last) { g_impSig = g_capSig; g_capNext = -1; }
  } else if (std::fabs(seasonSig() - g_impSig) > 0.5f) {
    g_capSig = seasonSig();
    g_capNext = 0;
  }
  Vec3 d(fwd.x, 0, fwd.z);
  if (d.lengthSq() < 1e-6f) d = {0, 0, -1};
  d.normalize();
  float yaw = std::atan2(d.x, d.z);
  float dYaw = std::fabs(yaw - g_lastYaw);
  if (dYaw > PI) dYaw = TAU - dYaw;
  float moved = hypot2(cam.x - g_lastPos.x, cam.z - g_lastPos.z);
  if (!g_worker.joinable()) {
    g_mainCache.limit = 60000;
    rebuild(cam.x, cam.z, d.x, d.z);
    g_lastPos = cam;
    g_lastYaw = yaw;
    g_dirty = false;
    g_worker = std::thread(workerLoop);
    cullVisible();
    return;
  }
  std::unique_lock<std::mutex> lk(g_jobMtx);
  if (g_jobDone) {
    g_jobDone = false;
    BuildJob J = std::move(g_job);
    lk.unlock();
    apply(J);
    lk.lock();
  }
  if (!g_jobPending && (g_dirty || moved > 4)) {
    g_dirty = false;
    g_lastPos = cam;
    g_lastYaw = yaw;
    g_job = BuildJob{cam.x, cam.z, d.x, d.z, g_blockers, {}, {}, {}, {}, {}};
    g_jobPending = true;
    g_jobCv.notify_one();
  }
  lk.unlock();
  cullVisible();
}

static void drawPart(PartSlot& p, InstBatch& b, int extra, float f0, float f1, float i0, float i1, bool packed) {
  if (b.count <= 0) return;
  Shader* s;
  if (p.proc) s = &render::object(VFmt::P3C4N4, IM_LITE, SeasonKind::Grass, OF_WIND | OF_DOUBLE | extra);
  else s = &render::objectFor(p.mat, IM_LITE, extra);
  s->set1f("u_packed", packed ? 1.0f : 0.0f);
  if (render::dbgMode == 2) { if (f1 > 0) f0 = f1 = (f0 + f1) * 0.5f, f1 += 0.01f; if (i1 > 0) i0 = i1 = (i0 + i1) * 0.5f, i1 += 0.01f; }
  s->set1i("u_tier", &b == &p.lodBatch ? 1 : 0);
  s->set2f("u_fade", f0, f1);
  s->set2f("u_fadeIn", i0, i1);
  b.draw();
  s->set2f("u_fade", 0, 0);
  s->set2f("u_fadeIn", 0, 0);
  s->set1f("u_packed", 0);
}

std::map<std::string, long long> g_fprof;
struct FProfPrint { ~FProfPrint() { for (auto& kv : g_fprof) std::printf("fprof %12lld %s\n", kv.second, kv.first.c_str()); } } g_fprofPrint;
float detailLevel() { return effScale(); }
int (*profOpen)(const char*) = nullptr;
void (*profClose)(int) = nullptr;
static const char* LAYER_PROF[2][4] = {{"forest.grass", "forest.under", "forest.rock", "forest.tree"}, {"forest.grassLod", "forest.underLod", "forest.rockLod", "forest.treeLod"}};
struct PrepassTuner {
  GLuint q[4][2] = {};
  bool mode[4] = {};
  bool valid[4] = {};
  int idx = 0;
  bool on = true, testing = false;
  double frameT = 0, avgOn = -1, avgOff = -1;
  int frames = 0;
  void begin() {
    if (!q[0][0]) for (auto& a : q) glGenQueries(2, a);
    int r = (idx + 1) % 4;
    if (valid[r]) {
      GLint ok = 0;
      glGetQueryObjectiv(q[r][1], GL_QUERY_RESULT_AVAILABLE, &ok);
      if (ok) {
        GLuint64 a = 0, b = 0;
        glGetQueryObjectui64v(q[r][0], GL_QUERY_RESULT, &a);
        glGetQueryObjectui64v(q[r][1], GL_QUERY_RESULT, &b);
        double ms = (b - a) / 1e6;
        double& avg = mode[r] ? avgOn : avgOff;
        avg = avg < 0 ? ms : avg * 0.9 + ms * 0.1;
      }
      valid[r] = false;
    }
    frames++;
    int period = 600, test = 45;
    int ph = frames % period;
    bool best = avgOff < 0 || avgOn < 0 ? (ph < period / 2) : avgOn <= avgOff;
    testing = ph >= period - test;
    on = testing ? !best : best;
    if (avgOff < 0 && frames > 60 && frames < 120) on = false;
    if (const char* f = std::getenv("HW_PREPASS")) on = f[0] == '1';
    mode[idx] = on;
    glQueryCounter(q[idx][0], GL_TIMESTAMP);
  }
  void end() {
    glQueryCounter(q[idx][1], GL_TIMESTAMP);
    valid[idx] = true;
    idx = (idx + 1) % 4;
  }
} g_tuner;

void drawOpaque() {
  glDisable(GL_CULL_FACE);
  bool imp = g_impReady;
  g_tuner.begin();
  bool pre = false;  // depth prepass caused speckles on NVIDIA (prepass/colour depth mismatch)
  int pD = profOpen ? profOpen("forest.depthPre") : -1;
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  for (auto& s : g_slots) {
    if (!pre) break;
    if (s->layer == L_GRASS) continue;
    LodCfg lc = lodFor(s->layer);
    bool packed = s->layer == L_UNDER;
    bool useImp = imp && s->imp >= 0;
    for (auto& p : s->parts) {
      if (!p.mat.cutout || p.proc) continue;
      float R = LAYERS[s->layer].radius();
      if (!useImp) drawPart(p, p.batch, OF_DEPTH, R * 0.84f, R, 0, 0, packed);
      else if (s->hasLod) drawPart(p, p.batch, OF_DEPTH, lc.mid, lc.mid + MID_BAND, 0, 0, packed);
      else drawPart(p, p.batch, OF_DEPTH, lc.imp, lc.imp + IMP_BAND, 0, 0, packed);
    }
  }
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthFunc(GL_LEQUAL);
  if (profClose) profClose(pD);
  static bool noUnder = std::getenv("HW_NOUNDER") != nullptr;
  for (auto& s : g_slots) {
    if (noUnder && s->layer == L_UNDER) continue;
    LodCfg lc = lodFor(s->layer);
    bool packed = s->layer == L_UNDER;
    bool useImp = imp && s->imp >= 0;
    long long t0_ = gfx::triangles;
    for (auto& p : s->parts) {
      float R = LAYERS[s->layer].radius();
      int li = s->layer < 4 ? s->layer : 3;
      int pi = profOpen ? profOpen(LAYER_PROF[0][li]) : -1;
      if (!useImp) drawPart(p, p.batch, 0, R * 0.84f, R, 0, 0, packed);
      else if (s->hasLod) drawPart(p, p.batch, 0, lc.mid, lc.mid + MID_BAND, 0, 0, packed);
      else drawPart(p, p.batch, 0, lc.imp, lc.imp + IMP_BAND, 0, 0, packed);
      if (profClose) profClose(pi);
      if (useImp && s->hasLod) {
        pi = profOpen ? profOpen(LAYER_PROF[1][li]) : -1;
        drawPart(p, p.lodBatch, 0, lc.imp, lc.imp + IMP_BAND, lc.mid, lc.mid + MID_BAND, packed);
        if (profClose) profClose(pi);
      }
    }
    if (std::getenv("HW_FPROF")) g_fprof[s->file] += gfx::triangles - t0_;
  }
  glDepthFunc(GL_LESS);
  g_tuner.end();
  int pImp = profOpen ? profOpen("forest.impostor") : -1;
  if (imp) {
    Shader& sh = render::program("impostor", IMP_VS, IMP_FS);
    sh.set1i("u_atlas", 0);
    sh.set1i("u_nrm", 5);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, g_impNrm);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_impTex);
    for (int L = 0; L < 4; L++) {
      if (g_impAll[L].count <= 0) continue;
      LodCfg lc = lodFor(L);
      float cn = canopyNear();
      float ib = render::dbgMode == 2 ? 0.01f : IMP_BAND, i0 = render::dbgMode == 2 ? lc.imp + IMP_BAND * 0.5f : lc.imp;
      sh.set1i("u_dbg", render::dbgMode);
      if (lc.canopy) sh.set4f("u_range", i0, i0 + ib, cn, cn + CANOPY_BAND);
      else sh.set4f("u_range", i0, i0 + ib, lc.outer * 0.85f, lc.outer);
      sh.set4f("u_cell", 0, 0, 0, (float)g_impRows);
      g_impAll[L].draw();
    }
    if (profClose) profClose(pImp);
    pImp = profOpen ? profOpen("forest.canopy") : -1;
    if (g_canopy.vao) {
      Shader& cs = render::program("canopy", CANOPY_VS, CANOPY_FS);
      cs.set2f("u_in", canopyNear(), canopyNear() + CANOPY_BAND);
      cs.set1i("u_atlas", 0);
      cs.set1f("u_rows", (float)g_impRows);
      {
        float rows[16];
        for (int i = 0; i < 16; i++) rows[i] = -1;
        const auto& ts = g_layerSlots[L_TREE];
        for (size_t i = 0; i < ts.size() && i < 16; i++) if (ts[i] && ts[i]->imp >= 0) rows[i] = (float)ts[i]->imp;
        glUniform1fv(cs.loc("u_row"), 16, rows);
      }
      g_canopy.draw();
    }
  }
  if (profClose) profClose(pImp);
  int pG = profOpen ? profOpen("forest.garden") : -1;
  long long tg_ = gfx::triangles;
  const Camera& cam = render::cam;
  for (auto& g : g_garden) {
    uint64_t h = 1469598103934665603ull;
    g.nearV.clear(); g.farV.clear();
    for (size_t i = 0; i < g.items.size(); i++) {
      const InstLite& it = g.items[i];
      float d = hypot2(it.x - cam.pos.x, it.z - cam.pos.z);
      if (d > G_OUT) continue;
      if (d > 5 && !cam.box({it.x - 1.5f, it.y - 0.5f, it.z - 1.5f}, {it.x + 1.5f, it.y + 2.5f, it.z + 1.5f})) continue;
      bool nr = d < G_MID + G_BAND + 2, fr = d > G_MID - 2;
      if (nr) g.nearV.push_back(it);
      if (fr) g.farV.push_back(it);
      h = (h ^ (i * 4 + nr * 2 + fr + 1)) * 1099511628211ull;
    }
    if (h != g.sig) {
      g.sig = h;
      for (size_t i = 0; i < g.batches.size(); i++) {
        g.batches[i].upload(g.nearV.data(), g.nearV.size());
        g.farBatches[i].upload(g.farV.data(), g.farV.size());
      }
    }
    for (size_t i = 0; i < g.batches.size(); i++) {
      AssetMaterial m = g.asset->parts[i].mat;
      m.kind = assets::kindFor(m.name, "under");
      Shader& sh = render::objectFor(m, IM_LITE);
      sh.set2f("u_fade", G_MID, G_MID + G_BAND);
      g.batches[i].draw();
      sh.set2f("u_fade", G_OUT * 0.85f, G_OUT);
      sh.set2f("u_fadeIn", G_MID, G_MID + G_BAND);
      g.farBatches[i].draw();
      sh.set2f("u_fade", 0, 0);
      sh.set2f("u_fadeIn", 0, 0);
    }
  }
  if (std::getenv("HW_FPROF")) g_fprof["garden"] += gfx::triangles - tg_;
  if (profClose) profClose(pG);
}

void drawShadow() {
  glDisable(GL_CULL_FACE);
  for (auto& s : g_slots) {
    if (s->layer != L_TREE && s->layer != L_ROCK) continue;
    for (auto& p : s->parts) drawPart(p, p.shadowBatch, 0, 0, 0, 0, 0, false);
  }
}

void drawBlobs() { drawCasts(g_casts); }

int instanceCount() { return g_instances; }

void resolveTreeCollision(float& x, float& z, float radius) {
  static unsigned s_frame = ~0u;
  static int s_new = 0;
  if (s_frame != (unsigned)render::frameNo) { s_frame = (unsigned)render::frameNo; s_new = 0; }
  const Vec3& pp = E.player.pos;
  bool nearPlayer = std::fabs(x - pp.x) + std::fabs(z - pp.z) < 60;
  int ccx = (int)std::floor(x / CELL), ccz = (int)std::floor(z / CELL);
  for (int cx = ccx - 1; cx <= ccx + 1; cx++)
    for (int cz = ccz - 1; cz <= ccz + 1; cz++) {
      if (!nearPlayer && !g_mainCache.cells.count(CellKey{L_TREE, cx, cz})) {
        if (s_new >= 3) continue;
        s_new++;
      }
      const auto& c = getCell(g_mainCache, L_TREE, cx, cz);
      for (const Item& it : c) {
        float r = it.scale * 0.7f + radius;
        float dx = x - it.x, dz = z - it.z;
        float d2 = dx * dx + dz * dz;
        if (d2 < r * r && d2 > 1e-6f && !blocked(it.x, it.z)) {
          float d = std::sqrt(d2);
          x = it.x + (dx / d) * r;
          z = it.z + (dz / d) * r;
        }
      }
    }
  static float rockR = -1;
  if (rockR < 0) {
    rockR = 0;
    for (auto& sp : g_slots)
      if (sp->layer == L_ROCK)
        for (auto& p : sp->parts) rockR = std::max(rockR, std::max(std::max(std::fabs(p.mesh->bmin.x), std::fabs(p.mesh->bmax.x)), std::max(std::fabs(p.mesh->bmin.z), std::fabs(p.mesh->bmax.z))));
    rockR *= 0.8f;
    if (rockR <= 0) rockR = -1;
  }
  if (rockR <= 0) return;
  for (int cx = ccx - 1; cx <= ccx + 1; cx++)
    for (int cz = ccz - 1; cz <= ccz + 1; cz++) {
      if (!g_mainCache.cells.count(CellKey{L_ROCK, cx, cz})) {
        if (!nearPlayer || s_new >= 3) continue;
        s_new++;
      }
      for (const Item& it : getCell(g_mainCache, L_ROCK, cx, cz)) {
        float rr = it.scale * rockR;
        if (rr < 0.3f) continue;
        float r = rr + radius;
        float dx = x - it.x, dz = z - it.z;
        float d2 = dx * dx + dz * dz;
        if (d2 < r * r && d2 > 1e-6f) {
          float d = std::sqrt(d2);
          x = it.x + (dx / d) * r;
          z = it.z + (dz / d) * r;
        }
      }
    }
}

void nearTrees(float x, float z, float r, std::vector<float>& out, int max) {
  struct N { float d; Item it; };
  std::vector<N> near;
  int c0x = (int)std::floor((x - r) / CELL), c1x = (int)std::floor((x + r) / CELL);
  int c0z = (int)std::floor((z - r) / CELL), c1z = (int)std::floor((z + r) / CELL);
  for (int cx = c0x; cx <= c1x; cx++)
    for (int cz = c0z; cz <= c1z; cz++)
      for (const Item& it : getCell(g_mainCache, L_TREE, cx, cz)) {
        if (it.file == 0) continue;
        float d = hypot2(it.x - x, it.z - z);
        if (d > r || blocked(it.x, it.z)) continue;
        near.push_back({d, it});
      }
  std::sort(near.begin(), near.end(), [](const N& a, const N& b) { return a.d < b.d; });
  out.clear();
  for (int k = 0; k < std::min(max, (int)near.size()); k++) {
    const Item& it = near[k].it;
    out.insert(out.end(), {it.x, it.y, it.z, it.scale});
  }
}

bool nearestRock(float x, float z, float& ox, float& oz, float& os) {
  float best = 1e9f;
  int cx0 = (int)std::floor((x - 60) / CELL), cx1 = (int)std::floor((x + 60) / CELL), cz0 = (int)std::floor((z - 60) / CELL), cz1 = (int)std::floor((z + 60) / CELL);
  for (int cx = cx0; cx <= cx1; cx++)
    for (int cz = cz0; cz <= cz1; cz++)
      for (const Item& it : getCell(g_mainCache, L_ROCK, cx, cz)) {
        if (it.scale < 0.7f) continue;
        float d = hypot2(it.x - x, it.z - z);
        if (d < best) { best = d; ox = it.x; oz = it.z; os = it.scale; }
      }
  return best < 1e8f;
}

bool treeTrunkAt(float x, float y, float z) {
  const auto& c = getCell(g_mainCache, L_TREE, (int)std::floor(x / CELL), (int)std::floor(z / CELL));
  for (const Item& it : c) {
    float dx = x - it.x, dz = z - it.z;
    float r = it.scale * 0.55f;
    if (dx * dx + dz * dz < r * r && y > it.y - 0.5f && y < it.y + 14 * it.scale) return !blocked(it.x, it.z);
  }
  return false;
}

void shutdown() {
  if (g_impFbo) glDeleteFramebuffers(1, &g_impFbo);
  if (g_impTex) glDeleteTextures(1, &g_impTex);
  if (g_impNrm) glDeleteTextures(1, &g_impNrm);
  if (g_impDepth) glDeleteRenderbuffers(1, &g_impDepth);
  g_impQuad.destroy();
  g_canopy.destroy();
  for (auto& b : g_impAll) b.destroy();
  for (auto& s : g_slots) { s->impBatch.destroy(); for (auto& p : s->parts) p.lodBatch.destroy(); }
  if (g_worker.joinable()) {
    { std::lock_guard<std::mutex> lk(g_jobMtx); g_quit = true; }
    g_jobCv.notify_one();
    g_worker.join();
  }
  for (auto& s : g_slots)
    for (auto& p : s->parts) { p.batch.destroy(); p.shadowBatch.destroy(); }
  g_slots.clear();
  for (auto& g : g_garden)
    { for (auto& b : g.batches) b.destroy(); for (auto& b : g.farBatches) b.destroy(); }
  g_garden.clear();
  for (auto& m : g_procMesh) m.destroy();
  g_workCache.clear();
  g_mainCache.clear();
}

}  // namespace forest
