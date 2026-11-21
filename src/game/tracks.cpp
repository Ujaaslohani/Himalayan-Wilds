// v1.1.0: footprints. The player and the animals leave prints in snow, in mud along the rivers and on wet ground
#include <algorithm>
// after rain. Prints are pressed shapes laid on the terrain; they fade (shrink) over two minutes.
#include "tracks.h"
#include "engine.h"
#include "season.h"
#include "wildlife.h"
#include "../world/terrain.h"
#include "../render/render.h"
#include "../render/sky.h"
#include "../models/shape.h"
#include "fx.h"
#include <cmath>
#include <vector>

namespace {
struct Print { float x, y, z, yaw, t; uint8_t kind, snow; };   // kind 0 boot, 1 hoof/paw
std::vector<Print> g_prints;
size_t g_head = 0;
const size_t MAXP = 360;
const float LIFE = 120.0f;
Mesh g_mesh[2][2];   // [kind][snow]
float g_lastX = 1e9, g_lastZ = 1e9, g_clock = 0;
bool g_left = false;

int softGround(float x, float z) {   // 0 none, 1 mud/wet, 2 snow
  if (season.snow > 0.35f) return 2;
  RiverHit r = world::riverQuery(x, z);
  if (r.d < r.hw + 4.0f && r.d > r.hw - 1.0f) return 1;   // the muddy bank strip
  if (sky::rainAmount() > 0.15f) return 1;
  return 0;
}
void add(float x, float z, float yaw, int kind, int snow) {
  Print p{x, world::getTerrainHeight(x, z) + 0.012f, z, yaw, g_clock, (uint8_t)kind, (uint8_t)(snow == 2)};
  if (g_prints.size() < MAXP) g_prints.push_back(p);
  else { g_prints[g_head] = p; g_head = (g_head + 1) % MAXP; }
}
void build() {
  for (int s = 0; s < 2; s++) {
    const char* c = s ? "#93a4b8" : "#2b241b";   // snow: blue shadow in the hollow; mud: dark wet earth
    Shape b;   // boot: heel and sole
    b.ball(c, {0, 0, -0.09f}, {0.045f, 0.006f, 0.055f}, {0, 0, 0}, 1);
    b.ball(c, {0, 0, 0.06f}, {0.05f, 0.006f, 0.08f}, {0, 0, 0}, 1);
    g_mesh[0][s] = uploadMesh(b.build(true));
    Shape h;   // cloven hoof / pad: two lobes
    h.ball(c, {-0.022f, 0, 0}, {0.02f, 0.006f, 0.04f}, {0, 0.15f, 0}, 1);
    h.ball(c, {0.022f, 0, 0}, {0.02f, 0.006f, 0.04f}, {0, -0.15f, 0}, 1);
    g_mesh[1][s] = uploadMesh(h.build(true));
  }
}
}  // namespace

namespace tracks {
void update(float dt) {
  g_clock += dt;
  const Vec3& p = E.player.pos;
  float dx = p.x - g_lastX, dz = p.z - g_lastZ;
  if (dx * dx + dz * dz > 0.72f * 0.72f) {
    float yaw = std::atan2(dx, dz);
    g_lastX = p.x; g_lastZ = p.z;
    int s = softGround(p.x, p.z);
    if (s && std::fabs(p.y - 1.7f - world::getTerrainHeight(p.x, p.z)) < 0.5f) {
      g_left = !g_left;
      float side = g_left ? -0.13f : 0.13f;
      add(p.x + std::cos(yaw) * side, p.z - std::sin(yaw) * side, yaw, 0, s);
    }
  }
  // v1.1.3: breath vapour on cold days: the player (every ~3 s, faster when running) and animals close by
  {
    float cold = std::max({season.snow * 1.4f, season.autumn * (1 - E.daylight) * 1.2f, season.spring * (1 - E.daylight) * 0.6f, sstep(p.y, 70.0f, 120.0f)});
    static float bt = 0, at = 0;
    bt -= dt; at -= dt;
    if (cold > 0.35f && bt <= 0) {
      bt = 3.2f;
      Vec3 f(-std::sin(E.player.yaw), 0, -std::cos(E.player.yaw));
      fx::breath(p + f * 0.32f + Vec3(0, -0.12f, 0), f);
    }
    if (cold > 0.35f && at <= 0) {
      at = 0.9f;
      float ap[3 * 8];
      int an = wildlife::nearPositions(p.x, p.z, 25.0f, ap, 8);
      for (int i = 0; i < an; i++) {
        if (((int)(E.clock * 0.7f) + i) % 3) continue;   // each one breathes every few seconds, out of step
        float y = world::getTerrainHeight(ap[i * 3], ap[i * 3 + 1]) + 1.05f;
        fx::breath(Vec3(ap[i * 3], y, ap[i * 3 + 1]), Vec3(0, 0, 0));
      }
    }
  }
  {  // v1.1.10: after a cold night, mist lifts off the river in the early morning
    static float mt = 0;
    mt -= dt;
    float morning = sstep(E.time, 5.0f, 6.0f) * (1 - sstep(E.time, 8.5f, 10.0f));
    float coldN = std::max(season.autumn, std::max(season.spring * 0.7f, season.snow * 0.8f)) + 0.3f;
    if (mt <= 0 && morning * coldN > 0.3f) {
      mt = 0.5f;
      for (int k = 0; k < 4; k++) {
        float a = (float)E.clock * 7.13f + k * 1.9f, r = 6 + std::fmod((float)E.clock * 13.7f + k * 5.3f, 34.0f);
        float x = p.x + std::cos(a) * r, z = p.z + std::sin(a) * r;
        RiverHit rh = world::riverQuery(x, z);
        if (rh.d < rh.hw * 0.9f) { fx::mist(Vec3(x, rh.wy + 0.2f, z)); break; }
      }
    }
  }
  // animals near the player: a print pair whenever one has moved on from its last print
  static float acc = 0;
  acc += dt;
  if (acc < 0.25f) return;
  acc = 0;
  float pos[3 * 24];
  int n = wildlife::nearPositions(p.x, p.z, 45.0f, pos, 24);
  for (int i = 0; i < n; i++) {
    float ax = pos[i * 3], az = pos[i * 3 + 1];
    int s = softGround(ax, az);
    if (!s) continue;
    // nearest recent animal print: skip if close, and take the heading from it
    float best = 1e9, bx = ax, bz = az;
    for (auto& q : g_prints)
      if (q.kind == 1 && g_clock - q.t < 20) { float d = (q.x - ax) * (q.x - ax) + (q.z - az) * (q.z - az); if (d < best) { best = d; bx = q.x; bz = q.z; } }
    if (best < 0.8f * 0.8f) continue;
    float yaw = best < 9.0f ? std::atan2(ax - bx, az - bz) : 0.0f;
    add(ax - 0.12f * std::cos(yaw), az + 0.12f * std::sin(yaw), yaw, 1, s);
    add(ax + 0.12f * std::cos(yaw) + 0.3f * std::sin(yaw), az - 0.12f * std::sin(yaw) + 0.3f * std::cos(yaw), yaw, 1, s);
  }
}
void draw() {
  if (g_prints.empty()) return;
  if (!g_mesh[0][0].valid()) build();
  Shader& s = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::None, 0);
  render::setEmissive(s, Color(0, 0, 0), 0);
  const Vec3& p = E.player.pos;
  for (auto& q : g_prints) {
    float age = g_clock - q.t;
    if (age > LIFE || age < 0) continue;
    float d2 = (q.x - p.x) * (q.x - p.x) + (q.z - p.z) * (q.z - p.z);
    if (d2 > 40.0f * 40.0f) continue;
    float k = 1.0f - std::max(0.0f, (age - LIFE * 0.6f) / (LIFE * 0.4f));   // last 40%: the print fills in
    render::setModel(s, Mat4::compose({q.x, q.y, q.z}, Quat::axisAngle({0, 1, 0}, q.yaw), {k, 1, k}));
    g_mesh[q.kind][q.snow].draw();
  }
}
void clear() { g_prints.clear(); g_head = 0; }
}  // namespace tracks
