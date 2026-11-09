#include "relics.h"
#include "engine.h"
#include "fx.h"
#include "../world/terrain.h"
#include "../render/render.h"
#include "../models/shape.h"
#include "../audio/audio.h"
#include "extras.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>

namespace {
struct Relic { float x, y, z; std::string where; bool found = false; };
std::vector<Relic> g_list;
bool g_placed = false;
Mesh g_mesh, g_base;

bool walkable(float x0, float z0, float x1, float z1) {
  // the straight walk from a reachable spot to the relic never exceeds the player's slope limit
  float d = hypot2(x1 - x0, z1 - z0);
  int n = std::max(2, (int)(d / 1.5f));
  for (int i = 0; i <= n; i++) {
    float t = (float)i / n, x = x0 + (x1 - x0) * t, z = z0 + (z1 - z0) * t;
    if (world::localSlope(x, z) > 0.9f) return false;
    if (std::fabs(x) > MAP_HALF - 25 || std::fabs(z) > MAP_HALF - 25) return false;
  }
  return true;
}
void add(float x, float z, const std::string& where, float yOff = 0) {
  for (auto& r : g_list) if (hypot2(r.x - x, r.z - z) < 60) return;  // keep them spread out
  g_list.push_back({x, world::getTerrainHeight(x, z) + yOff, z, where});
}

void testOverride();
void place() {
  g_placed = true;
  g_list.clear();
  // 1. top of the waterfall: on the bank beside the river head
  {
    auto& h = world::RIVER.head;
    bool ok = false;
    for (float rr : {5.0f, 8.0f, 11.0f, 15.0f, 20.0f}) {
      for (int k = 0; k < 24 && !ok; k++) {
        float a = k * TAU / 24, x = h[0] + std::cos(a) * rr, z = h[1] + std::sin(a) * rr;
        if (world::riverMask(x, z) < 0.02f && world::localSlope(x, z) < 0.9f && world::getTerrainHeight(x, z) > world::getTerrainHeight(h[0], h[1]) - 4) {
          add(x, z, "at the top of the waterfall");
          ok = true;
        }
      }
      if (ok) break;
    }
  }
  // 2. on the bed of each lake (wade in to reach it)
  for (auto& L : world::LAKES) add(L.x + L.r * 0.15f, L.z - L.r * 0.1f, "on the bed of " + L.name);
  // 3. behind each trail mountain: off the trail near the top, on ground you can still walk to
  for (auto& R : world::ROUTES) {
    if (R.pts.size() < 4) continue;
    bool done = false;
    for (float f : {0.85f, 0.75f, 0.65f, 0.92f, 0.55f, 0.45f}) {
      size_t i = (size_t)(f * (R.pts.size() - 1));
      float px = R.pts[i][0], pz = R.pts[i][1];
      float tx = R.pts[std::min(i + 1, R.pts.size() - 1)][0] - R.pts[i > 0 ? i - 1 : 0][0], tz = R.pts[std::min(i + 1, R.pts.size() - 1)][1] - R.pts[i > 0 ? i - 1 : 0][1];
      float tl = std::max(1e-3f, hypot2(tx, tz));
      for (float side : {1.0f, -1.0f})
        for (float off : {22.0f, 16.0f, 28.0f, 12.0f, 34.0f, 9.0f}) {
          float x = px - tz / tl * off * side, z = pz + tx / tl * off * side;
          if (hypot2(x, z) < hypot2(px, pz) - 6) continue;  // the far side, away from the village
          if (world::pathMask(x, z) > 0.1f || world::riverMask(x, z) > 0.02f) continue;
          if (!walkable(px, pz, x, z)) continue;
          add(x, z, "behind a trail mountain");
          done = true;
          break;
        }
      if (done) break;
    }
  }
  // 4. quiet off-road corners of the valley: far from roads, trails and the village
  uint32_t seed = 913;
  auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0f; };
  int want = 12, tries = 0;
  while ((int)g_list.size() < want && tries++ < 4000) {
    float a = rnd() * TAU, r = 150 + rnd() * (MAP_HALF - 190);
    float x = std::cos(a) * r, z = std::sin(a) * r;
    if (world::pathDistance(x, z) < 40 || world::nearRoute(x, z) < 30) continue;
    if (world::riverMask(x, z) > 0.02f || world::lakeAt(x, z, 4) >= 0) continue;
    if (world::localSlope(x, z) > 0.6f) continue;
    float n = 1e9f, nx = 0, nz = 0;  // must be walkable from the nearest road
    for (auto& R : world::ROADS) for (auto& p : R.pts) { float d = hypot2(p[0] - x, p[1] - z); if (d < n) { n = d; nx = p[0]; nz = p[1]; } }
    if (n > 140 || !walkable(nx, nz, x, z)) continue;
    add(x, z, world::forestDensity(x, z) > 0.4f ? "deep in the forest" : "in a quiet corner of the valley");
  }
  testOverride();
}

void testOverride() {
  if (const char* e = std::getenv("HW_RELICAT")) {  // test hook: move relic 0 to x,z
    float x = 0, z = 0;
    if (std::sscanf(e, "%f,%f", &x, &z) == 2 && !g_list.empty()) { g_list[0].x = x; g_list[0].z = z; g_list[0].y = world::getTerrainHeight(x, z); }
  }
}
void buildMesh() {
  Shape s;
  // golden prayer wheel: drum with a red band, gilded rims, a little roof finial, on a carved stone plinth
  s.tube("#c98a12", {0, 0.55f, 0}, 0.22f, 0.22f, 0.5f, 14);
  s.tube("#9f1239", {0, 0.7f, 0}, 0.225f, 0.225f, 0.12f, 14);
  s.tube("#fbbf24", {0, 0.54f, 0}, 0.25f, 0.25f, 0.05f, 14);
  s.tube("#fbbf24", {0, 1.03f, 0}, 0.25f, 0.25f, 0.05f, 14);
  s.cone("#f59e0b", {0, 1.08f, 0}, 0.2f, 0.22f, 12);
  s.ball("#fde68a", {0, 1.33f, 0}, {0.06f, 0.08f, 0.06f});
  s.rod("#78350f", {0, 0.4f, 0}, {0, 1.32f, 0}, 0.025f, 0.025f);
  g_mesh = uploadMesh(s.build(true));
  Shape b;
  b.tube("#8a847d", {0, 0.0f, 0}, 0.42f, 0.5f, 0.42f, 8);
  b.tube("#78716c", {0, 0.42f, 0}, 0.3f, 0.36f, 0.12f, 8);
  for (int i = 0; i < 6; i++) b.box("#ef4444", {std::cos(i * 1.047f) * 0.46f, 0.22f, std::sin(i * 1.047f) * 0.46f}, {0.06f, 0.06f, 0.06f});  // painted mani marks
  g_base = uploadMesh(b.build(true));
}
// ---- secret: the Migoi's footprints. Deep in the narrow river gorge north of the valley (~ -62, 593) a line of
// giant bare footprints climbs out of the water to a carved stone head of the yeti with ice-blue eyes. No marker,
// no compass entry, no map icon: it only shows within ~40 m. Touch the stone once for the reward (saved as a stat).
struct Egg { bool placed = false; float x = 0, y = 0, z = 0, yaw = 0; std::vector<Vec3> prints; bool howled = false; };
Egg g_egg;
Mesh g_eggHead, g_eggPrint;
void placeEgg() {
  g_egg.placed = true;
  const float ex = -62.4f, ez = 592.9f;
  float bx = ex, bz = ez, best = 1e9f;
  for (float rr = 2; rr <= 16; rr += 1)
    for (int k = 0; k < 32; k++) {
      float a = k * TAU / 32, x = ex + std::cos(a) * rr, z = ez + std::sin(a) * rr;
      if (world::waterHeightAt(x, z) > world::getTerrainHeight(x, z) - 0.05f) continue;  // under water
      if (rr < best) { best = rr; bx = x; bz = z; }
    }
  g_egg.x = bx; g_egg.z = bz; g_egg.y = world::getTerrainHeight(bx, bz);
  g_egg.yaw = std::atan2(ex - bx, ez - bz);  // the head looks back down the gorge, at the river
  // footprints: from the water's edge up to the stone, alternating left / right, ~1.1 m strides
  float dx = bx - ex, dz = bz - ez, L = std::max(0.1f, hypot2(dx, dz));
  dx /= L; dz /= L;
  for (int i = 0; i < 7; i++) {
    float t = L - 1.4f - i * 1.1f;
    float side = (i & 1) ? 0.28f : -0.28f;
    float x = ex + dx * t - dz * side, z = ez + dz * t + dx * side;
    float g = world::getTerrainHeight(x, z), w = world::waterHeightAt(x, z);
    if (w > g - 0.05f) continue;  // no prints under the water
    g_egg.prints.push_back({x, g + 0.015f, z});
  }
  if (std::getenv("HW_EGG")) std::printf("egg at %.1f %.1f %.1f, %zu prints\n", g_egg.x, g_egg.y, g_egg.z, g_egg.prints.size());
}
void buildEgg() {
  Shape h;  // weathered stone yeti head on a cairn: shaggy brow, deep sockets, a glint of blue
  h.ball("#6b6560", {0, 0.18f, 0}, {0.55f, 0.22f, 0.5f}, {0, 0, 0}, 1);
  h.ball("#78716c", {0, 0.45f, 0}, {0.42f, 0.2f, 0.4f}, {0, 0.6f, 0}, 1);
  h.ball("#d6d3d1", {0, 1.0f, 0}, {0.42f, 0.48f, 0.4f}, {0, 0, 0}, 2);           // head, frost-white stone
  h.ball("#e7e5e4", {0, 1.32f, 0.02f}, {0.36f, 0.22f, 0.34f}, {0, 0, 0}, 2);      // crown of matted fur
  h.ball("#a8a29e", {0, 1.12f, 0.27f}, {0.34f, 0.08f, 0.14f}, {0, 0, 0}, 1);      // heavy brow
  h.ball("#57534e", {0.13f, 1.02f, 0.33f}, {0.09f, 0.07f, 0.05f}, {0, 0, 0}, 1);   // sockets
  h.ball("#57534e", {-0.13f, 1.02f, 0.33f}, {0.09f, 0.07f, 0.05f}, {0, 0, 0}, 1);
  h.ball("#a8a29e", {0, 0.86f, 0.36f}, {0.16f, 0.1f, 0.09f}, {0, 0, 0}, 1);       // muzzle
  h.box("#292524", {0, 0.78f, 0.38f}, {0.16f, 0.025f, 0.03f});                     // mouth line
  for (int i = 0; i < 9; i++) {  // offerings: little stones and a red thread around the cairn
    float a = i * TAU / 9;
    h.ball(i % 3 == 0 ? "#b91c1c" : "#8a847d", {std::cos(a) * 0.62f, 0.05f, std::sin(a) * 0.58f}, {0.07f, 0.05f, 0.07f}, {0, a, 0}, 0);
  }
  g_eggHead = uploadMesh(h.build(true));
  Shape f;  // one giant bare footprint (~45 cm): heel, sole, five toes, pressed dark into the ground
  f.ball("#3f3a34", {0, 0, -0.13f}, {0.1f, 0.01f, 0.11f}, {0, 0, 0}, 1);
  f.ball("#3f3a34", {0.01f, 0, 0.04f}, {0.12f, 0.01f, 0.13f}, {0, 0, 0}, 1);
  const float TX[5] = {-0.1f, -0.04f, 0.02f, 0.07f, 0.11f}, TZ[5] = {0.2f, 0.225f, 0.225f, 0.21f, 0.185f}, TR[5] = {0.045f, 0.035f, 0.032f, 0.03f, 0.027f};
  for (int i = 0; i < 5; i++) f.ball("#3f3a34", {TX[i], 0, TZ[i]}, {TR[i], 0.01f, TR[i] * 1.2f}, {0, 0, 0}, 0);
  g_eggPrint = uploadMesh(f.build(true));
}
void updateEgg() {
  if (!g_egg.placed) placeEgg();
  if (extras::getStat("yeti_found") > 0) return;
  const Vec3& p = E.player.pos;
  float d = hypot2(p.x - g_egg.x, p.z - g_egg.z);
  // a far-off howl down the gorge the first time you come near after dark
  if (!g_egg.howled && d < 45 && E.daylight < 0.4f) { g_egg.howled = true; ambience::howl(60); }
  if (d > 2.2f || std::fabs(p.y - 1.6f - g_egg.y) > 3.0f) return;
  extras::stat("yeti_found");
  E.player.maxHp += 15;
  E.player.hp = E.player.maxHp;
  actions::addScore(1000);
  audio::victoryFanfare();
  popup("Secret found!", rgbHex("#7dd3fc"));
  actions::notify("Footprints of the Migoi. Deep in the gorge, a stone face of the snow-man watches the river. The old ones say it guards travellers who find it. +15 max health, +1000 renown.");
}
void drawEgg() {
  if (!g_egg.placed) return;
  const Vec3& p = E.player.pos;
  float d = hypot2(p.x - g_egg.x, p.z - g_egg.z);
  if (d > 40) return;  // a secret: nothing to see from afar
  if (!g_eggHead.valid()) buildEgg();
  Shader& s = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::None, 0);
  render::setEmissive(s, Color(0, 0, 0), 0);
  for (size_t i = 0; i < g_egg.prints.size(); i++) {
    const Vec3& q = g_egg.prints[i];
    render::setModel(s, Mat4::compose(q, Quat::axisAngle({0, 1, 0}, g_egg.yaw + PI + ((i & 1) ? 0.12f : -0.12f)), {1, 1, 1}));
    g_eggPrint.draw();
  }
  if (!render::sphereVisible({g_egg.x, g_egg.y + 0.8f, g_egg.z}, 1.5f)) return;
  render::setModel(s, Mat4::compose({g_egg.x, g_egg.y, g_egg.z}, Quat::axisAngle({0, 1, 0}, g_egg.yaw), {1.2f, 1.2f, 1.2f}));
  g_eggHead.draw();
  // ice-blue eyes: faint by day, glowing at night; they dim for good once you have found it
  bool found = extras::getStat("yeti_found") > 0;
  float glow = (found ? 0.35f : 1.0f) * (0.4f + 0.9f * (1 - E.daylight)) * (0.85f + 0.15f * std::sin(E.clock * 1.3f));
  float c = std::cos(g_egg.yaw), sn = std::sin(g_egg.yaw);
  for (int k = -1; k <= 1; k += 2) {
    float lx = 0.13f * k * 1.2f, lz = 0.36f * 1.2f;
    fx::glow(g_egg.x + lx * c + lz * sn, g_egg.y + 1.02f * 1.2f, g_egg.z - lx * sn + lz * c, 0.22f * glow);
  }
}
}  // namespace

namespace relics {
void reset() { if (!g_placed) place(); for (auto& r : g_list) r.found = false; }
void restore(const std::vector<int>& f) {
  if (!g_placed) place();
  for (auto& r : g_list) r.found = false;
  for (int i : f) if (i >= 0 && i < (int)g_list.size()) g_list[i].found = true;
}
std::vector<int> found() { std::vector<int> o; for (size_t i = 0; i < g_list.size(); i++) if (g_list[i].found) o.push_back((int)i); return o; }
int count() { int n = 0; for (auto& r : g_list) n += r.found; return n; }
int total() { if (!g_placed) place(); return (int)g_list.size(); }

void update(float) {
  if (!g_placed) place();
  updateEgg();
  const Vec3& p = E.player.pos;
  for (auto& r : g_list) {
    if (r.found) continue;
    if (hypot2(p.x - r.x, p.z - r.z) > 2.0f || std::fabs(p.y - 1.6f - r.y) > 3.5f) continue;
    r.found = true;
    int n = count(), N = (int)g_list.size();
    audio::powerupChime();
    actions::addScore(250);
    popup("Prayer wheel " + std::to_string(n) + "/" + std::to_string(N), rgbHex("#fbbf24"));
    if (n == N) {
      E.player.maxHp += 25;
      E.player.hp = E.player.maxHp;
      actions::addScore(1500);
      audio::victoryFanfare();
      actions::notify("You found every lost prayer wheel! The monks bless you: +25 max health, +1500 renown.");
    } else actions::notify(std::string("You found a lost prayer wheel ") + r.where + "! (" + std::to_string(n) + "/" + std::to_string(N) + ") +250 renown.");
  }
}

bool position(int i, float& x, float& y, float& z) {
  if (!g_placed) place();
  if (i < 0 || i >= (int)g_list.size()) return false;
  x = g_list[i].x; y = g_list[i].y; z = g_list[i].z;
  std::printf("relic %d/%zu at %.0f %.0f %.0f: %s\n", i, g_list.size(), x, y, z, g_list[i].where.c_str());
  return true;
}

void draw() {
  if (!g_placed) return;
  drawEgg();
  if (!g_mesh.valid()) {
    buildMesh();
    if (std::getenv("HW_RELICAT") && !g_list.empty()) std::printf("relicdbg n=%zu r0=%.1f %.1f %.1f found=%d mesh=%d verts\n", g_list.size(), g_list[0].x, g_list[0].y, g_list[0].z, (int)g_list[0].found, g_mesh.vertexCount);
  }
  const Vec3& p = E.player.pos;
  for (auto& r : g_list) {
    if (r.found) continue;
    float d = hypot2(p.x - r.x, p.z - r.z);
    if (d > 160 || !render::sphereVisible({r.x, r.y + 0.8f, r.z}, 1.5f)) continue;
    Shader& s = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::None, 0);
    render::setModel(s, Mat4::compose({r.x, r.y, r.z}, Quat::axisAngle({0, 1, 0}, 0.4f), {1.35f, 1.35f, 1.35f}));
    render::setEmissive(s, Color(0, 0, 0), 0);
    g_base.draw();
    float spin = E.clock * 0.9f, bob = std::sin(E.clock * 1.7f + r.x) * 0.04f;
    render::setModel(s, Mat4::compose({r.x, r.y + bob, r.z}, Quat::axisAngle({0, 1, 0}, spin), {1.35f, 1.35f, 1.35f}));
    render::setEmissive(s, Color(0.9f, 0.6f, 0.15f), 0.25f + 0.1f * std::sin(E.clock * 3));  // warm gold shine
    g_mesh.draw();
    render::setEmissive(s, Color(0, 0, 0), 0);
    // soft golden glow, brighter at dusk and night, so a sharp eye can spot it from afar
    fx::glow(r.x, r.y + 1.15f + bob, r.z, 1.3f + 0.8f * (1 - E.daylight) + 0.2f * std::sin(E.clock * 2.2f));
  }
}
}  // namespace relics
