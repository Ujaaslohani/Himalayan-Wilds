#include <unordered_map>
#include "buildings.h"
#include "village.h"
#include "fx.h"
#include "../models/models.h"
#include "../models/houses.h"
#include "../models/temple.h"
#include "../world/terrain.h"
#include "../render/render.h"

namespace {
float bridgeY(float cx, float cz, float rot, float wy) {
  float hx = std::cos(rot) * (BRIDGE_LENGTH / 2 - 0.8f);
  float hz = -std::sin(rot) * (BRIDGE_LENGTH / 2 - 0.8f);
  float a = world::getTerrainHeight(cx + hx, cz + hz);
  float b = world::getTerrainHeight(cx - hx, cz - hz);
  return std::max({a, b, wy}) + 0.15f - BRIDGE_DECK_Y;
}
}  // namespace

PlacementCheck checkPlacement(const Building& b, float x, float z, float rot) {
  if (b.type == "bridge") {
    RiverHit q = world::riverQuery(x, z);
    if (q.d > q.hw + 12 || q.s < 40) {
      float y = world::getTerrainHeight(x, z);
      return {false, "Bridges must span the river", x, y, z, rot};
    }
    float r = std::atan2(q.tx, q.tz);
    float y = bridgeY(q.cx, q.cz, r, q.wy);
    bool fits = q.hw * 2 + 3 <= BRIDGE_LENGTH;
    return {fits, fits ? "" : "The river is too wide here. Find where it narrows.", q.cx, y, q.cz, r};
  }
  float r = footprint(b.type);
  float off[5][2] = {{0, 0}, {r, 0}, {-r, 0}, {0, r}, {0, -r}};
  float mn = 1e9f, mx = -1e9f;
  for (auto& o : off) {
    float h = world::getTerrainHeight(x + o[0], z + o[1]);
    mn = std::min(mn, h);
    mx = std::max(mx, h);
  }
  float y = mn;
  auto res = [&](const std::string& reason) { return PlacementCheck{reason.empty(), reason, x, y, z, rot}; };
  if (hypot2(x, z) > settlementLimit()) return res("Too far from the village edge");
  if (world::riverMask(x, z) > 0.02f || y < -0.3f || world::lakeAt(x, z, 2) >= 0) return res("Too close to the water");
  if (mx - y > 1.5f) return res("Ground is too steep");
  for (auto& o : A.buildings) {
    if (!o.built || o.id == b.id || o.type == "bridge") continue;
    float need = r + footprint(o.type) + 0.6f;
    if (hypot2(o.pos[0] - x, o.pos[2] - z) < need) return res("Too close to " + o.name);
  }
  return res("");
}

// collision lookup grid: buildings bucketed into 16 m cells, rebuilt only when the building list changes,
// so each villager / player check touches only the few buildings nearby instead of looping over all of them
struct BCircle { float x, z, r; };
static std::unordered_map<int64_t, std::vector<BCircle>> g_bgrid;
static double g_bsig = -1;
static const float BCELL = 16;
static int64_t bkey(int cx, int cz) { return ((int64_t)cx << 32) ^ (uint32_t)cz; }
static void refreshBuildingGrid() {
  double sg = (double)A.buildings.size();
  for (auto& b : A.buildings) sg += (b.built ? 1 : 0) + b.pos[0] * 0.37 + b.pos[2] * 0.91 + b.type.size() * 0.013;
  if (sg == g_bsig) return;
  g_bsig = sg;
  g_bgrid.clear();
  for (auto& b : A.buildings) {
    if (!b.built || b.type == "bridge") continue;
    BCircle c{b.pos[0], b.pos[2], footprint(b.type) * 0.85f};
    for (int cx = (int)std::floor((c.x - c.r - 1) / BCELL); cx <= (int)std::floor((c.x + c.r + 1) / BCELL); cx++)
      for (int cz = (int)std::floor((c.z - c.r - 1) / BCELL); cz <= (int)std::floor((c.z + c.r + 1) / BCELL); cz++) g_bgrid[bkey(cx, cz)].push_back(c);
  }
}

void resolveBuildingCollision(float& px, float& pz, float radius) {
  static unsigned s_frame = ~0u;
  if (s_frame != (unsigned)render::frameNo) { s_frame = (unsigned)render::frameNo; refreshBuildingGrid(); }
  auto it = g_bgrid.find(bkey((int)std::floor(px / BCELL), (int)std::floor(pz / BCELL)));
  if (it != g_bgrid.end()) for (const BCircle& b : it->second) {
    float r = b.r + radius;
    float dx = px - b.x, dz = pz - b.z;
    float d2 = dx * dx + dz * dz;
    if (d2 < r * r && d2 > 1e-6f) {
      float d = std::sqrt(d2);
      px = b.x + (dx / d) * r;
      pz = b.z + (dz / d) * r;
    }
  }
  resolveStructureCollision(px, pz, radius);
  if (px * px + pz * pz > 9000) temple::resolveCollision(px, pz, radius);
}

// bridge railings: keep walkers on the deck between the two fences
void bridgeRailCollision(float& x, float& z, float y) {
  for (auto& b : A.buildings) {
    if (!b.built || b.type != "bridge") continue;
    float c = std::cos(b.rotY), s = std::sin(b.rotY);
    float dx = x - b.pos[0], dz = z - b.pos[2];
    float lx = dx * c - dz * s, lz = dx * s + dz * c;
    float deck = b.pos[1] + BRIDGE_DECK_Y;
    if (std::fabs(lx) > BRIDGE_LENGTH / 2 - 0.3f || std::fabs(lz) > 2.1f || y < deck - 1.0f || y > deck + 2.6f) continue;
    const float lim = 1.12f;
    if (std::fabs(lz) > lim) {
      lz = lz > 0 ? lim : -lim;
      x = b.pos[0] + lx * c + lz * s;
      z = b.pos[2] - lx * s + lz * c;
    }
  }
}

float walkHeight(float x, float z) {
  float h = world::getTerrainHeight(x, z);
  if (x * x + z * z > 9000) {
    h = world::stepHeightAt(x, z, h);
    h = temple::ground(x, z, h);
  }
  for (auto& b : A.buildings) {
    if (!b.built || b.type != "bridge") continue;
    float c = std::cos(b.rotY), s = std::sin(b.rotY);
    float lx = (x - b.pos[0]) * c - (z - b.pos[2]) * s;
    float lz = (x - b.pos[0]) * s + (z - b.pos[2]) * c;
    if (std::fabs(lz) >= 1.5f) continue;
    float deck = b.pos[1] + BRIDGE_DECK_Y, ax = std::fabs(lx);
    if (ax < BRIDGE_LENGTH / 2) h = std::max(h, deck);
    else if (ax < BRIDGE_LENGTH / 2 + BRIDGE_RAMP) h = std::max(h, deck - (ax - BRIDGE_LENGTH / 2) * (BRIDGE_RAMP_DROP / BRIDGE_RAMP));
  }
  return h;
}

namespace buildings {

static void drawAll() {
  for (auto& b : A.buildings) {
    if (!b.built) continue;
    Shader& s = render::object(VFmt::P3C4, IM_NONE, SeasonKind::Build, OF_WIND);
    render::setModel(s, Mat4::compose({b.pos[0], b.pos[1], b.pos[2]}, Quat::axisAngle({0, 1, 0}, b.rotY), {1, 1, 1}));
    models::building(b.type).draw();
  }
}

void draw() {
  glEnable(GL_CULL_FACE);
  drawAll();
  for (auto& b : A.buildings)
    if (b.built && b.type == "campfire") fx::campfire(b.pos[0], b.pos[1], b.pos[2], 1);
}

void drawShadow() { drawAll(); }

void drawPreview() {
  auto& w = E.world;
  if (w.placing.empty()) return;
  {
    // build mode: a dotted ring of small lights marks how far out you can build
    float R = settlementLimit();
    const Vec3& p = E.player.pos;
    int n = std::max(24, (int)(TAU * R / 5.0f));
    for (int i = 0; i < n; i++) {
      float a = i * TAU / n, x = std::cos(a) * R, z = std::sin(a) * R;
      if (hypot2(x - p.x, z - p.z) > 70) continue;
      fx::glow(x, world::getTerrainHeight(x, z) + 0.35f, z, 0.45f);
    }
  }
  Mat4 m = Mat4::compose(w.previewPos, Quat::axisAngle({0, 1, 0}, w.previewRot), {1, 1, 1});
  if (w.placeKind != SK_COUNT && w.hasPlaceLook) {
    const BuiltModel& bm = structureModel(w.placeKind, w.placeLook);
    fx::ghost(bm.mesh, m, w.previewValid, true);
    return;
  }
  for (auto& b : A.buildings)
    if (b.id == w.placing) fx::ghost(&models::building(b.type), m, w.previewValid, false);
}

}  // namespace buildings

float settlementExtent() {
  static double lastT = -1;
  static float ext = 0;
  if (lastT >= 0 && std::fabs(E.clock - lastT) < 0.5) return ext;  // cheap: recount twice a second
  lastT = E.clock;
  float e = 0;
  for (auto& b : A.buildings)
    if (b.built && b.type != "bridge") e = std::max(e, hypot2(b.pos[0], b.pos[2]));
  for (auto& st : A.structures)
    if (st.kind != SK_PATH) e = std::max(e, hypot2(st.x, st.z));
  ext = e;
  return ext;
}
float settlementLimit() { return std::min(220.0f, std::max(SETTLEMENT_LIMIT, settlementExtent() + 30.0f)); }
float settlementSafe() { return std::min(170.0f, std::max(40.0f, settlementExtent() + 20.0f)); }
