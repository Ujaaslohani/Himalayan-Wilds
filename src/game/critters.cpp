// v1.1.8: small creatures that make the land feel lived in (not hunted, just there):
//  - hares sitting in meadows that bolt away in zig-zag bounds when you come close
//  - pheasants pecking at forest edges that burst up and whirr away low
//  - squirrels spiralling up tree trunks, and woodpeckers clinging to trunks
//  - small birds perched on the roof ridges and chimneys of the homes, flying off as you approach
// They live only around the player: spawned 18-45 m out, gone again past 70 m. Cheap shapes, one draw each.
#include "critters.h"
#include "engine.h"
#include "season.h"
#include "secrets.h"
#include "../world/terrain.h"
#include "../world/forest.h"
#include "../render/render.h"
#include "../models/shape.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace {
enum Kind { HARE, PHEASANT, SQUIRREL, WOODPECKER, ROOFBIRD, FISH, KINDS };
const bool LAND_LIFE = false;   // v1.1.9: hares, pheasants, squirrels and perched birds switched off for now (fish stay)
struct Critter {
  Kind k;
  Vec3 home;      // where it sits (trunk base for climbers, perch for roof birds)
  float yaw = 0, t = 0, seed = 0;
  float fleeT = -1;   // seconds since it was startled (-1 = calm)
  Vec3 fleeDir;
  float trunkR = 0.3f;
};
std::vector<Critter> g_c;
Mesh g_mesh[KINDS][3];
float g_spawnT = 0;
uint32_t g_rs = 12345;
float rnd() { g_rs ^= g_rs << 13; g_rs ^= g_rs >> 17; g_rs ^= g_rs << 5; return (g_rs & 0xffffff) / 16777216.0f; }

void build() {
  for (int f = 0; f < 3; f++) {
    {  // hare: crouched (0), stretched mid-bound (1), gathered (2)
      Shape h;
      const char* fur = "#8a7458"; const char* belly = "#c9b89a";
      float st = f == 1 ? 1.35f : f == 2 ? 0.85f : 1.0f;
      h.ball(fur, {0, 0.14f, 0}, {0.1f, 0.11f, 0.17f * st});
      h.ball(belly, {0, 0.1f, 0.03f}, {0.08f, 0.07f, 0.12f * st});
      h.ball(fur, {0, 0.24f, 0.15f * st}, {0.06f, 0.065f, 0.08f});
      h.ball("#2a2018", {0.035f, 0.26f, 0.19f * st}, {0.012f, 0.012f, 0.012f});
      h.ball("#2a2018", {-0.035f, 0.26f, 0.19f * st}, {0.012f, 0.012f, 0.012f});
      for (int sd = -1; sd <= 1; sd += 2) h.ball(fur, {sd * 0.025f, 0.36f, 0.11f * st}, {0.018f, 0.1f, 0.028f}, {f == 1 ? -0.6f : -0.15f, 0, sd * 0.15f});   // long ears
      h.ball("#f2efe8", {0, 0.17f, -0.17f * st}, {0.035f, 0.035f, 0.03f});   // white scut
      for (int sd = -1; sd <= 1; sd += 2) h.ball(fur, {sd * 0.07f, 0.07f, -0.08f * st + (f == 1 ? -0.1f : 0)}, {0.035f, 0.06f, 0.11f});   // hind legs
      g_mesh[HARE][f] = uploadMesh(h.build(true));
    }
    {  // pheasant: on the ground (0), wings up (1), wings down (2)
      Shape p;
      p.ball("#7a3f1c", {0, 0.2f, 0}, {0.09f, 0.1f, 0.16f});
      p.ball("#b05a24", {0, 0.21f, 0.05f}, {0.08f, 0.09f, 0.1f});
      p.ball("#1f4a3a", {0, 0.33f, 0.14f}, {0.04f, 0.06f, 0.045f});           // green neck
      p.ball("#c42a1c", {0, 0.37f, 0.17f}, {0.03f, 0.03f, 0.03f});            // red face wattle
      p.ball("#f0ece4", {0, 0.31f, 0.135f}, {0.045f, 0.012f, 0.045f});         // white collar
      p.cone("#c8b080", {0, 0.36f, 0.2f}, 0.01f, 0.035f, 4, {PI * 0.5f, 0, 0});
      p.box("#8a5a2a", {0, 0.24f, -0.33f}, {0.04f, 0.01f, 0.36f}, {-0.2f, 0, 0});   // long barred tail
      if (f == 0) for (int sd = -1; sd <= 1; sd += 2) p.rod("#6a6050", {sd * 0.03f, 0.11f, 0}, {sd * 0.03f, 0, 0.02f}, 0.007f, 0.006f, 3);
      else for (int sd = -1; sd <= 1; sd += 2) p.ball("#6e4a2a", {sd * 0.17f, 0.24f + (f == 1 ? 0.08f : -0.04f), 0}, {0.15f, 0.015f, 0.09f}, {0, 0, sd * (f == 1 ? 0.5f : -0.3f)});
      g_mesh[PHEASANT][f] = uploadMesh(p.build(true));
    }
    {  // squirrel (on a trunk the model is turned to cling: head up), bushy tail curled
      Shape q;
      const char* red = "#8c4a22";
      q.ball(red, {0, 0, 0}, {0.045f, 0.05f, 0.09f});
      q.ball(red, {0, 0.02f, 0.08f}, {0.035f, 0.035f, 0.04f});
      for (int sd = -1; sd <= 1; sd += 2) q.cone(red, {sd * 0.018f, 0.05f, 0.08f}, 0.01f, 0.03f, 4);
      q.ball("#c9a37a", {0, -0.02f, 0.01f}, {0.03f, 0.03f, 0.06f});
      q.ball(red, {0, 0.06f, -0.12f}, {0.04f, 0.09f + 0.01f * f, 0.05f}, {0.5f, 0, 0});   // tail
      g_mesh[SQUIRREL][f] = uploadMesh(q.build(true));
    }
    {  // woodpecker: black and white, red crown; head drawn back (0) / forward (1, 2) for the drumming
      Shape w;
      w.ball("#18181a", {0, 0, 0}, {0.04f, 0.05f, 0.09f});
      w.ball("#e8e4dc", {0.03f, 0.0f, 0.02f}, {0.015f, 0.035f, 0.05f});
      w.ball("#e8e4dc", {-0.03f, 0.0f, 0.02f}, {0.015f, 0.035f, 0.05f});
      float hz = f == 0 ? 0.075f : 0.095f;
      w.ball("#18181a", {0, 0.01f, hz}, {0.032f, 0.032f, 0.035f});
      w.ball("#c41e1e", {0, 0.035f, hz - 0.01f}, {0.02f, 0.015f, 0.022f});
      w.cone("#3a3630", {0, 0.01f, hz + 0.03f}, 0.008f, 0.04f, 4, {PI * 0.5f, 0, 0});
      w.box("#18181a", {0, -0.01f, -0.1f}, {0.03f, 0.008f, 0.07f}, {0.4f, 0, 0});
      g_mesh[WOODPECKER][f] = uploadMesh(w.build(true));
    }
    {  // small brown bird (sparrow/finch) sitting (0) / flying (1, 2)
      Shape b;
      b.ball("#7a6248", {0, 0.04f, 0}, {0.03f, 0.03f, 0.05f});
      b.ball("#d8c8a8", {0, 0.03f, 0.015f}, {0.025f, 0.022f, 0.035f});
      b.ball("#5a4834", {0, 0.07f, 0.04f}, {0.022f, 0.022f, 0.024f});
      b.cone("#3a3028", {0, 0.068f, 0.065f}, 0.006f, 0.016f, 4, {PI * 0.5f, 0, 0});
      b.box("#5a4834", {0, 0.045f, -0.06f}, {0.02f, 0.005f, 0.04f}, {-0.2f, 0, 0});
      if (f > 0) for (int sd = -1; sd <= 1; sd += 2) b.ball("#5a4834", {sd * 0.05f, 0.05f + (f == 1 ? 0.03f : -0.015f), 0}, {0.05f, 0.006f, 0.025f}, {0, 0, sd * (f == 1 ? 0.6f : -0.35f)});
      g_mesh[ROOFBIRD][f] = uploadMesh(b.build(true));
    }
    {  // v1.1.10: trout: olive back, speckled flank, pale belly; tail bent left / straight / right for the swim
      Shape t;
      float bend = (f - 1) * 0.35f;
      t.ball("#4a5434", {0, 0.012f, 0.02f}, {0.035f, 0.032f, 0.13f});
      t.ball("#b8b49a", {0, -0.008f, 0.02f}, {0.03f, 0.02f, 0.11f});
      t.ball("#7a6a4a", {0, 0.005f, 0.0f}, {0.036f, 0.018f, 0.1f});   // speckled band
      t.ball("#4a5434", {std::sin(bend) * 0.06f, 0.008f, -0.11f}, {0.02f, 0.022f, 0.05f}, {0, bend, 0});
      t.box("#3e4630", {std::sin(bend) * 0.12f, 0.008f, -0.16f}, {0.006f, 0.05f, 0.04f}, {0, bend * 1.6f, 0});
      t.box("#3e4630", {0, 0.04f, 0.03f}, {0.004f, 0.025f, 0.04f});   // dorsal fin
      g_mesh[FISH][f] = uploadMesh(t.build(true));
    }
  }
}

void spawn() {
  const Vec3& P = E.player.pos;
  int have[KINDS] = {0};
  for (auto& c : g_c) have[c.k]++;
  bool winter = season.snow > 0.6f;
  // v1.1.10: trout in the river near you, holding in the current in the deeper middle
  for (int tries = 0; tries < 10 && have[FISH] < 5; tries++) {
    float a = rnd() * TAU, r = 6 + rnd() * 30;
    float x = P.x + std::cos(a) * r, z = P.z + std::sin(a) * r;
    RiverHit rh = world::riverQuery(x, z);
    if (rh.d > rh.hw * 0.7f || rh.depth < 0.45f || season.snow > 0.6f) continue;
    Critter c; c.k = FISH;
    c.home = {x, rh.wy - std::min(0.4f, rh.depth * 0.45f), z};
    c.yaw = std::atan2(-rh.tx, -rh.tz);   // heads upstream
    c.seed = rnd() * 100;
    have[FISH]++;
    g_c.push_back(c);
  }
  if (!LAND_LIFE) return;
  // hares and pheasants on open ground / forest edges around you
  for (int tries = 0; tries < 12; tries++) {
    float a = rnd() * TAU, r = 18 + rnd() * 27;
    float x = P.x + std::cos(a) * r, z = P.z + std::sin(a) * r;
    if (world::riverMask(x, z) > 0.1f || world::lakeAt(x, z, 1) >= 0 || forest::obstacleAt(x, z, 0.5f)) continue;
    float fd = world::forestDensity(x, z);
    Critter c;
    if (have[HARE] < 3 && fd < 0.4f) c.k = HARE;
    else if (have[PHEASANT] < 2 && fd > 0.15f && fd < 0.7f && !winter) c.k = PHEASANT;
    else continue;
    c.home = {x, world::getTerrainHeight(x, z), z};
    c.yaw = rnd() * TAU; c.seed = rnd() * 100;
    have[c.k]++;
    g_c.push_back(c);
  }
  // climbers on trunks
  if (have[SQUIRREL] < 2 || have[WOODPECKER] < 1) {
    std::vector<float> tr;
    forest::nearTrees(P.x, P.z, 45, tr, 24);
    for (size_t i = 0; i + 3 < tr.size(); i += 4) {
      float d = hypot2(tr[i] - P.x, tr[i + 2] - P.z);
      if (d < 15 || rnd() > 0.25f) continue;
      Critter c;
      if (have[SQUIRREL] < 2 && !winter) c.k = SQUIRREL; else if (have[WOODPECKER] < 1) c.k = WOODPECKER; else break;
      c.home = {tr[i], tr[i + 1], tr[i + 2]};
      c.trunkR = 0.22f * tr[i + 3] + 0.1f;
      c.yaw = rnd() * TAU; c.seed = rnd() * 100;
      have[c.k]++;
      g_c.push_back(c);
    }
  }
  // small birds on the roof tops of homes within 60 m
  if (have[ROOFBIRD] < 6) {
    std::vector<Vec3> roofs;
    secrets::roofPerches(roofs);
    for (auto& rp : roofs) {
      if (have[ROOFBIRD] >= 6) break;
      float d = hypot2(rp.x - P.x, rp.z - P.z);
      if (d < 18 || d > 60 || rnd() > 0.4f) continue;
      int n = 1 + (int)(rnd() * 3);
      for (int k = 0; k < n && have[ROOFBIRD] < 6; k++) {
        Critter c; c.k = ROOFBIRD;
        c.home = {rp.x + (rnd() - 0.5f) * 1.6f, rp.y - 0.12f, rp.z + (rnd() - 0.5f) * 1.6f};
        c.yaw = rnd() * TAU; c.seed = rnd() * 100;
        have[ROOFBIRD]++;
        g_c.push_back(c);
      }
    }
  }
}
}  // namespace

namespace critters {
void update(float dt) {
  if (!E.playing) return;
  const Vec3& P = E.player.pos;
  g_spawnT -= dt;
  if (g_spawnT <= 0) { g_spawnT = 2.0f; spawn(); }
  for (auto& c : g_c) {
    c.t += dt;
    float d = hypot2(c.home.x - P.x, c.home.z - P.z);
    float flushR = c.k == FISH ? 5 : c.k == HARE ? 13 : c.k == PHEASANT ? 8 : c.k == ROOFBIRD ? 14 : c.k == WOODPECKER ? 10 : 9;
    if (c.fleeT < 0 && d < flushR) {
      c.fleeT = 0;
      Vec3 aw(c.home.x - P.x, 0, c.home.z - P.z);
      float l = std::max(0.1f, std::sqrt(aw.x * aw.x + aw.z * aw.z));
      c.fleeDir = Vec3(aw.x / l, 0, aw.z / l);
    }
    if (c.fleeT >= 0) c.fleeT += dt;
  }
  g_c.erase(std::remove_if(g_c.begin(), g_c.end(), [&](const Critter& c) {
    float d = hypot2(c.home.x - P.x, c.home.z - P.z);
    return d > 70 || c.fleeT > (c.k == FISH ? 1.5f : 8.0f);
  }), g_c.end());
}

void draw() {
  if (g_c.empty()) return;
  if (!g_mesh[0][0].valid()) build();
  Shader& sh = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::None, 0);
  render::setEmissive(sh, Color(0, 0, 0), 0);
  for (auto& c : g_c) {
    Vec3 p = c.home;
    float yaw = c.yaw, pitch = 0;
    int f = 0;
    float ft = c.fleeT;
    switch (c.k) {
      case HARE:
        if (ft < 0) { yaw += 0.2f * std::sin(c.t * 0.3f + c.seed); p.y += 0.0f; f = 0; }   // sitting, now and then turning
        else {   // zig-zag bounding away, ~9 m/s, three-beat hops
          float dist = 9.0f * ft;
          float zig = std::sin(ft * 2.6f + c.seed) * 2.0f;
          Vec3 side(-c.fleeDir.z, 0, c.fleeDir.x);
          p = c.home + c.fleeDir * dist + side * zig;
          p.y = world::getTerrainHeight(p.x, p.z) + std::fabs(std::sin(ft * 9.0f)) * 0.35f;
          yaw = std::atan2(c.fleeDir.x + side.x * std::cos(ft * 2.6f + c.seed) * 0.5f, c.fleeDir.z + side.z * std::cos(ft * 2.6f + c.seed) * 0.5f);
          f = std::sin(ft * 9.0f) > 0 ? 1 : 2;
        }
        break;
      case PHEASANT:
        if (ft < 0) {   // walking a slow circle, pecking
          float a = c.t * 0.25f + c.seed;
          p = c.home + Vec3(std::cos(a), 0, std::sin(a)) * 1.2f;
          p.y = world::getTerrainHeight(p.x, p.z);
          yaw = a + PI * 0.5f;
          pitch = std::sin(c.t * 4 + c.seed) > 0.7f ? 0.5f : 0;   // peck
        } else {   // whirring burst, low and fast, then a glide
          float dist = 4.0f * ft + 4.0f * ft * ft;
          p = c.home + c.fleeDir * dist;
          p.y = world::getTerrainHeight(c.home.x, c.home.z) + std::min(3.5f, ft * 4.0f);
          yaw = std::atan2(c.fleeDir.x, c.fleeDir.z);
          f = ft < 1.5f ? 1 + ((int)(c.t * 22) & 1) : 1;
        }
        break;
      case SQUIRREL:
      case WOODPECKER: {
        float h, a;
        if (c.k == SQUIRREL) {
          if (ft < 0) { h = 1.6f + 1.2f * (0.5f + 0.5f * std::sin(c.t * 0.35f + c.seed)); a = c.seed + std::sin(c.t * 0.2f) * 1.5f; }
          else { h = 2.8f + ft * 3.0f; a = c.seed + ft * 2.5f; }   // spirals up out of sight round the trunk
        } else { h = 2.5f + std::fmod(c.seed, 2.0f); a = c.seed + (ft >= 0 ? ft * 0.0f : 0); }
        p = c.home + Vec3(std::cos(a), 0, std::sin(a)) * (c.trunkR + 0.04f);
        p.y = c.home.y + h;
        yaw = std::atan2(-std::cos(a), -std::sin(a));   // facing the trunk
        pitch = -1.45f;   // body vertical, head up
        if (c.k == WOODPECKER) {
          if (ft >= 0) {   // off in a bounding flight
            p = p + Vec3(std::cos(a), 0, std::sin(a)) * (ft * 6) + c.fleeDir * (ft * 3);
            p.y += std::sin(ft * 8) * 0.2f;
            pitch = 0; yaw = std::atan2(std::cos(a), std::sin(a));
            f = 1;
          } else f = std::fmod(c.t * 1.0f + c.seed, 3.0f) < 0.6f ? 1 + ((int)(c.t * 30) & 1) : 0;   // drumming bursts
        }
        break;
      }
      case FISH:
        if (ft < 0) {   // holding against the current: small side drift and a slow tail beat
          p.x += std::sin(c.t * 0.6f + c.seed) * 0.15f; p.z += std::cos(c.t * 0.5f + c.seed) * 0.15f;
          yaw += std::sin(c.t * 1.3f + c.seed) * 0.12f;
          f = 1 + (int)std::round(std::sin(c.t * 5.0f + c.seed));
        } else {   // darts off downstream-and-away, diving deeper
          Vec3 dd = c.fleeDir;
          p = c.home + dd * (ft * 4.5f);
          p.y -= std::min(0.3f, ft * 0.4f);
          yaw = std::atan2(dd.x, dd.z);
          f = 1 + (int)std::round(std::sin(c.t * 22.0f));
          if (ft > 1.5f) continue;
        }
        break;
      case ROOFBIRD:
        if (ft < 0) { yaw += (std::fmod(c.t + c.seed, 3.0f) < 0.2f ? 1.0f : 0.0f) + 0.3f * std::floor(std::sin(c.t * 0.5f + c.seed) * 3); p.y += std::max(0.0f, std::sin(c.t * 7 + c.seed)) > 0.97f ? 0.04f : 0; }
        else {
          float dist = 6.0f * ft;
          p = c.home + c.fleeDir * dist + Vec3(std::sin(ft * 3 + c.seed), 0, std::cos(ft * 2.7f + c.seed)) * 1.5f;
          p.y += ft * 2.5f + std::sin(ft * 10) * 0.15f;
          yaw = std::atan2(c.fleeDir.x, c.fleeDir.z);
          f = 1 + ((int)(c.t * 18 + c.seed) & 1);
        }
        break;
      default: break;
    }
    if (!render::sphereVisible({p.x, p.y + 0.15f, p.z}, 0.4f)) continue;
    Quat q = Quat::axisAngle({0, 1, 0}, yaw) * Quat::axisAngle({1, 0, 0}, pitch);
    render::setModel(sh, Mat4::compose(p, q, {1, 1, 1}));
    g_mesh[c.k][f].draw();
  }
}
void clear() { g_c.clear(); }
}  // namespace critters
