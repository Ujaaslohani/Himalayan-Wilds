#include "secrets.h"
#include "engine.h"
#include "fx.h"
#include "season.h"
#include "wildlife.h"
#include "extras.h"
#include "../world/terrain.h"
#include "../render/render.h"
#include "../render/water.h"
#include "../models/shape.h"
#include "../models/houses.h"
#include "../audio/audio.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

namespace secrets {
namespace {

// ---------- helpers ----------
bool dry(float x, float z) { return world::waterHeightAt(x, z) < world::getTerrainHeight(x, z) + 0.05f; }
float gy(float x, float z) { return world::getTerrainHeight(x, z); }
bool near(float x, float y, float z, float r, float dy = 3.5f) {
  const Vec3& p = E.player.pos;
  return hypot2(p.x - x, p.z - z) < r && std::fabs(p.y - 1.6f - y) < dy;
}
float pdist(float x, float z) { const Vec3& p = E.player.pos; return hypot2(p.x - x, p.z - z); }
bool flag(const char* k) { return extras::getStat(k) > 0; }
void setFlag(const char* k) { if (!flag(k)) extras::stat(k, 1); }
Shader& obj() { return render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::None, 0); }
void put(Shader& s, float x, float y, float z, float yaw, float sc) {
  render::setModel(s, Mat4::compose({x, y, z}, Quat::axisAngle({0, 1, 0}, yaw), {sc, sc, sc}));
}
int hour() { return (int)std::floor(E.time) % 24; }

Mesh g_drop, g_print, g_bell, g_cave, g_caveDeco, g_bowM, g_mani, g_ruin, g_poppy, g_flags, g_relic;
CaveFrame g_cf;
Shape g_caveFloorShape;
Mesh g_caveFloor;
// plank bridges where the roads of the new lands cross the river (they used to wade through fords)
struct Bridge { float x, z, yaw, len, deck, bed; Mesh mesh; };
std::vector<Bridge> g_bridges;
GLuint g_punchVao = 0, g_punchVbo = 0;
int g_punchN = 0;

// ---------- placement (all deterministic from the world) ----------
bool g_placed = false;
struct Spot { float x = 0, y = 0, z = 0, yaw = 0; };
Spot g_bellS, g_starS, g_caveS, g_ruinS, g_poppyS;
const Shrine* g_top = nullptr;
std::vector<Spot> g_maniS;
const float GORGE_X = -62.4f, GORGE_Z = 654.5f;  // the Migoi's gorge

const char* MANI[9] = {
    "\"Om mani padme hum. Before the village, the valley was ice, and the ice was the Migoi's.\"",
    "\"A herder followed a lost yak up the river and found the ice had turned to grass.\"",
    "\"He built a fire where the river bends. Seven families followed him over the passes.\"",
    "\"The first winter the snow buried the doors. A white deer led the hunters to the warm springs.\"",
    "\"The lama rang a bell on the highest peak, and the avalanche turned aside.\"",
    "\"Those who settled the far meadows did not listen. Their houses stand empty still.\"",
    "\"The founder's bow was left behind the falling water, for the one who walks through.\"",
    "\"When the Hunter's stars stand over the peak in winter, the old gods leave a gift.\"",
    "\"The valley gives to those who take only what they need. Om mani padme hum.\"",
};

void place() {
  g_placed = true;
  // highest temple: bell and star alignment
  float best = -1e9f;
  for (auto& sh : world::SHRINES) if (sh.y > best) { best = sh.y; g_top = &sh; }
  if (g_top) {
    // inside the courtyard (local +-13 m, gate on +z): bell in the front-left corner, star point front-right
    float c = std::cos(g_top->yaw), sn = std::sin(g_top->yaw);
    auto at = [&](float lx, float lz, float yaw) { float x = g_top->x + lx * c + lz * sn, z = g_top->z - lx * sn + lz * c; return Spot{x, g_top->y + 0.15f, z, g_top->yaw + yaw}; };
    g_bellS = at(-8.5f, 8.0f, 0.6f);
    g_starS = at(8.5f, 8.0f, 0);
  }
  // waterfall cave: the grotto behind the curtain (its frame comes from the falls geometry)
  {
    CaveFrame cf = water::caveFrame();
    g_cf = cf;
    g_caveS = {cf.front.x, cf.floorY, cf.front.z, cf.yaw};
  }
  // bridges: the outer roads' river crossings
  g_bridges.clear();
  for (size_t ri = CORE_ROADS; ri < world::ROADS.size(); ri++) {
    const Road& rd = world::ROADS[ri];
    float bestD = 1e9f; size_t bi = 0;
    for (size_t i = 0; i < rd.pts.size(); i++) {
      RiverHit q = world::riverQuery(rd.pts[i][0], rd.pts[i][1]);
      float d = q.d - q.hw;
      if (d < bestD) { bestD = d; bi = i; }
    }
    if (bestD > 1.0f || bi == 0 || bi + 1 >= rd.pts.size()) continue;
    RiverHit q = world::riverQuery(rd.pts[bi][0], rd.pts[bi][1]);
    float tx = rd.pts[bi + 1][0] - rd.pts[bi - 1][0], tz = rd.pts[bi + 1][1] - rd.pts[bi - 1][1];
    float L = hypot2(tx, tz); tx /= L; tz /= L;
    // centre on the water, then reach out until the bank (ramp) climbs to the deck on both sides
    float cx = q.cx ? q.cx : rd.pts[bi][0], cz = q.cz ? q.cz : rd.pts[bi][1];
    cx = rd.pts[bi][0]; cz = rd.pts[bi][1];
    float deck = q.wy + 1.1f, half = q.hw + 2;
    for (float h = q.hw; h < q.hw + 26; h += 0.5f) {
      half = h;
      if (gy(cx + tx * h, cz + tz * h) > deck - 0.2f && gy(cx - tx * h, cz - tz * h) > deck - 0.2f) break;
    }
    bool dup = false;
    for (auto& b : g_bridges) if (hypot2(b.x - cx, b.z - cz) < 30) dup = true;
    if (!dup) g_bridges.push_back({cx, cz, std::atan2(tx, tz), half * 2 + 1.0f, deck, q.wy - q.depth, Mesh()});
  }
  if (std::getenv("HW_SECRETS")) for (auto& b : g_bridges) std::printf("secret bridge %.1f %.1f len %.1f deck %.1f\n", b.x, b.z, b.len, b.deck);
  // mani stones: beside the temple trails, two per trail
  g_maniS.clear();
  int nr = (int)world::ROUTES.size();
  for (int k = 0; k < 9 && nr > 0; k++) {
    const Route& r = world::ROUTES[k % nr];
    float s = r.len * (k < nr ? 0.3f : 0.65f) + (k / nr) * 9.0f;
    float x, z, tx, tz;
    world::routePoint(r, s, x, z, tx, tz);
    float L = hypot2(tx, tz); if (L < 1e-3f) { tx = 0; tz = 1; L = 1; }
    tx /= L; tz /= L;
    // the flatter verge, 2.6-3.4 m off the tread; set on the lowest corner so nothing floats
    float best = 1e9f, bx = x, bz = z;
    for (float side : {2.6f, -2.6f, 3.4f, -3.4f}) {
      float ox = x + tz * side, oz = z - tx * side;
      float sl = world::localSlope(ox, oz);
      if (sl < best && dry(ox, oz)) { best = sl; bx = ox; bz = oz; }
    }
    float lo = 1e9f;
    for (int c = 0; c < 4; c++) lo = std::min(lo, gy(bx + ((c & 1) ? 0.7f : -0.7f), bz + ((c & 2) ? 0.5f : -0.5f)));
    g_maniS.push_back({bx, lo - 0.08f, bz, std::atan2(tx, tz) + PI * 0.5f});
  }
  g_maniS.clear();   // mani stones removed (v0.9.45)
  // ruined village and the blue poppy meadow: out in the new lands
  float bestR = 1e9f, bestP = -1e9f;
  for (int i = 0; i < 72; i++) {
    float a = i * TAU / 72;
    P2 q = world::outerPoint(a, 170);
    float sl = world::localSlope(q[0], q[1]);
    if (dry(q[0], q[1]) && world::pathDistance(q[0], q[1]) > 25) {
      float score = sl + std::fabs(a - 2.4f) * 0.02f;
      if (score < bestR) { bestR = score; g_ruinS = {q[0], gy(q[0], q[1]), q[1], a}; }
    }
    P2 m = world::outerPoint(a, 110);
    float g = gy(m[0], m[1]);
    if (dry(m[0], m[1]) && world::localSlope(m[0], m[1]) < 0.3f && g > bestP && hypot2(m[0] - g_ruinS.x, m[1] - g_ruinS.z) > 200) { bestP = g; g_poppyS = {m[0], g, m[1], 0}; }
  }
  if (std::getenv("HW_SECRETS")) {
    std::printf("secret bell %.1f %.1f %.1f star %.1f %.1f %.1f\n", g_bellS.x, g_bellS.y, g_bellS.z, g_starS.x, g_starS.y, g_starS.z);
    std::printf("secret cave %.1f %.1f %.1f ruin %.1f %.1f %.1f poppy %.1f %.1f %.1f\n", g_caveS.x, g_caveS.y, g_caveS.z, g_ruinS.x, g_ruinS.y, g_ruinS.z, g_poppyS.x, g_poppyS.y, g_poppyS.z);
    std::printf("secret caveframe front %.1f %.1f %.1f yaw %.3f roofLen %.1f roofY %.1f center %.1f %.1f\n", g_cf.front.x, g_cf.front.y, g_cf.front.z, g_cf.yaw, g_cf.roofLen, g_cf.roofY, g_cf.center.x, g_cf.center.z);
    for (auto& m : g_maniS) std::printf("secret mani %.1f %.1f %.1f\n", m.x, m.y, m.z);
  }
}

void buildMeshes() {
  Shape d; d.ball("#4a0a0a", {0, 0, 0}, {0.09f, 0.008f, 0.07f}, {0, 0, 0}, 1);
  d.ball("#5c0d0d", {0.12f, 0, 0.05f}, {0.04f, 0.006f, 0.035f}, {0, 0, 0}, 0);
  d.ball("#3d0808", {-0.07f, 0, 0.14f}, {0.03f, 0.006f, 0.03f}, {0, 0, 0}, 0);
  g_drop = uploadMesh(d.build(true));
  Shape f;  // a giant print pressed into snow: blue-grey shadowed hollow
  f.ball("#8b9bb0", {0, 0, -0.13f}, {0.11f, 0.01f, 0.12f}, {0, 0, 0}, 1);
  f.ball("#8b9bb0", {0.01f, 0, 0.05f}, {0.13f, 0.01f, 0.14f}, {0, 0, 0}, 1);
  const float TX[5] = {-0.1f, -0.04f, 0.02f, 0.07f, 0.11f}, TZ[5] = {0.21f, 0.235f, 0.235f, 0.22f, 0.195f};
  for (int i = 0; i < 5; i++) f.ball("#8b9bb0", {TX[i], 0, TZ[i]}, {0.04f, 0.01f, 0.05f}, {0, 0, 0}, 0);
  g_print = uploadMesh(f.build(true));
  Shape b;  // bronze bell on a wooden frame
  b.box("#5b3a22", {-0.9f, 1.35f, 0}, {0.16f, 2.7f, 0.16f});
  b.box("#5b3a22", {0.9f, 1.35f, 0}, {0.16f, 2.7f, 0.16f});
  b.box("#4a2f1b", {0, 2.72f, 0}, {2.1f, 0.18f, 0.2f});
  b.box("#7a1d1d", {0, 2.86f, 0}, {2.3f, 0.08f, 0.32f});
  b.tube("#9a6b2e", {0, 2.12f, 0}, 0.2f, 0.36f, 0.62f, 12);
  b.tube("#b07d36", {0, 1.83f, 0}, 0.37f, 0.37f, 0.06f, 12);
  b.ball("#9a6b2e", {0, 2.48f, 0}, {0.2f, 0.12f, 0.2f}, {0, 0, 0}, 1);
  b.box("#3f2a18", {0, 2.6f, 0}, {0.05f, 0.2f, 0.05f});
  g_bell = uploadMesh(b.build(false));
  // ---- the cave: a tunnel into the cliff behind the falls (local: origin = mouth centre on the floor, -z inward)
  {
    const CaveFrame& cf = g_cf;
    const float D = cf.depth, MH = cf.roofY - cf.floorY;
    Rng rng(4242);
    auto R = [&](float a, float b) { return a + (b - a) * rng.next(); };
    // tunnel walls: irregular arch rings, narrowing and lowering as it goes in, closed at the back
    const int RINGS = 12, SEG = 14;
    Geo g;
    std::vector<float> jit((RINGS + 1) * (SEG + 1));
    for (auto& j : jit) j = R(0.82f, 1.12f);
    for (int r = 0; r <= RINGS; r++) {
      float t = r / (float)RINGS, z = 0.6f - t * (D + 0.6f);
      float hw = cf.half * (1.15f - 0.35f * t) + 0.25f * std::sin(t * 7.0f), hh = MH * (1.0f - 0.3f * t);
      for (int k = 0; k <= SEG; k++) {
        float a = PI * k / SEG, j = jit[r * (SEG + 1) + k];
        if (k == 0 || k == SEG) j = 1;
        g.pos.push_back({std::cos(a) * hw * j, std::max(0.0f, std::sin(a) * hh * j - 0.15f) + (k == 0 || k == SEG ? -0.3f : 0), z + R(-0.15f, 0.15f) * (r > 0 && r < RINGS)});
      }
    }
    for (int r = 0; r < RINGS; r++)
      for (int k = 0; k < SEG; k++) {
        uint32_t a0 = r * (SEG + 1) + k, a1 = a0 + 1, b0 = a0 + SEG + 1, b1 = b0 + 1;
        g.idx.insert(g.idx.end(), {a0, b0, a1, a1, b0, b1});
      }
    uint32_t cbase = (uint32_t)g.pos.size();   // back cap
    g.pos.push_back({0, MH * 0.3f, -D - 0.4f});
    for (int k = 0; k < SEG; k++) g.idx.insert(g.idx.end(), {cbase, (uint32_t)(RINGS * (SEG + 1) + k + 1), (uint32_t)(RINGS * (SEG + 1) + k)});
    computeSmoothNormals(g);
    for (auto& n : g.nrm) n = n * -1.0f;      // lit from inside
    Shape c;
    c.add(g, "#6a645a");
    // the floor: closes the tube underneath (the punched mouth must never show the sky through it)
    {
      Geo f;
      for (int r = 0; r <= RINGS; r++) {
        float t = r / (float)RINGS, z = 0.6f - t * (D + 0.6f), hw = cf.half * (1.15f - 0.35f * t) + 0.25f * std::sin(t * 7.0f) + 0.2f;
        f.pos.push_back({-hw, -0.02f, z}); f.pos.push_back({hw, -0.02f, z});
        f.nrm.push_back({0, 1, 0}); f.nrm.push_back({0, 1, 0});
        if (r > 0) { uint32_t a = (r - 1) * 2, b = r * 2; f.idx.insert(f.idx.end(), {a, b, a + 1, a + 1, b, b + 1}); }
      }
      g_caveFloorShape.add(f, "#3f3b35");
    }
    // the floor: damp gravel strip with stones
    for (int i = 0; i < 26; i++) {
      float z = R(-D + 0.4f, 0.3f), t = -z / D, hw = cf.half * (1.1f - 0.35f * t) - 0.3f;
      g_caveFloorShape.ball(i % 3 ? "#57524a" : "#45413b", {R(-hw, hw), 0.02f, z}, {R(0.15f, 0.4f), R(0.06f, 0.14f), R(0.15f, 0.35f)}, {0, R(0, 3), 0}, 0);
    }
    for (int i = 0; i < 10; i++) {   // dripstones
      float z = R(-D + 1.0f, -1.0f), t = -z / D, L = R(0.2f, 0.6f);
      c.cone("#5a554d", {R(-0.6f, 0.6f), MH * (1 - 0.3f * t) - 0.25f - L * 0.5f, z}, R(0.05f, 0.12f), L, 5, {PI, 0, 0});
    }
    // low boulders at the foot of each side of the mouth, set into the face (none up the arch: they would float
    // in front of the sloping cliff)
    for (int side = -1; side <= 1; side += 2)
      for (int k = 0; k < 3; k++)
        c.ball(k % 2 ? "#6b655b" : "#5f5950", {side * (cf.half + 0.4f + k * 0.5f), 0.25f + k * 0.15f, 0.1f - k * 0.3f}, {R(0.5f, 0.75f), R(0.4f, 0.6f), R(0.45f, 0.6f)}, {0, R(0, 3), 0}, 1);
    g_cave = uploadMesh(c.build(true));
    g_caveFloor = uploadMesh(g_caveFloorShape.build(false));   // drawn without the seasonal material: no snow indoors
    Shape o;  // the hermit's shrine at the back: altar, butter lamps, offerings, flags
    float az = -D + 1.0f;
    o.box("#8a8478", {0, 0.45f, az}, {1.1f, 0.9f, 0.55f});
    o.box("#a8322f", {0, 0.92f, az}, {1.16f, 0.05f, 0.6f});
    for (int i = 0; i < 5; i++) o.tube("#c9a227", {-0.4f + i * 0.2f, 0.99f, az + 0.12f}, 0.045f, 0.035f, 0.08f, 6);
    o.ball("#b91c1c", {0.38f, 0.99f, az - 0.15f}, {0.07f, 0.05f, 0.07f}, {0, 0, 0}, 0);
    o.ball("#e7e5e4", {-0.38f, 0.98f, az - 0.15f}, {0.09f, 0.05f, 0.06f}, {0, 0, 0}, 0);
    o.box("#e7e5e4", {0, 1.35f, az - 0.32f}, {0.32f, 0.48f, 0.04f});
    o.box("#1d4ed8", {0, 1.37f, az - 0.3f}, {0.22f, 0.32f, 0.01f});
    const char* FC[5] = {"#1d4ed8", "#f5f5f4", "#b91c1c", "#15803d", "#eab308"};
    for (int i = 0; i < 9; i++) {
      float t = i / 8.0f;
      o.box(FC[i % 5], {-1.0f + t * 2.0f, MH * 0.72f - 0.3f * 4 * t * (1 - t), -D * 0.6f}, {0.22f, 0.3f, 0.02f});
    }
    g_caveDeco = uploadMesh(o.build(false));
    Shape w;  // the founder's old bow, leaning on the altar
    w.tube("#4a3020", {0.68f, 0.85f, az + 0.1f}, 0.025f, 0.03f, 1.5f, 5, {0, 0, 0.28f});
    w.tube("#d6d3d1", {0.78f, 0.85f, az + 0.14f}, 0.006f, 0.006f, 1.4f, 3, {0, 0, 0.32f});
    g_bowM = uploadMesh(w.build(true));
    // the mouth outline (slightly smaller than the tunnel's first ring) for the depth punch
    std::vector<float> pv;
    const int K = 16;
    for (int k = 0; k < K; k++) {
      float a0 = PI * k / K, a1 = PI * (k + 1) / K;
      float hw = cf.half * 1.05f, hh = MH * 0.95f;
      float v[9] = {0, 0, 0.45f, std::cos(a0) * hw, std::max(0.0f, std::sin(a0) * hh - 0.15f), 0.45f, std::cos(a1) * hw, std::max(0.0f, std::sin(a1) * hh - 0.15f), 0.45f};
      pv.insert(pv.end(), v, v + 9);
    }
    g_punchN = (int)pv.size() / 3;
    glGenVertexArrays(1, &g_punchVao);
    glBindVertexArray(g_punchVao);
    glGenBuffers(1, &g_punchVbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_punchVbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(pv.size() * 4), pv.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
    glBindVertexArray(0);
  }
  // ---- bridges: weathered planks with gaps on two stringers, trestle legs into the river bed, post-and-rail sides
  for (auto& br : g_bridges) {
    Rng rng((uint32_t)(br.x * 13 + br.z * 7));
    auto R = [&](float a, float b) { return a + (b - a) * rng.next(); };
    Shape b;
    const char* PL[4] = {"#7a5a3c", "#6b4e33", "#85664a", "#5e452e"};
    float L = br.len, W = 2.8f;
    // local: +z along the road, y = deck top
    for (float z = -L / 2; z < L / 2; z += 0.27f) {
      if (rng.next() < 0.03f) continue;   // a missing plank here and there
      b.box(PL[(int)(rng.next() * 4)], {R(-0.05f, 0.05f), -0.05f + R(-0.02f, 0.02f), z}, {W + R(-0.15f, 0.15f), 0.08f, 0.24f}, {R(-0.02f, 0.02f), R(-0.04f, 0.04f), R(-0.015f, 0.015f)});
    }
    for (int sd = -1; sd <= 1; sd += 2) {
      b.box("#4f3a27", {sd * 1.0f, -0.25f, 0}, {0.22f, 0.3f, L}, {0, 0, 0});
      // posts and rails
      for (float z = -L / 2 + 0.3f; z <= L / 2; z += 2.0f) b.box("#5a4330", {sd * (W / 2 + 0.05f), 0.5f, z}, {0.14f, 1.1f, 0.14f});
      b.box("#6e5238", {sd * (W / 2 + 0.05f), 0.98f, 0}, {0.1f, 0.1f, L}, {0, 0, 0});
      b.box("#664b33", {sd * (W / 2 + 0.05f), 0.55f, 0}, {0.07f, 0.08f, L}, {0, 0, 0});
    }
    // trestles every 3 m down to the bed
    float depth = br.deck - br.bed + 0.3f;
    for (float z = -L / 2 + 2.5f; z < L / 2 - 2.0f; z += 3.0f) {
      for (int sd = -1; sd <= 1; sd += 2) b.box("#4a3624", {sd * 1.05f, -depth / 2, z}, {0.2f, depth, 0.2f}, {0, 0, sd * 0.06f});
      b.box("#4a3624", {0, -0.45f, z}, {2.4f, 0.16f, 0.16f});
      b.box("#46331f", {0, -depth * 0.5f, z}, {0.12f, depth * 1.05f, 0.12f}, {0, 0, 0.55f});
    }
    br.mesh = uploadMesh(b.build(false));
  }
  // ---- mani heap: a low pile of flat stones with carved, painted slabs set into it
  {
    Rng rng(77);
    auto R = [&](float a, float b) { return a + (b - a) * rng.next(); };
    Shape m;
    const char* SC[3] = {"#77726a", "#817c73", "#6c675f"};
    for (int i = 0; i < 14; i++) {
      float x = R(-0.75f, 0.75f), z = R(-0.35f, 0.35f), y = 0.06f + (1 - std::fabs(x) / 0.9f) * R(0.05f, 0.3f);
      m.box(SC[i % 3], {x, y, z}, {R(0.3f, 0.5f), R(0.07f, 0.12f), R(0.22f, 0.36f)}, {R(-0.1f, 0.1f), R(0, 3), R(-0.1f, 0.1f)});
    }
    for (int k = 0; k < 3; k++) {
      float x = -0.42f + k * 0.42f, tilt = R(-0.25f, -0.12f);
      m.box(SC[k % 3], {x, 0.42f, 0.05f}, {0.36f, 0.5f, 0.08f}, {tilt, R(-0.15f, 0.15f), 0});
      for (int l = 0; l < 4; l++)   // "om mani padme hum" rows, picked out in white and ochre
        m.box(l == 1 ? "#c2410c" : "#e7e5e4", {x, 0.28f + l * 0.09f, 0.1f + (0.28f + l * 0.09f - 0.42f) * -tilt}, {0.26f, 0.035f, 0.012f}, {tilt, 0, 0});
    }
    g_mani = uploadMesh(m.build(false));
  }
  // ---- the abandoned village: dry-stone walls laid course by course, crumbling to rubble
  {
    Rng rng(1913);
    auto R = [&](float a, float b) { return a + (b - a) * rng.next(); };
    Shape r;
    const char* ST[5] = {"#77726a", "#7f7a71", "#6f6a62", "#86817a", "#6a655d"};
    // one wall from (x0,z0) to (x1,z1), up to h metres, its top broken away unevenly
    auto wall = [&](float x0, float z0, float x1, float z1, float h, float keep) {
      float dx = x1 - x0, dz = z1 - z0, L = hypot2(dx, dz), yaw = std::atan2(dx, dz) - PI * 0.5f;
      float ux = dx / L, uz = dz / L;
      for (float y = 0.0f; y < h; y += 0.3f) {
        float s = R(0, 0.3f);
        while (s < L) {
          float len = R(0.35f, 0.7f), t = (s + len * 0.5f) / L;
          float hereH = h * (0.35f + 0.65f * std::fabs(std::sin(t * 4.1f + x0))) * (t < keep ? 1.0f : 0.45f);
          if (y < hereH && rng.next() > 0.05f + 0.5f * (y / std::max(hereH, 0.1f)) * (y / std::max(hereH, 0.1f)))
            r.box(ST[(int)(rng.next() * 5)], {x0 + ux * (s + len * 0.5f) + R(-0.04f, 0.04f), y + 0.14f, z0 + uz * (s + len * 0.5f) + R(-0.04f, 0.04f)},
                  {len - 0.03f, R(0.22f, 0.28f), R(0.38f, 0.48f)}, {R(-0.04f, 0.04f), yaw + R(-0.08f, 0.08f), R(-0.04f, 0.04f)});
          s += len;
        }
      }
      for (int i = 0; i < (int)(L * 1.2f); i++)   // fallen stones along its foot
        r.box(ST[i % 5], {x0 + ux * R(0, L) + uz * R(-1.2f, 1.2f), 0.08f, z0 + uz * R(0, L) - ux * R(-1.2f, 1.2f)}, {R(0.3f, 0.6f), 0.18f, R(0.25f, 0.4f)}, {R(-0.3f, 0.3f), R(0, 3), R(-0.3f, 0.3f)});
    };
    auto house = [&](float x, float z, float W, float D, float h, float rot) {
      float c = std::cos(rot), sn = std::sin(rot);
      auto P = [&](float a, float b, float& ox, float& oz) { ox = x + a * c + b * sn; oz = z - a * sn + b * c; };
      float ax, az, bx, bz, cx, cz, dx, dz;
      P(-W / 2, -D / 2, ax, az); P(W / 2, -D / 2, bx, bz); P(W / 2, D / 2, cx, cz); P(-W / 2, D / 2, dx, dz);
      wall(ax, az, bx, bz, h, 1.0f);
      wall(bx, bz, cx, cz, h * 0.8f, 0.6f);
      float gx, gz; P(-0.6f, D / 2, gx, gz);      // doorway gap on the front
      wall(cx, cz, gx + (cx - gx) * 0.0f, gz, h * 0.6f, 1.0f);
      float g2x, g2z; P(-W / 2 + 0.2f, D / 2, g2x, g2z);
      (void)g2x; (void)g2z;
      wall(dx, dz, ax, az, h * 0.9f, 0.8f);
      // a charred roof beam fallen across the floor
      r.box("#5a412b", {x, 0.35f, z}, {W * 0.95f, 0.18f, 0.2f}, {0.25f, rot + 0.4f, 0.1f});
      r.box("#654a31", {x + 0.6f, 0.15f, z - 0.8f}, {W * 0.6f, 0.14f, 0.16f}, {0.0f, rot - 0.7f, 0.05f});
    };
    house(-6, -2, 6, 5, 1.7f, 0.15f);
    house(5, -4, 5, 4.5f, 1.3f, -0.2f);
    house(1, 7.5f, 7, 5, 1.0f, 0.05f);
    wall(-11, 3, -4, 6, 0.9f, 0.5f);   // a terrace wall
    // the broken chorten: whitewash flaking off its stone, the spire fallen beside it
    r.box("#cfc9bf", {10, 0.4f, 3}, {1.8f, 0.8f, 1.8f});
    r.box("#c4bdb2", {10, 1.05f, 3}, {1.35f, 0.5f, 1.35f});
    r.box("#d1cbc1", {10, 1.45f, 3}, {1.0f, 0.3f, 1.0f});
    r.ball("#c9c2b7", {10, 1.85f, 3}, {0.55f, 0.45f, 0.55f}, {0, 0, 0}, 1);
    r.box("#55514b", {10.25f, 2.05f, 3.2f}, {0.35f, 0.35f, 0.3f}, {0.3f, 0.4f, 0.2f});   // exposed core
    r.tube("#bdb7af", {11.5f, 0.2f, 4.2f}, 0.18f, 0.08f, 1.4f, 8, {0, 0.7f, 1.45f});
    // the diary: a wooden chest under a stone slab by the first house
    r.box("#4a3020", {0, 0.22f, 1}, {0.62f, 0.42f, 0.42f});
    r.box("#2c1d12", {0, 0.45f, 1}, {0.66f, 0.05f, 0.46f});
    r.box("#57534c", {0.1f, 0.52f, 1.05f}, {0.7f, 0.08f, 0.5f}, {0.05f, 0.2f, 0.06f});
    // old prayer-flag pole, the flags long gone to grey rags
    r.box("#5b4632", {-1.5f, 1.8f, 2.6f}, {0.12f, 3.6f, 0.12f}, {0.08f, 0, 0.04f});
    for (int i = 0; i < 3; i++) r.box("#9a958c", {-1.32f, 3.0f - i * 0.4f, 2.6f}, {0.3f, 0.26f, 0.02f}, {0, 0, 0.3f});
    g_ruin = uploadMesh(r.build(false));
  }
  Shape p;  // Himalayan blue poppy clump
  p.tube("#4d7c0f", {0, 0.3f, 0}, 0.012f, 0.015f, 0.6f, 4);
  p.tube("#4d7c0f", {0.15f, 0.25f, 0.05f}, 0.012f, 0.015f, 0.5f, 4, {0.1f, 0, -0.2f});
  for (int i = 0; i < 5; i++) {
    float a = i * TAU / 5;
    p.ball("#4f86f7", {std::cos(a) * 0.06f, 0.62f, std::sin(a) * 0.06f}, {0.07f, 0.02f, 0.07f}, {0.4f * std::sin(a), 0, -0.4f * std::cos(a)}, 1);
    p.ball("#5b93ff", {0.15f + std::cos(a) * 0.05f, 0.51f, 0.05f + std::sin(a) * 0.05f}, {0.06f, 0.018f, 0.06f}, {0.4f * std::sin(a), 0, -0.4f * std::cos(a)}, 1);
  }
  p.ball("#facc15", {0, 0.64f, 0}, {0.025f, 0.025f, 0.025f}, {0, 0, 0}, 0);
  p.ball("#facc15", {0.15f, 0.53f, 0.05f}, {0.022f, 0.022f, 0.022f}, {0, 0, 0}, 0);
  for (int i = 0; i < 6; i++) p.ball("#3f6212", {std::cos(i * 1.1f) * 0.12f, 0.04f, std::sin(i * 1.1f) * 0.12f}, {0.09f, 0.02f, 0.04f}, {0, i * 1.1f, 0}, 0);
  g_poppy = uploadMesh(p.build(true));
  Shape g;  // star relic: a small golden chorten on a stone
  g.box("#57534e", {0, 0.15f, 0}, {0.6f, 0.3f, 0.6f});
  g.box("#d4a017", {0, 0.4f, 0}, {0.36f, 0.2f, 0.36f});
  g.ball("#e3b341", {0, 0.62f, 0}, {0.18f, 0.18f, 0.18f}, {0, 0, 0}, 1);
  g.tube("#e3b341", {0, 0.88f, 0}, 0.02f, 0.07f, 0.3f, 6);
  g_relic = uploadMesh(g.build(false));
  // festival prayer flags: a ring of poles round the village green, strung with five-colour flags
  Shape fl;
  const char* COL[5] = {"#1d4ed8", "#f5f5f4", "#b91c1c", "#15803d", "#eab308"};
  const int NP = 10; const float R = 30;
  std::vector<Vec3> poles;
  for (int i = 0; i < NP; i++) {
    float a = i * TAU / NP + 0.31f, x = std::cos(a) * R, z = std::sin(a) * R;
    float h = gy(x, z);
    poles.push_back({x, h, z});
    fl.box("#5b3a22", {x, h + 2.3f, z}, {0.14f, 4.6f, 0.14f});
  }
  for (int i = 0; i < NP; i++) {
    Vec3 a = poles[i], b = poles[(i + 1) % NP];
    a.y += 4.4f; b.y += 4.4f;
    float yaw = std::atan2(b.x - a.x, b.z - a.z);
    const int N = 16;
    for (int k = 1; k < N; k++) {
      float t = k / (float)N, sag = 1.1f * 4 * t * (1 - t);
      Vec3 q = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t - sag, a.z + (b.z - a.z) * t};
      fl.box(COL[k % 5], {q.x, q.y - 0.22f, q.z}, {0.02f, 0.4f, 0.32f}, {0, yaw, 0});
    }
  }
  g_flags = uploadMesh(fl.build(false));
}

// ---------- blood trails ----------
struct Drop { float x, y, z, yaw, t; };
std::vector<Drop> g_drops;
std::map<int, std::pair<float, float>> g_lastDrop;
const float DROP_LIFE = 240;

// ---------- Migoi prints after fresh snow ----------
std::vector<Spot> g_prints;
int g_printsDay = -1;
Spot g_printEnd;
void layPrints() {
  g_printsDay = season.day;
  g_prints.clear();
  uint32_t h = 2654435761u * (uint32_t)(season.day + 17);
  auto rnd = [&]() { h ^= h << 13; h ^= h >> 17; h ^= h << 5; return (h & 0xffffff) / 16777216.0f; };
  // start somewhere 120..420 m out, walk 90 m in a wandering line toward the mountains
  float a = rnd() * TAU, r = 120 + rnd() * 300;
  float x = std::cos(a) * r, z = std::sin(a) * r, dir = a + (rnd() - 0.5f) * 1.2f;
  for (int i = 0; i < 70; i++) {
    dir += (rnd() - 0.5f) * 0.25f;
    float nx = x + std::cos(dir) * 1.4f, nz = z + std::sin(dir) * 1.4f;
    if (!dry(nx, nz) || world::localSlope(nx, nz) > 0.9f) { dir += 1.2f; continue; }
    x = nx; z = nz;
    float side = (i & 1) ? 0.3f : -0.3f, yaw = std::atan2(std::cos(dir), std::sin(dir));
    float px = x - std::sin(dir) * side, pz = z + std::cos(dir) * side;
    g_prints.push_back({px, gy(px, pz) + 0.02f, pz, yaw});
  }
  g_printEnd = {x, gy(x, z), z, 0};
}

// ---------- state ----------
int g_lastAnnounce = -1, g_bellRungDay = -1, g_bellPaidDay = -1, g_starSeenDay = -1, g_feastDay = -1;
float g_bellSwing = 0, g_lastShot = -99;
bool g_whiteSeen = false;

int festivalOn() {
  if (season.index == 2 && season.dayOfSeason == 5) return 1;
  if (season.index == 3 && season.dayOfSeason == 1) return 2;
  return 0;
}
bool starNight() {
  if (season.index != 3 || season.dayOfSeason != 4) return false;
  int hr = hour();
  return hr >= 21 || hr < 3;
}
bool poppyDay() { return season.index == 0 && season.dayOfSeason == 5; }

// stone and timber pieces use the building material (masonry, wood grain); the grotto uses the rock material
void drawStatic(bool shadow) {
  float lim = shadow ? 0.45f : 1.0f;
  auto one = [&](SeasonKind k, const Spot& o, Mesh& m, float rad, float maxD) {
    if (pdist(o.x, o.z) > maxD * lim) return;
    if (!shadow && !render::sphereVisible({o.x, o.y + rad * 0.4f, o.z}, rad)) return;
    Shader& s = render::object(VFmt::P3C4N4, IM_NONE, k, 0);
    put(s, o.x, o.y, o.z, o.yaw, 1);
    m.draw();
  };
  if (g_top) one(SeasonKind::Build, g_bellS, g_bell, 3, 220);
  for (auto& m : g_maniS) one(SeasonKind::Build, m, g_mani, 1.2f, 120);
  for (auto& br : g_bridges) one(SeasonKind::Build, Spot{br.x, br.deck, br.z, br.yaw}, br.mesh, br.len * 0.6f, 260);
}

}  // namespace

// the cave: punch the mouth out of the depth buffer (after the terrain is drawn), then draw the tunnel through it
const char* PUNCH_VS = R"(
in vec3 a_pos;
uniform mat4 u_m;
void main(){ gl_Position = u_viewProj * (u_m * vec4(a_pos, 1.0)); }
)";
const char* PUNCH_FS = R"(
out vec4 o;
void main(){ gl_FragDepth = 1.0; o = vec4(0.0); }
)";
void drawCave() {
  if (!g_placed || !g_drop.valid() || !g_punchVao) return;
  const CaveFrame& cf = g_cf;
  if (pdist(g_caveS.x, g_caveS.z) > 160 || (pdist(g_caveS.x, g_caveS.z) > cf.depth + 4 && !render::sphereVisible({g_caveS.x, cf.floorY + 2, g_caveS.z}, cf.depth + 2))) return;
  Mat4 M = Mat4::compose({g_caveS.x, cf.floorY, g_caveS.z}, Quat::axisAngle({0, 1, 0}, g_caveS.yaw), {1, 1, 1});
  Shader& ps = render::program("cavepunch", PUNCH_VS, PUNCH_FS);
  glUniformMatrix4fv(ps.loc("u_m"), 1, GL_FALSE, &M.m[0]);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(g_punchVao);
  glDrawArrays(GL_TRIANGLES, 0, g_punchN);
  glBindVertexArray(0);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  Shader& s = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::Rock, OF_DOUBLE);
  put(s, g_caveS.x, cf.floorY, g_caveS.z, g_caveS.yaw, 1);
  float lampK = 0.09f + 0.16f * (1 - clampf(E.daylight, 0, 1));   // lamp light fills the tunnel, most noticeable at night
  render::setEmissive(s, Color(lampK, lampK * 0.7f, lampK * 0.42f), -1.0f);   // -1: indoors, no snow   // the butter lamps' warm glow on the walls
  g_cave.draw();
  render::setEmissive(s, Color(0, 0, 0), 0);
  Shader& fs = obj();
  put(fs, g_caveS.x, cf.floorY, g_caveS.z, g_caveS.yaw, 1);
  g_caveFloor.draw();
  Shader& d = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::Build, 0);
  put(d, g_caveS.x, cf.floorY, g_caveS.z, g_caveS.yaw, 1);
  render::setEmissive(d, Color(0, 0, 0), -1.0f);
  g_caveDeco.draw();
  render::setEmissive(d, Color(0, 0, 0), 0);
  if (!flag("cave_found")) { Shader& w = obj(); put(w, g_caveS.x, cf.floorY, g_caveS.z, g_caveS.yaw, 1); g_bowM.draw(); }
  float c = std::cos(g_caveS.yaw), sn = std::sin(g_caveS.yaw), az = -cf.depth + 1.12f;
  for (int i = 0; i < 5; i++) {
    float lx = -0.4f + i * 0.2f, fl = 0.85f + 0.15f * std::sin(E.clock * 9 + i * 1.7f);
    fx::glow(g_caveS.x + lx * c + az * sn, cf.floorY + 1.08f, g_caveS.z - lx * sn + az * c, 0.13f * fl);
  }
}

bool bridgeDeck(float x, float z, float feetY, float& y) {
  for (auto& br : g_bridges) {
    float dx = x - br.x, dz = z - br.z, c = std::cos(br.yaw), sn = std::sin(br.yaw);
    float lx = dx * c - dz * sn, lz = dx * sn + dz * c;
    if (std::fabs(lx) < 1.35f && std::fabs(lz) < br.len / 2 && feetY > br.deck - 2.2f && feetY < br.deck + 1.2f) { y = br.deck; return true; }
  }
  return false;
}

bool caveFloor(float x, float z, float feetY, float& y) {
  if (!g_placed) return false;
  const CaveFrame& cf = g_cf;
  if (feetY > cf.roofY - 0.5f) return false;
  float dx = x - g_caveS.x, dz = z - g_caveS.z, c = std::cos(g_caveS.yaw), sn = std::sin(g_caveS.yaw);
  float lx = dx * c - dz * sn, lz = dx * sn + dz * c;
  if (lz > 0.8f || lz < -cf.depth + 0.3f) return false;
  float t = std::max(0.0f, -lz / cf.depth), hw = cf.half * (1.1f - 0.35f * t) - 0.35f;
  if (std::fabs(lx) > hw) return false;
  y = cf.floorY;
  if (lz > -2.0f) {   // a ramp from the pool bank up into the mouth: no ledge to stall on
    float t = clampf((0.8f - lz) / 2.8f, 0, 1);
    float out = world::getTerrainHeight(g_caveS.x + 1.4f * sn, g_caveS.z + 1.4f * c);
    y = std::min(cf.floorY, out) + (cf.floorY - std::min(cf.floorY, out)) * t;
  }
  return true;
}

// chimney smoke over every home: soft puffs that rise, widen, lean with the wind and fade (one small draw per home)
const char* SMOKE_VS = R"(
in vec3 a_pos;          // seed: phase, side, size
uniform vec3 u_origin;
out float v_a;
out float v_lit;
void main(){
  float t = fract(a_pos.x + u_clock * 0.07);
  vec2 wind = vec2(cos(u_windT * 0.0 + 0.6), sin(0.6)) * (0.6 + 2.2 * u_windS);
  vec3 p = u_origin + vec3(wind.x * t * 7.0 + (a_pos.y - 0.5) * t * 1.6, t * 7.5, wind.y * t * 7.0 + (a_pos.z - 0.5) * t * 1.6);
  vec4 vp = u_view * vec4(p, 1.0);
  v_a = smoothstep(0.0, 0.08, t) * (1.0 - t) * 0.42;
  v_lit = clamp(0.25 + u_daylight, 0.0, 1.0);
  gl_Position = u_proj * vp;
  gl_PointSize = min((0.6 + 2.6 * t) * (0.8 + a_pos.z * 0.5) * u_viewH * 0.9 / max(-vp.z, 0.5), 220.0);
}
)";
const char* SMOKE_FS = R"(
in float v_a;
in float v_lit;
out vec4 fragColor;
void main(){
  vec2 q = gl_PointCoord - 0.5;
  float d = length(q) * 2.0;
  if (d > 1.0) discard;
  float n = vnoise(gl_PointCoord * 4.0 + v_a * 37.0);
  float a = (1.0 - d * d) * (0.6 + 0.4 * n) * v_a;
  fragColor = vec4(vec3(0.72, 0.71, 0.70) * v_lit, a);
}
)";
GLuint g_smokeVao = 0, g_smokeVbo = 0;
struct Chimney { Vec3 p; };
std::vector<Chimney> g_chim;
size_t g_chimSig = 0;
void drawSmoke() {
  // chimney tops: the highest point of each home's model, turned and placed like the house
  size_t sig = A.structures.size() * 131;
  for (auto& st : A.structures) sig = sig * 31 + (size_t)(st.x * 7 + st.z * 13) + st.kind;
  if (sig != g_chimSig) {
    g_chimSig = sig;
    g_chim.clear();
    for (auto& st : A.structures) {
      if (KINDS[st.kind].group != KG_HOME) continue;
      const BuiltModel& bm = structureModel(st.kind, st.look);
      if (!bm.data || !bm.data->vertexCount) continue;
      const MeshData& md = *bm.data;
      size_t stride = md.verts.size() / md.vertexCount;
      float top = -1e9f;
      for (int i = 0; i < md.vertexCount; i++) top = std::max(top, *(const float*)&md.verts[i * stride + 4]);
      float sx = 0, sz = 0; int n = 0;
      for (int i = 0; i < md.vertexCount; i++) {
        const float* v = (const float*)&md.verts[i * stride];
        if (v[1] > top - 0.08f) { sx += v[0]; sz += v[2]; n++; }
      }
      if (!n) continue;
      sx /= n; sz /= n;
      float c = std::cos(st.rot), sn = std::sin(st.rot);
      g_chim.push_back({{st.x + sx * c + sz * sn, st.y + top + 0.1f, st.z - sx * sn + sz * c}});
    }
  }
  if (std::getenv("HW_SMOKETEST") && g_chim.empty()) g_chim.push_back({{0, gy(0, -12) + 5, -12}});
  if (g_chim.empty()) return;
  if (!g_smokeVao) {
    std::vector<float> seeds(28 * 3);
    Rng rng(611);
    for (int i = 0; i < 28; i++) { seeds[i * 3] = i / 28.0f + rng.next() * 0.03f; seeds[i * 3 + 1] = rng.next(); seeds[i * 3 + 2] = rng.next(); }
    glGenVertexArrays(1, &g_smokeVao);
    glBindVertexArray(g_smokeVao);
    glGenBuffers(1, &g_smokeVbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_smokeVbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(seeds.size() * 4), seeds.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
    glBindVertexArray(0);
  }
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  Shader& s = render::program("chimneysmoke", SMOKE_VS, SMOKE_FS);
  glBindVertexArray(g_smokeVao);
  for (auto& c : g_chim) {
    if (pdist(c.p.x, c.p.z) > 420 || !render::sphereVisible({c.p.x, c.p.y + 4, c.p.z}, 8)) continue;
    s.set3("u_origin", c.p);
    glDrawArrays(GL_POINTS, 0, 28);
  }
  glBindVertexArray(0);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void drawShadow() {
  if (!g_placed || !g_drop.valid()) return;
  drawStatic(true);
}

void reset() {
  g_drops.clear(); g_lastDrop.clear(); g_prints.clear(); g_printsDay = -1;
  g_lastAnnounce = g_bellRungDay = g_bellPaidDay = g_starSeenDay = g_feastDay = -1;
  g_whiteSeen = false;
}

int festivalToday() { return festivalOn(); }

void onWhiteDeerKilled() {
  extras::stat("white_deer_killed");
  actions::notify("You killed the white deer. Old Kalyan will not meet your eye: the valley's luck has gone with it. (+600 renown)");
  actions::addScore(600);
}

void onShot(float x, float z) {
  if (hypot2(x - GORGE_X, z - GORGE_Z) > 110) return;
  if (E.clock - g_lastShot < 3) return;
  g_lastShot = E.clock;
  actions::later(1.1, [] { audio::bowRelease(0.35f, -0.4f); });
  actions::later(2.3, [] { audio::bowRelease(0.15f, 0.5f); });
  actions::later(3.6, [] { audio::bowRelease(0.06f, -0.2f); });
  if (randf() < 0.2f) {
    actions::later(6.0, [] { if (E.daylight < 0.4f) ambience::howl(320); else ambience::ringBell(1, 1); });
    if (!flag("echo_reply")) { setFlag("echo_reply"); actions::notify("Something in the gorge answered your echo..."); }
  }
}

bool arrowStep(const Vec3& a, const Vec3& b) {
  if (!g_placed || !g_top) return false;
  Vec3 c = {g_bellS.x, g_bellS.y + 2.12f, g_bellS.z};
  Vec3 ab = b - a, ac = c - a;
  float L = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
  float t = L > 1e-6f ? std::clamp((ac.x * ab.x + ac.y * ab.y + ac.z * ab.z) / L, 0.0f, 1.0f) : 0;
  Vec3 q = {a.x + ab.x * t - c.x, a.y + ab.y * t - c.y, a.z + ab.z * t - c.z};
  if (q.x * q.x + q.y * q.y + q.z * q.z > 0.42f * 0.42f) return false;
  ambience::ringBell(1, 1);
  g_bellSwing = 1;
  if (g_bellRungDay != season.day) {
    g_bellRungDay = season.day;
    actions::notify("BONG. The bell's note rolls across the valley. Somewhere below, a monk looks up.");
  }
  return true;
}

void update(float) {
  if (!g_placed) place();
  const Vec3& p = E.player.pos;
  float dt = 1.0f / 60;
  g_bellSwing = std::max(0.0f, g_bellSwing - dt * 0.35f);

  // blood: a wounded animal on the move drips every ~1.6 m
  wildlife::forEachWounded([&](float x, float z, float yaw, int id) {
    auto it = g_lastDrop.find(id);
    if (it != g_lastDrop.end() && hypot2(x - it->second.first, z - it->second.second) < 1.6f) return;
    g_lastDrop[id] = {x, z};
    if (!dry(x, z)) return;
    float j = (randf() - 0.5f) * 0.5f;
    g_drops.push_back({x + j, gy(x + j, z) + 0.02f, z - j, yaw + randf() * 3, E.clock});
  });
  if (!g_drops.empty() && E.clock - g_drops.front().t > DROP_LIFE) g_drops.erase(g_drops.begin());
  if (g_drops.size() > 900) g_drops.erase(g_drops.begin(), g_drops.begin() + 100);

  // festivals: announced at dawn, feast in the evening on the village green
  int fest = festivalOn();
  if (fest && g_lastAnnounce != season.day && E.time >= 6) {
    g_lastAnnounce = season.day;
    bool gifted = extras::getStat("fest_gift_day") == season.day + 1;
    if (!gifted) extras::stat("fest_gift_day", season.day + 1 - extras::getStat("fest_gift_day"));
    audio::powerupChime();
    if (fest == 1) actions::notify("Harvest festival today! Flags are up on the green. Bring 25 food to the feast after sunset for renown and a blessing.");
    else actions::notify("Losar — the new year! Flags are up on the green. Bring 25 food to the feast after sunset for renown and a blessing.");
    if (!gifted) actions::addItems({{"arrow", 10}, {"herb", 3}});
    if (!gifted) popup("Festival gift: +10 arrows, +3 herbs", rgbHex("#fbbf24"));
  }
  if (fest && g_feastDay != season.day && E.time >= 18 && hypot2(p.x, p.z) < 40 && extras::getStat("feast_day") != season.day + 1) {
    g_feastDay = season.day;
    extras::stat("feast_day", season.day + 1 - extras::getStat("feast_day"));
    if (invCount("food") >= 25) {
      actions::addItems({{"food", -25}});
      int bonus = fest == 2 ? 600 : 450;
      actions::addScore(bonus);
      E.player.hp = E.player.maxHp;
      audio::victoryFanfare();
      extras::stat("feasts");
      actions::notify(std::string(fest == 2 ? "The Losar feast" : "The harvest feast") + " is a joy: songs, butter tea and dancing. The village is proud of you. +" + std::to_string(bonus) + " renown, fully healed.");
    } else {
      actions::notify("The feast is thin this year: the stores have under 25 food. The villagers eat quietly. (No bonus)");
    }
  }

  // white deer: first sighting
  float wx, wy, wz;
  int ws = wildlife::whiteDeer(wx, wy, wz);
  if (ws == 1 && !g_whiteSeen && pdist(wx, wz) < 45) {
    g_whiteSeen = true;
    if (!flag("white_deer_seen")) {
      setFlag("white_deer_seen");
      actions::addScore(150);
      actions::notify("A white deer, pale as snow. The villagers say it carries the valley's luck — they say no one should shoot it. (+150 renown)");
    }
  }

  // waterfall cave
  float caveAx, caveAz;
  {
    float az = -g_cf.depth + 1.6f;
    caveAx = g_caveS.x + az * std::sin(g_caveS.yaw); caveAz = g_caveS.z + az * std::cos(g_caveS.yaw);
  }
  if (!flag("cave_found") && hypot2(p.x - caveAx, p.z - caveAz) < 2.6f && std::fabs(p.y - 1.6f - g_caveS.y) < 2.5f) {
    setFlag("cave_found");
    audio::powerupChime();
    actions::addScore(500);
    bool had = invCount("recurve_bow") > 0;
    if (had) actions::addItems({{"arrow", 40}, {"sinew", 3}});
    else actions::addItems({{"recurve_bow", 1}});
    actions::notify(had ? "Behind the falls: a hermit's offering and the founder's old bow. You leave the bow, take his arrows. (+40 arrows, +500 renown)"
                                                 : "Behind the falls: a hermit's butter lamps and the founder's old horn bow. It still draws true. (Recurve bow, +500 renown)");
  }

  // mani stones
  for (int i = 0; i < (int)g_maniS.size(); i++) {
    auto& m = g_maniS[i];
    std::string k = "mani_" + std::to_string(i);
    if (extras::getStat(k) > 0 || !near(m.x, m.y, m.z, 2.4f)) continue;
    extras::stat(k);
    int n = 0;
    for (int j = 0; j < 9; j++) n += extras::getStat("mani_" + std::to_string(j)) > 0;
    actions::addScore(n == 9 ? 800 : 60);
    popup("Mani stone " + std::to_string(n) + "/9", rgbHex("#e7e5e4"));
    actions::notify(std::string(MANI[i]) + (n == 9 ? "  — You have read the whole founding story. (+800 renown)" : ""));
  }

  // temple bell gift at the next dawn
  if (g_bellRungDay >= 0 && g_bellPaidDay != g_bellRungDay && season.day > g_bellRungDay && E.time >= 6) {
    g_bellPaidDay = g_bellRungDay;
    actions::addItems({{"herb", 4}, {"arrow", 8}});
    actions::addScore(120);
    extras::stat("bell_gifts");
    actions::notify("The monks heard your bell. At dawn a bundle was left at your door: herbs and arrows. (+120 renown)");
  }

  // star alignment
  if (std::getenv("HW_SECRETS") && (int)E.clock % 5 == 0) { static int lc = -1; if ((int)E.clock != lc) { lc = (int)E.clock; std::printf("star? night=%d idx=%d dos=%d hr=%d d=%.1f seen=%d\n", starNight(), season.index, season.dayOfSeason, hour(), g_top ? pdist(g_top->x, g_top->z) : -1.f, g_starSeenDay); std::fflush(stdout); } }
  if (starNight() && g_top && g_starSeenDay != season.day && pdist(g_top->x, g_top->z) < 45) {
    g_starSeenDay = season.day;
    actions::notify("The Hunter's stars stand over the peak. Where their light falls beside the temple, something glints.");
  }
  if (starNight() && !flag("star_relic") && g_starSeenDay == season.day && near(g_starS.x, g_starS.y, g_starS.z, 2.0f)) {
    setFlag("star_relic");
    audio::victoryFanfare();
    actions::addScore(1500);
    E.player.maxHp += 15; E.player.hp = E.player.maxHp;
    actions::notify("A golden chorten, left by the old gods. You feel stronger. (+15 max health, +1500 renown)");
  }

  // Migoi prints after fresh snow (winter)
  if (season.index == 3 && g_printsDay != season.day) layPrints();
  if (season.index != 3 && !g_prints.empty()) g_prints.clear();
  if (!g_prints.empty() && extras::getStat("migoi_trail_day") != season.day + 1 && near(g_printEnd.x, g_printEnd.y, g_printEnd.z, 3)) {
    extras::stat("migoi_trail_day", season.day + 1 - extras::getStat("migoi_trail_day"));
    actions::addItems({{"wolf_fur", 1}, {"herb", 2}});
    actions::addScore(250);
    actions::notify("The prints end at a scraped hollow in the snow. Something left tufts of white fur and a cache of roots. (+250 renown)");
  }

  // ruined village diary
  if (false && !flag("ruin_diary") && near(g_ruinS.x, g_ruinS.y, g_ruinS.z + 1, 2.2f)) {
    setFlag("ruin_diary");
    audio::powerupChime();
    actions::addScore(700);
    actions::notify("A diary, the ink faded: \"The third winter took the yaks. The spring brought no thaw, and the Migoi came down to the fields. We go back over the pass. May whoever finds this listen to the bell.\" (+700 renown)");
  }

  // blue poppy
  if (poppyDay() && extras::getStat("poppy_year") <= season.day / 28 && near(g_poppyS.x, g_poppyS.y, g_poppyS.z, 2.0f)) {
    extras::stat("poppy_year", season.day / 28 + 1 - extras::getStat("poppy_year"));
    audio::powerupChime();
    actions::addItems({{"herb", 12}});
    E.player.maxHp += flag("poppy_blessed") ? 0 : 10;
    setFlag("poppy_blessed");
    actions::addScore(900);
    actions::notify("A Himalayan blue poppy, open for a single day. Its petals make a rare medicine. (+12 herbs, +900 renown)");
  }
}

void draw() {
  if (!g_placed) return;
  if (!g_drop.valid()) buildMeshes();
  const Vec3& p = E.player.pos;
  Shader& s = obj();
  render::setEmissive(s, Color(0, 0, 0), 0);
  // blood (shrinks in its last minute)
  for (auto& d : g_drops) {
    if (hypot2(d.x - p.x, d.z - p.z) > 45) continue;
    float age = E.clock - d.t, k = std::clamp((DROP_LIFE - age) / 60, 0.0f, 1.0f);
    if (k <= 0.02f) continue;
    put(s, d.x, d.y, d.z, d.yaw, 0.4f + 0.6f * k);
    g_drop.draw();
  }
  for (auto& q : g_prints) {
    if (hypot2(q.x - p.x, q.z - p.z) > 60) continue;
    render::setModel(s, Mat4::compose({q.x, q.y, q.z}, Quat::axisAngle({0, 1, 0}, q.yaw), {1.15f, 1, 1.15f}));
    g_print.draw();
  }
  drawStatic(false);
  Shader& s2 = obj();
  auto show = [&](const Spot& o, Mesh& m, float sc, float rad, float maxD) {
    if (pdist(o.x, o.z) > maxD || !render::sphereVisible({o.x, o.y + rad * 0.5f, o.z}, rad)) return false;
    put(s2, o.x, o.y, o.z, o.yaw, sc);
    m.draw();
    return true;
  };
  if (poppyDay() && show(g_poppyS, g_poppy, 1.3f, 1, 90)) {}
  if (starNight() && !flag("star_relic") && g_starSeenDay == season.day) {
    if (show(g_starS, g_relic, 1.2f, 1.5f, 200)) {
      float tw = 0.7f + 0.3f * std::sin(E.clock * 3);
      fx::glow(g_starS.x, g_starS.y + 0.9f, g_starS.z, 0.6f * tw);
      for (int i = 1; i < 8; i++) fx::glow(g_starS.x, g_starS.y + 1.2f + i * 2.5f, g_starS.z, 0.35f * tw / (1 + i * 0.3f));
    }
  }
  if (festivalOn() && hypot2(p.x, p.z) < 400) {
    Shader& sf = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::Build, 0);
    render::setModel(sf, Mat4::compose({0, 0, 0}, Quat::axisAngle({0, 1, 0}, 0), {1, 1, 1}));
    g_flags.draw();
    if (E.daylight < 0.5f && hypot2(p.x, p.z) < 160)
      for (int i = 0; i < 10; i++) {
        float a = i * TAU / 10 + 0.31f + TAU / 20, x = std::cos(a) * 29, z = std::sin(a) * 29;
        fx::glow(x, gy(x, z) + 3.2f, z, 0.35f);
      }
  }
}

bool where(const char* w, float& x, float& y, float& z) {
  if (!g_placed) place();
  std::string k = w;
  const Spot* o = k == "bell" ? &g_bellS : k == "cave" ? &g_caveS : k == "ruin" ? &g_ruinS : k == "poppy" ? &g_poppyS : k == "star" ? &g_starS : (k == "mani" && !g_maniS.empty()) ? &g_maniS[0] : nullptr;
  if (!o) return false;
  x = o->x; y = o->y; z = o->z;
  return true;
}
}  // namespace secrets
