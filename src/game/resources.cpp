#include "../render/sky.h"
#include "resources.h"
#include "engine.h"
#include "npcs.h"
#include "../world/terrain.h"
#include "../assets/assets.h"
#include "../render/render.h"
#include "../models/models.h"
#include "../platform/platform.h"
#include <cmath>

const ResInfo RES_INFO[4] = {
    {"Fallen Deodar Log", "wood", 1, 1, 300, MI_WOOD},
    {"Slate Outcrop", "stone", 1, 1, 300, MI_STONE},
    {"Mountain Nettle", "fiber", 1, 1, 210, MI_FIBER},
    {"Wild Artemisia", "herb", 1, 1, 240, MI_HERB},
};

namespace {
std::vector<ResNode> g_nodes;
bool g_dirty = true;
const char* MODEL_FILE[4] = {nullptr, "Rock Medium.glb", "Bush.glb", "Flower Group.glb"};
const char* TINT[4] = {nullptr, "#9fb4cc", "#d9f07a", "#ffffff"};

struct RPart { const AssetPart* ap; InstBatch nearB, farB, shadowB; };
struct Layer {
  ResType type;
  std::vector<RPart> parts;
  DynBatch log, logShadow;
  std::vector<InstFull> all, nearV, farV, shadowV;
  uint64_t sig = 0;
};
const float R_MID = 24, R_BAND = 8, R_MARGIN = 2, R_SHADOW = 46;
unsigned g_gen = 1;
std::vector<Layer> g_layers;

bool okSpot(ResType t, float x, float z) {
  if (std::fabs(x) > MAP_HALF - 25 || std::fabs(z) > MAP_HALF - 25) return false;
  if (hypot2(x, z) < SETTLEMENT_RADIUS + 5) return false;
  for (auto& c : npcs::CAMPS)
    if (hypot2(x - c.campX, z - c.campZ) < 9) return false;
  if (world::riverMask(x, z) > 0.02f || world::pathDistance(x, z) < 3.5f || world::nearRoute(x, z) < 5.5f) return false;
  for (const Shrine& sh : world::SHRINES) if (hypot2(x - sh.x, z - sh.z) < 13.0f + 10) return false;
  if (world::lakeAt(x, z, 4) >= 0) return false;
  float h = world::getTerrainHeight(x, z);
  if (h < -0.4f || h > 42) return false;
  float s = std::fabs(world::getTerrainHeight(x + 1.5f, z) - world::getTerrainHeight(x - 1.5f, z)) + std::fabs(world::getTerrainHeight(x, z + 1.5f) - world::getTerrainHeight(x, z - 1.5f));
  if (s > 1.6f) return false;
  float f = world::forestDensity(x, z);
  if (t == RES_WOOD) return f > 0.2f;
  if (t == RES_FIBER) return f < 0.75f;
  return true;
}
}  // namespace

namespace resources {

std::vector<std::string> requiredFiles() { return {"Rock Medium.glb", "Bush.glb", "Flower Group.glb"}; }

void spawn(uint32_t seed) {
  g_nodes.clear();
  Rng rnd(seed);
  auto r = [&] { return rnd.next(); };
  std::pair<ResType, int> plan[4] = {{RES_WOOD, 9}, {RES_STONE, 8}, {RES_FIBER, 8}, {RES_HERB, 7}};
  int id = 1;
  for (auto& pl : plan) {
    ResType type = pl.first;
    int clusters = pl.second;
    for (int c = 0; c < clusters; c++) {
      float ring = (float)c / clusters;
      float rMin = ring < 0.4f ? 26 : ring < 0.85f ? 70 : 140;
      float rMax = ring < 0.4f ? 70 : ring < 0.85f ? 140 : 205;
      float cx = 0, cz = 0;
      bool found = false;
      for (int k = 0; k < 80 && !found; k++) {
        float a = r() * TAU, rr = rMin + r() * (rMax - rMin);
        cx = std::cos(a) * rr;
        cz = std::sin(a) * rr;
        found = okSpot(type, cx, cz);
        if (found)
          for (auto& n : g_nodes)
            if (hypot2(n.x - cx, n.z - cz) < 12) { found = false; break; }
      }
      if (!found) continue;
      int count = 2 + (int)std::floor(r() * 3);
      for (int i = 0; i < count; i++) {
        float x = cx, z = cz;
        for (int k = 0; k < 12; k++) {
          x = cx + (r() - 0.5f) * 9;
          z = cz + (r() - 0.5f) * 9;
          bool ok = okSpot(type, x, z);
          if (ok)
            for (auto& n : g_nodes)
              if (hypot2(n.x - x, n.z - z) < 2.6f) { ok = false; break; }
          if (ok) break;
        }
        const ResInfo& info = RES_INFO[type];
        ResNode n;
        n.id = id++;
        n.type = type;
        n.x = x;
        n.z = z;
        n.y = world::getTerrainHeight(x, z);
        n.rot = r() * TAU;
        n.scale = type == RES_STONE ? 0.55f + r() * 0.3f : type == RES_WOOD ? 0.85f + r() * 0.3f : 0.7f + r() * 0.3f;
        n.left = info.harvests;
        n.regrowAt = 0;
        g_nodes.push_back(n);
      }
    }
  }
  g_dirty = true;
}

const std::vector<ResNode>& nodes() { return g_nodes; }

ResNode* find(float x, float z, float r) {
  ResNode* best = nullptr;
  float bd = r * r;
  for (auto& n : g_nodes) {
    if (n.left <= 0) continue;
    float d2 = (n.x - x) * (n.x - x) + (n.z - z) * (n.z - z);
    if (d2 < bd) { bd = d2; best = &n; }
  }
  return best;
}

const ResNode* nearestOfType(ResType t, float x, float z) {
  const ResNode* best = nullptr;
  float bd = 1e30f;
  for (auto& n : g_nodes) {
    if (n.type != t || n.left <= 0) continue;
    float d2 = (n.x - x) * (n.x - x) + (n.z - z) * (n.z - z);
    if (d2 < bd) { bd = d2; best = &n; }
  }
  return best;
}

HarvestResult harvest(ResNode& n) {
  const ResInfo& info = RES_INFO[n.type];
  auto& b = E.world.bonus;
  int extra = n.type == RES_WOOD ? b.woodExtra : n.type == RES_STONE ? b.stoneExtra : n.type == RES_HERB ? b.herbExtra : 0;
  n.left -= 1;
  if (n.left <= 0) n.regrowAt = E.clock + info.respawn * (extra > 0 ? 0.6f : 1.0f);
  g_dirty = true;
  return {info.item, info.amount, info.name, n.left <= 0};
}

void update() {
  const Vec3& p = E.player.pos;
  for (auto& n : g_nodes) {
    if (n.left > 0 || E.clock < n.regrowAt) continue;
    if (hypot2(p.x - n.x, p.z - n.z) < 30) continue;
    n.left = RES_INFO[n.type].harvests;
    g_dirty = true;
  }
}

void clearAround(float x, float z, float r) {
  for (auto& n : g_nodes)
    if ((n.x - x) * (n.x - x) + (n.z - z) * (n.z - z) < r * r) {
      n.left = 0;
      n.regrowAt = 1e30f;
      g_dirty = true;
    }
}

void initRender() {
  registerMarkers("resources", [](std::vector<Marker>& out) {
    for (auto& n : g_nodes) {
      if (n.left <= 0) continue;
      out.push_back({n.x, n.y + (n.type == RES_STONE ? 2.0f : 1.7f), n.z, (MarkerIcon)RES_INFO[n.type].icon, RES_INFO[n.type].name});
    }
  });
  g_layers.clear();
  for (int t = 0; t < 4; t++) {
    Layer L;
    L.type = (ResType)t;
    if (t == RES_WOOD) { L.log.init(&models::log()); L.logShadow.init(&models::log()); }
    else {
      const Asset* a = assets::get(MODEL_FILE[t]);
      if (a)
        for (auto& p : a->parts) {
          RPart r{&p, {}, {}, {}};
          r.nearB.create(&p.mesh, InstKind::Full);
          r.farB.create(p.lod.vao ? &p.lod : &p.mesh, InstKind::Full);
          r.shadowB.create(&p.mesh, InstKind::Full);
          L.parts.push_back(r);
        }
    }
    g_layers.push_back(std::move(L));
  }
  g_dirty = true;
}

static void rebuild() {
  sky::invalidateStaticShadow();
  for (auto& L : g_layers) {
    L.all.clear();
    for (auto& node : g_nodes) {
      if (node.type != L.type || node.left <= 0) continue;
      const ResInfo& info = RES_INFO[node.type];
      float grow = 0.6f + 0.4f * ((float)node.left / info.harvests);
      float s = node.scale * grow;
      Mat4 m = Mat4::compose({node.x, node.y - (node.type == RES_STONE ? 0.15f : 0), node.z}, Quat::axisAngle({0, 1, 0}, node.rot), {s, s, s});
      L.all.push_back(toInst(m));
    }
  }
  g_gen++;
}

float outerR() { return g_settings.fullMap ? 700.0f : std::max(160.0f, 110.0f * g_settings.drawDist()); }

static void cull() {
  const Camera& cam = render::cam;
  float R = outerR();
  for (auto& L : g_layers) {
    L.nearV.clear(); L.farV.clear(); L.shadowV.clear();
    uint64_t h = 1469598103934665603ull ^ g_gen;
    for (size_t i = 0; i < L.all.size(); i++) {
      const InstFull& it = L.all[i];
      float x = it.r0[3], y = it.r1[3], z = it.r2[3];
      float d = hypot2(x - cam.pos.x, z - cam.pos.z);
      if (d > R) continue;
      if (d < R_SHADOW) L.shadowV.push_back(it);
      if (d > 6 && !cam.box({x - 3, y - 1, z - 3}, {x + 3, y + 4, z + 3})) continue;
      bool nr = L.type == RES_WOOD || d < R_MID + R_BAND + R_MARGIN, fr = L.type != RES_WOOD && d > R_MID - R_MARGIN;
      if (nr) L.nearV.push_back(it);
      if (fr) L.farV.push_back(it);
      h = (h ^ (i * 4 + nr * 2 + fr + 1)) * 1099511628211ull;
    }
    h ^= L.shadowV.size() * 0x9E3779B97F4A7C15ull;
    if (h == L.sig) continue;
    L.sig = h;
    if (L.type == RES_WOOD) {
      L.log.items = L.nearV; L.log.upload();
      L.logShadow.items = L.shadowV; L.logShadow.upload();
    } else
      for (auto& p : L.parts) {
        p.nearB.upload(L.nearV.data(), L.nearV.size());
        p.farB.upload(L.farV.data(), L.farV.size());
        p.shadowB.upload(L.shadowV.data(), L.shadowV.size());
      }
  }
}


void draw() {
  if (g_dirty) { g_dirty = false; rebuild(); }
  cull();
  float R = outerR();
  for (auto& L : g_layers) {
    if (L.type == RES_WOOD) {
      Shader& s = render::object(VFmt::P3C4, IM_FULL, SeasonKind::None, 0);
      s.set2f("u_fade", R * 0.88f, R);
      glEnable(GL_CULL_FACE);
      L.log.draw();
      s.set2f("u_fade", 0, 0);
      continue;
    }
    glDisable(GL_CULL_FACE);
    for (auto& p : L.parts) {
      AssetMaterial m = p.ap->mat;
      m.base = Color::hex(TINT[L.type]);
      m.kind = assets::kindFor(m.name, "under");
      Shader& s = render::objectFor(m, IM_FULL);
      s.set2f("u_fade", R_MID, R_MID + R_BAND);
      p.nearB.draw();
      s.set2f("u_fade", R * 0.88f, R);
      s.set2f("u_fadeIn", R_MID, R_MID + R_BAND);
      p.farB.draw();
      s.set2f("u_fade", 0, 0);
      s.set2f("u_fadeIn", 0, 0);
    }
  }
}

void drawShadow() {
  for (auto& L : g_layers) {
    if (L.type == RES_WOOD) {
      render::object(VFmt::P3C4, IM_FULL, SeasonKind::None, 0);
      L.logShadow.draw();
      continue;
    }
    for (auto& p : L.parts) {
      render::objectFor(p.ap->mat, IM_FULL);
      p.shadowB.draw();
    }
  }
}

void shutdown() {
  for (auto& L : g_layers) {
    L.log.destroy();
    L.logShadow.destroy();
    for (auto& p : L.parts) { p.nearB.destroy(); p.farB.destroy(); p.shadowB.destroy(); }
  }
  g_layers.clear();
}

}  // namespace resources
