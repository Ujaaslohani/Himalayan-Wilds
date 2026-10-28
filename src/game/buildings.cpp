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
  if (hypot2(x, z) > SETTLEMENT_LIMIT) return res("Too far from the village hearth");
  if (world::riverMask(x, z) > 0.02f || y < -0.3f || world::lakeAt(x, z, 2) >= 0) return res("Too close to the water");
  if (mx - y > 1.5f) return res("Ground is too steep");
  for (auto& o : A.buildings) {
    if (!o.built || o.id == b.id || o.type == "bridge") continue;
    float need = r + footprint(o.type) + 0.6f;
    if (hypot2(o.pos[0] - x, o.pos[2] - z) < need) return res("Too close to " + o.name);
  }
  return res("");
}

void resolveBuildingCollision(float& px, float& pz, float radius) {
  for (auto& b : A.buildings) {
    if (!b.built || b.type == "bridge") continue;
    float r = footprint(b.type) * 0.85f + radius;
    float dx = px - b.pos[0], dz = pz - b.pos[2];
    float d2 = dx * dx + dz * dz;
    if (d2 < r * r && d2 > 1e-6f) {
      float d = std::sqrt(d2);
      px = b.pos[0] + (dx / d) * r;
      pz = b.pos[2] + (dz / d) * r;
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
