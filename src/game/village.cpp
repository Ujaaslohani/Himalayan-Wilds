#include "../assets/bakeao.h"
#include "../render/envmap.h"
#include "events.h"
#include "../render/sky.h"
#include "village.h"
#include "../render/mhuman.h"
#include "buildings.h"
#include "resources.h"
#include "npcs.h"
#include "fx.h"
#include "../models/houses.h"
#include "../models/people.h"
#include "../models/models.h"
#include "../models/temple.h"
#include "../models/shape.h"
#include "../world/terrain.h"
#include "../world/forest.h"
#include "../render/render.h"
#include <map>
#include <set>

const char* STAGE_NAMES[5] = {"Clearing", "Growing Village", "Established Settlement", "Market Town", "Temple Town"};

namespace {
const float BOX[SK_COUNT][4] = {
    {-2.3f, 2.3f, -2.0f, 2.0f}, {-2.6f, 3.4f, -2.3f, 2.4f}, {-3.2f, 6.3f, -2.6f, 2.6f}, {-4.8f, 4.8f, -2.8f, 2.8f},
    {-1.7f, 1.7f, -1.2f, 0.9f}, {-2.6f, 2.6f, -1.9f, 1.9f}, {-5.3f, 5.3f, -3.8f, 3.8f}, {-2.6f, 2.6f, -1.8f, 1.8f},
    {-1.5f, 1.5f, -0.08f, 0.08f}, {0, 0, 0, 0}, {-0.12f, 0.12f, -0.12f, 0.12f}, {-0.5f, 0.5f, -0.5f, 0.5f}, {-0.7f, 0.7f, -0.25f, 0.25f},
};

void toWorld(const Structure& s, float lx, float ly, float lz, Vec3& out) {
  float c = std::cos(s.rot), sn = std::sin(s.rot);
  out = {s.x + lx * c + lz * sn, s.y + ly, s.z - lx * sn + lz * c};
}

struct Resident {
  int id;
  float x, y, z, yaw, speed, phase, tx, tz;
  int mode;
  bool goingHome;
  float t;
  uint32_t tint;
  int home;
  std::string job;
  bool working;
  float dance;
  float talkT = 0, lookT = 0, lookX = 0, lookZ = 0, stuckT = 0, lastX = 0, lastZ = 0, slow = 0;
  int talkWith = -1;
  bool deliver = false;
  std::vector<P2> path;  // road waypoints toward the final target
  size_t pathI = 0;
  float occT = 0;
  bool hidden = false;
  float hunger = 0, tired = 0, lonely = 0;
  float px = 0, py = 0, pz = 0, pyaw = 0, pphase = 0;
  bool pv = false;  // needs 0..1 that steer what a villager does next
  std::string autoJob;
  int nodeId = -1;  // resource node being worked (tree, rock, herb patch)
  int carry = -1;   // job index of the haul being carried home (-1 none)
  float headYaw = 0;
};
enum { RM_WALK, RM_IDLE, RM_INSIDE };
std::vector<Resident> g_res;
float g_ralpha = 1;
int g_nextResident = 1;
const char* TINTS[16] = {"#4d5d3a", "#8b5a2b", "#9f1239", "#57534e", "#1e3a8a", "#7c2d12", "#166534", "#a16207", "#6b21a8", "#0f766e",
                         "#d6cfc2", "#b45309", "#334155", "#be185d", "#365314", "#78350f"};
const char* CAPS[10] = {"#7f1d1d", "#1e3a8a", "#14532d", "#78350f", "#e7e5e4", "#831843", "#3f3f46", "#a16207", "#0e7490", "#57534e"};
const char* PANTS[8] = {"#e3d8c2", "#44403c", "#57534e", "#d6cfc2", "#3f2a1c", "#1f2937", "#8a7f73", "#c2b59b"};
uint32_t hashTint(const char* const* pal, int n, int id, int salt) { uint32_t h = (uint32_t)id * 2654435761u ^ (uint32_t)salt * 40503u; h ^= h >> 13; return rgbHex(pal[h % (uint32_t)n]); }
const char* JOB_KEYS[4] = {"woodcutter", "quarry", "herbalist", "hunter"};
int JOB_RES[4] = {RES_WOOD, RES_STONE, RES_HERB, -1};

bool homeDoor(int uid, float& x, float& z) {
  for (auto& s : A.structures)
    if (s.uid == uid) {
      Vec3 d = structureModel(s.kind, s.look).door;
      Vec3 w;
      toWorld(s, d.x, 0, d.z, w);
      x = w.x;
      z = w.z;
      return true;
    }
  return false;
}

std::vector<std::pair<float, float>> gatherSpots() {
  std::vector<std::pair<float, float>> out;
  for (auto& s : A.structures) {
    KindGroup g = KINDS[s.kind].group;
    if (g == KG_VILLAGE || s.kind == SK_BENCH || s.kind == SK_FLAGPOLE) {
      Vec3 d = structureModel(s.kind, s.look).door, w;
      toWorld(s, d.x, 0, d.z, w);
      out.push_back({w.x, w.z});
    }
  }
  for (auto& b : A.buildings) {
    if (!b.built || b.type == "bridge") continue;
    float r = footprint(b.type) + 1.6f;
    out.push_back({b.pos[0] + std::sin(b.rotY) * r, b.pos[2] + std::cos(b.rotY) * r});
  }
  return out;
}

void assignHomes() {
  std::vector<int> homes;
  for (auto& s : A.structures)
    for (int i = 0; i < KINDS[s.kind].capacity; i++) homes.push_back(s.uid);
  for (size_t i = 0; i < g_res.size(); i++) g_res[i].home = i < homes.size() ? homes[i] : 0;
}

bool workSpot(Resident& r, float& ox, float& oz) {
  if (r.job.empty()) return false;
  int ji = 0;
  for (int k = 0; k < 4; k++) if (r.job == JOB_KEYS[k]) ji = k;
  int res = JOB_RES[ji];
  if (res >= 0) {
    const ResNode* n = resources::nearestOfType((ResType)res, r.x * 0.3f, r.z * 0.3f);
    if (n && hypot2(n->x, n->z) < settlementLimit() + 20) {
      r.nodeId = n->id;
      float a = randf() * TAU;
      ox = n->x + std::cos(a) * 1.6f;
      oz = n->z + std::sin(a) * 1.6f;
      return true;
    }
  }
  r.nodeId = -1;
  float a = randf() * TAU, d = res >= 0 ? 30 + randf() * 15 : settlementSafe() + 8 + randf() * 25;  // hunters go past the settlement edge
  ox = std::cos(a) * d;
  oz = std::sin(a) * d;
  return true;
}

// long walks follow the nearest road: join it, walk along its points, leave it near the target
static void planRoute(Resident& r) {
  r.path.clear(); r.pathI = 0;
  float dist = hypot2(r.tx - r.x, r.tz - r.z);
  if (dist < 22) return;
  int best = -1; size_t ia = 0, ib = 0; float bestCost = 1e9f;
  for (size_t ri = 0; ri < world::ROADS.size(); ri++) {
    const auto& P = world::ROADS[ri].pts;
    float da = 1e9f, db = 1e9f; size_t a = 0, b = 0;
    for (size_t i = 0; i < P.size(); i += 2) {
      float d1 = hypot2(P[i][0] - r.x, P[i][1] - r.z), d2 = hypot2(P[i][0] - r.tx, P[i][1] - r.tz);
      if (d1 < da) { da = d1; a = i; }
      if (d2 < db) { db = d2; b = i; }
    }
    if (da > 18 || db > 18) continue;
    float cost = da + db;
    if (cost < bestCost && cost < dist * 0.6f) { bestCost = cost; best = (int)ri; ia = a; ib = b; }
  }
  if (best < 0) return;
  const auto& P = world::ROADS[best].pts;
  int step = ib >= ia ? 3 : -3;
  for (long i = (long)ia; step > 0 ? i < (long)ib : i > (long)ib; i += step) r.path.push_back(P[(size_t)i]);
  r.path.push_back(P[ib]);
}

void pickTarget(Resident& r, bool night) {
  float roll = randf();
  r.goingHome = false;
  r.working = false;
  if (E.festival.t > 0) {
    r.dance += (randf() - 0.5f) * 0.6f;
    float R = 4.6f + (r.id % 3) * 1.1f;
    r.tx = std::cos(r.dance) * R;
    r.tz = std::sin(r.dance) * R;
    return;
  }
  if (E.raid.active && r.home) {
    float x, z;
    if (homeDoor(r.home, x, z)) { r.tx = x; r.tz = z; r.goingHome = true; return; }
  }
  // daily routines: a night watchman patrols the settlement edge with a torch; by day stall keepers mind the market
  {
    int idx = (int)(&r - g_res.data());
    if (night && A.population >= 3 && idx == 0) {
      float R = settlementSafe() * 0.65f;
      r.dance += 0.45f;  // next point along the patrol ring
      r.tx = std::cos(r.dance) * R; r.tz = std::sin(r.dance) * R;
      r.goingHome = false;
      return;
    }
    if (!night && E.daylight > 0.4f && A.population >= 4 && idx >= 1) {
      int stall = 0;
      for (auto& st : A.structures)
        if (st.kind == SK_MARKET_STALL) {
          if (++stall == idx) {
            float c = std::cos(st.rot), sn = std::sin(st.rot);
            r.tx = st.x - sn * 0.6f; r.tz = st.z - c * 0.6f;  // behind the counter
            r.working = true;
            return;
          }
        }
    }
  }
  // needs: the most pressing one above its threshold decides the trip
  if (r.hunger > 0.75f) {  // eat: food store or the village fire
    float fx = 0, fz = 0;
    for (auto& s : A.structures) if (s.kind == SK_FOOD_STORE) { fx = s.x; fz = s.z; break; }
    float a = randf() * TAU; r.tx = fx + std::cos(a) * 2.5f; r.tz = fz + std::sin(a) * 2.5f; r.hunger = 0; return;
  }
  if (r.tired > 0.85f && r.home) {
    float x, z;
    if (homeDoor(r.home, x, z)) { r.tx = x; r.tz = z; r.goingHome = true; r.tired = 0; return; }
  }
  if (r.lonely > 0.7f) {
    auto sp = gatherSpots();
    if (!sp.empty()) { auto& s = sp[(size_t)(randf() * sp.size()) % sp.size()]; r.tx = s.first + (randf() - 0.5f) * 2.5f; r.tz = s.second + (randf() - 0.5f) * 2.5f; return; }
  }
  // rain: head home (or to the nearest gathering roof) instead of wandering
  if ((sky::rainAmount() > 0.4f || sky::blizzard() > 0.4f) && r.home) {
    float x, z;
    if (homeDoor(r.home, x, z)) { r.tx = x; r.tz = z; r.goingHome = true; return; }
  }
  // a worker who just finished a stint carries the haul back to the village centre
  if (r.deliver) {
    r.deliver = false;
    // the stint is done: the tree / rock / herb patch actually loses a harvest (it disappears when spent and regrows later)
    int ji = -1;
    for (int k = 0; k < 4; k++) if (r.job == JOB_KEYS[k]) ji = k;
    if (r.nodeId >= 0) {
      if (ResNode* n = resources::byId(r.nodeId)) if (n->left > 0) resources::harvest(*n);
      r.nodeId = -1;
    }
    r.carry = ji == 3 ? (randf() < 0.6f ? 3 : -1) : ji;  // hunters don't always make a kill
    float a = randf() * TAU;
    r.tx = std::cos(a) * 3.0f; r.tz = std::sin(a) * 3.0f;
    return;
  }
  // dusk: most villagers drift toward the fire and gathering spots
  bool dusk = E.daylight > 0.15f && E.daylight < 0.45f;
  if (dusk && roll < 0.7f) {
    auto sp = gatherSpots();
    if (!sp.empty() && randf() < 0.5f) { auto& s = sp[(size_t)(randf() * sp.size()) % sp.size()]; r.tx = s.first + (randf() - 0.5f) * 3; r.tz = s.second + (randf() - 0.5f) * 3; return; }
    float a = randf() * TAU; r.tx = std::cos(a) * (3.2f + randf() * 1.5f); r.tz = std::sin(a) * (3.2f + randf() * 1.5f); return;
  }
  if (!night && !r.job.empty() && E.daylight > 0.45f && roll < 0.75f) {
    float x, z;
    if (workSpot(r, x, z)) { r.tx = x; r.tz = z; r.working = true; return; }
  }
  if (r.home && (night ? roll < 0.6f : roll < 0.3f)) {
    float x, z;
    if (homeDoor(r.home, x, z)) { r.tx = x; r.tz = z; r.goingHome = night; return; }
  }
  auto spots = gatherSpots();
  if (night || roll < 0.55f) {
    if (!spots.empty() && (!night || randf() < 0.5f)) {
      auto& s = spots[(size_t)(randf() * spots.size()) % spots.size()];
      r.tx = s.first + (randf() - 0.5f) * 3;
      r.tz = s.second + (randf() - 0.5f) * 3;
      return;
    }
    float a = randf() * TAU;
    r.tx = std::cos(a) * 3.6f;
    r.tz = std::sin(a) * 3.6f;
    return;
  }
  float a = randf() * TAU, d = 6 + randf() * 26;
  r.tx = std::cos(a) * d;
  r.tz = std::sin(a) * d;
}

struct StructBatch { const Mesh* mesh; DynBatch batch; };
std::vector<std::unique_ptr<StructBatch>> g_structBatches;
bool g_structDirty = true;
std::vector<Vec3> g_staticLights;
DynBatch g_torsos[3], g_heads[3], g_arms, g_legs;
DynBatch g_sTorsos[3], g_sHeads[3], g_sArms, g_sLegs;
Mesh g_festPole, g_festFlag;
const char* FLAG_COLORS[6] = {"#dc2626", "#facc15", "#16a34a", "#2563eb", "#f8fafc", "#ea580c"};
DynBatch g_festPoles, g_festFlags;
GLuint g_lanternVao = 0, g_lanternVbo = 0;

const char* WINDOWS_FOR(const std::string& t, std::vector<Vec3>& out) {
  out.clear();
  if (t == "hunter_hut") out = {{-1.08f, 1.65f, 1.7f}, {1.08f, 1.65f, 1.7f}};
  else if (t == "carpenter_workshop") out = {{-1.82f, 1.65f, 1.6f}, {0.22f, 1.65f, 1.6f}, {2.4f, 2.2f, 0.1f}};
  else if (t == "herbalist_lodge") out = {{-0.96f, 1.65f, 1.6f}, {0.96f, 1.65f, 1.6f}};
  else if (t == "watchtower") out = {{0, 6.6f, 1.95f}, {0, 6.6f, -1.95f}, {1.95f, 6.6f, 0}, {-1.95f, 6.6f, 0}};
  else if (t == "bridge") out = {{-10.4f, 2.3f, 1.5f}, {10.4f, 2.3f, -1.5f}};
  else if (t == "campfire") out = {{0, 0.9f, 0}};
  return nullptr;
}

const char* FLAG_WIND_VS = R"(
in vec3 a_pos;
in vec4 a_col;
in vec4 a_i0;
in vec4 a_i1;
in vec4 a_i2;
in vec4 a_itint;
out vec3 v_col;
out vec3 v_wpos;
out float v_depth;
void main(){
  mat4 M = mat4(vec4(a_i0.x, a_i1.x, a_i2.x, 0.0), vec4(a_i0.y, a_i1.y, a_i2.y, 0.0), vec4(a_i0.z, a_i1.z, a_i2.z, 0.0), vec4(a_i0.w, a_i1.w, a_i2.w, 1.0));
  vec3 p = a_pos;
  float k = -p.y / 0.42;
  vec2 o = M[3].xz;
  vec3 wd = transpose(mat3(M)) * vec3(u_windDir.x, 0.0, u_windDir.y);
  float ph = u_windT * 6.0 + o.x * 1.3 + o.y * 1.1;
  p += (wd * 0.28 + vec3(0.0, 0.0, sin(ph) * 0.14)) * k * u_windS;
  vec4 wp = M * vec4(p, 1.0);
  v_wpos = wp.xyz;
  v_col = srgbToLinear(a_itint.rgb);
  vec4 vp = u_view * wp;
  v_depth = -vp.z;
  gl_Position = u_proj * vp;
}
)";
const char* FLAG_FS = R"(
in vec3 v_col;
in vec3 v_wpos;
in float v_depth;
out vec4 fragColor;
void main(){
  vec3 c = outColor(v_col);
  c = mix(c, fogTint(v_wpos), fogObject(v_wpos, v_depth));
  fragColor = vec4(c, 1.0);
}
)";
const char* LANTERN_VS = R"(
in vec3 a_pos;
uniform float u_on;
out float v_a;
void main(){
  vec3 sd = a_pos;
  float t = fract(sd.x + u_clock * (0.02 + sd.y * 0.015));
  float ang = sd.z * 6.2832 + t * 1.5;
  float r = 3.0 + sd.y * 9.0 + t * 12.0;
  vec3 p = vec3(cos(ang) * r, 2.0 + t * 55.0, sin(ang) * r);
  vec4 mv = u_view * vec4(p, 1.0);
  gl_Position = u_proj * mv;
  v_a = smoothstep(0.0, 0.05, t) * (1.0 - smoothstep(0.7, 1.0, t)) * u_on;
  gl_PointSize = clamp(0.55 * u_viewH / max(-mv.z, 0.1), 1.5, 36.0);
}
)";
const char* LANTERN_FS = R"(
in float v_a;
out vec4 fragColor;
void main(){
  vec2 q = gl_PointCoord - 0.5;
  float d = length(q * vec2(1.3, 1.0)) * 2.0;
  if (d > 1.0) discard;
  float core = 1.0 - smoothstep(0.0, 0.55, d);
  vec3 c = mix(vec3(1.0, 0.45, 0.1), vec3(1.0, 0.9, 0.55), core);
  fragColor = vec4(c * v_a * (0.35 + 0.65 * core), 1.0);
}
)";

}  // namespace

float structureRadius(StructureKind kind) {
  const float* b = BOX[kind];
  return std::max({hypot2(b[0], b[2]), hypot2(b[1], b[3]), hypot2(b[0], b[3]), hypot2(b[1], b[2]), 0.6f});
}

SpotCheck checkStructureSpot(StructureKind kind, float x, float z, float rot, int ignoreUid) {
  const KindInfo& info = KINDS[kind];
  float r = structureRadius(kind);
  float probe = std::min(r, 4.0f);
  float samples[5];
  float off[5][2] = {{0, 0}, {probe, 0}, {-probe, 0}, {0, probe}, {0, -probe}};
  float mn = 1e9f, mx = -1e9f;
  for (int i = 0; i < 5; i++) {
    samples[i] = world::getTerrainHeight(x + off[i][0], z + off[i][1]);
    mn = std::min(mn, samples[i]);
    mx = std::max(mx, samples[i]);
  }
  float y = info.group == KG_DECOR ? world::getTerrainHeight(x, z) : mn;
  auto res = [&](const std::string& reason) { return SpotCheck{reason.empty(), reason, x, y, z, rot}; };
  if (hypot2(x, z) > settlementLimit()) return res("Too far from the village edge");
  if (hypot2(x - 6.0f, z + 5.0f) < structureRadius(kind) + 1.4f) return res("Too close to the notice board");
  if (world::riverMask(x, z) > 0.02f || y < -0.3f || world::lakeAt(x, z, 2) >= 0) return res("Too close to the water");
  if (info.group != KG_DECOR && mx - y > 1.6f) return res("Ground is too steep");
  bool isDecor = info.group == KG_DECOR;
  for (auto& b : A.buildings) {
    if (!b.built || b.type == "bridge") continue;
    float need = (isDecor ? 0.6f : r * 0.8f) + footprint(b.type);
    if (hypot2(b.pos[0] - x, b.pos[2] - z) < need) return res("Too close to " + b.name);
  }
  for (auto& s : A.structures) {
    if (s.uid == ignoreUid) continue;
    bool otherDecor = KINDS[s.kind].group == KG_DECOR;
    if (isDecor && otherDecor) continue;
    float need = isDecor || otherDecor ? (isDecor ? structureRadius(s.kind) * 0.75f : r * 0.75f) + 0.3f : (r + structureRadius(s.kind)) * 0.72f;
    if (hypot2(s.x - x, s.z - z) < need) return res(std::string("Too close to the ") + KINDS[s.kind].name);
  }
  return res("");
}

void resolveStructureCollision(float& px, float& pz, float radius) {
  // quick reject: skip the whole list when far outside the village's structure bounds (cached per frame)
  static unsigned fr = ~0u;
  static float bx0, bx1, bz0, bz1;
  if (fr != (unsigned)render::frameNo) {
    fr = (unsigned)render::frameNo;
    bx0 = bz0 = 1e9f; bx1 = bz1 = -1e9f;
    for (auto& s : A.structures) { bx0 = std::min(bx0, s.x); bx1 = std::max(bx1, s.x); bz0 = std::min(bz0, s.z); bz1 = std::max(bz1, s.z); }
  }
  if (px < bx0 - 10 || px > bx1 + 10 || pz < bz0 - 10 || pz > bz1 + 10) return;
  for (auto& s : A.structures) {
    const float* b = BOX[s.kind];
    if (b[1] - b[0] < 0.01f) continue;
    float dx = px - s.x, dz = pz - s.z;
    if (dx * dx + dz * dz > 100) continue;
    float c = std::cos(s.rot), sn = std::sin(s.rot);
    float lx = dx * c - dz * sn, lz = dx * sn + dz * c;
    float minX = b[0] - radius, maxX = b[1] + radius, minZ = b[2] - radius, maxZ = b[3] + radius;
    if (lx <= minX || lx >= maxX || lz <= minZ || lz >= maxZ) continue;
    float pens[4] = {lx - minX, maxX - lx, lz - minZ, maxZ - lz};
    float m = std::min({pens[0], pens[1], pens[2], pens[3]});
    float nx = lx, nz = lz;
    if (m == pens[0]) nx = minX;
    else if (m == pens[1]) nx = maxX;
    else if (m == pens[2]) nz = minZ;
    else nz = maxZ;
    px = s.x + nx * c + nz * sn;
    pz = s.z - nx * sn + nz * c;
  }
}

// is (x, z) under the roof of a home, hall, store or stall? (rotated footprint box, slightly shrunk)
bool roofAt(float x, float z) {
  for (auto& st : A.structures) {
    if (KINDS[st.kind].group == KG_DECOR || st.kind == SK_FENCE || st.kind == SK_PATH) continue;
    const float* b = BOX[st.kind];
    float dx = x - st.x, dz = z - st.z, c = std::cos(st.rot), sn = std::sin(st.rot);
    float lx = dx * c - dz * sn, lz = dx * sn + dz * c;
    if (lx > b[0] * 0.85f && lx < b[1] * 0.85f && lz > b[2] * 0.85f && lz < b[3] * 0.85f) return true;
  }
  return false;
}

float tierBonus() { return 1.0f + 0.08f * A.stage; }

StageInfo computeStage() {
  auto count = [](StructureKind k) { int n = 0; for (auto& s : A.structures) if (s.kind == k) n++; return n; };
  int homes = 0;
  std::set<int> decorKinds;
  for (auto& s : A.structures) {
    if (KINDS[s.kind].group == KG_HOME) homes++;
    if (KINDS[s.kind].group == KG_DECOR) decorKinds.insert(s.kind);
  }
  int bigHomes = count(SK_FAMILY_HOUSE) + count(SK_COMMUNAL_HOUSE);
  int stalls = count(SK_MARKET_STALL);
  int hall = count(SK_COMMUNITY_HALL);
  int workshops = 0;
  for (auto& b : A.buildings) if (b.built && b.type != "campfire" && b.type != "bridge") workshops++;
  auto fmt = [](int a, int b, const char* what) { return std::to_string(a) + "/" + std::to_string(b) + " " + what; };
  std::vector<std::pair<bool, std::string>> one = {
      {homes >= 3, fmt(homes, 3, "homes")}, {stalls >= 1, fmt(stalls, 1, "market stall")}, {workshops >= 1, fmt(workshops, 1, "workshop")}, {A.population >= 4, fmt(A.population, 4, "residents")}};
  std::vector<std::pair<bool, std::string>> two = {
      {homes >= 6, fmt(homes, 6, "homes")}, {bigHomes >= 1, fmt(bigHomes, 1, "family or communal house")}, {hall >= 1, fmt(hall, 1, "community hall")},
      {stalls >= 2, fmt(stalls, 2, "market stalls")}, {A.population >= 12, fmt(A.population, 12, "residents")}, {(int)decorKinds.size() >= 2, fmt((int)decorKinds.size(), 2, "kinds of decoration")}};
  auto all = [](const std::vector<std::pair<bool, std::string>>& v) { for (auto& c : v) if (!c.first) return false; return true; };
  auto missing = [](const std::vector<std::pair<bool, std::string>>& v) { std::vector<std::string> o; for (auto& c : v) if (!c.first) o.push_back(c.second); return o; };
  if (!all(one)) return {0, STAGE_NAMES[0], missing(one)};
  if (!all(two)) return {1, STAGE_NAMES[1], missing(two)};
  int stores = count(SK_FOOD_STORE), lamps = count(SK_LAMP_POST);
  int quests = 0;
  for (auto& q : A.quests) if (q.completed) quests++;
  bool tower = false;
  for (auto& b : A.buildings) if (b.built && b.type == "watchtower") tower = true;
  std::vector<std::pair<bool, std::string>> three = {
      {homes >= 10, fmt(homes, 10, "homes")}, {A.population >= 20, fmt(A.population, 20, "residents")}, {stalls >= 3, fmt(stalls, 3, "market stalls")},
      {stores >= 2, fmt(stores, 2, "food stores")}, {lamps >= 4, fmt(lamps, 4, "lamp posts")}, {workshops >= 3, fmt(workshops, 3, "workshops")}};
  if (!all(three)) return {2, STAGE_NAMES[2], missing(three)};
  std::vector<std::pair<bool, std::string>> four = {
      {homes >= 14, fmt(homes, 14, "homes")}, {A.population >= 30, fmt(A.population, 30, "residents")}, {hall >= 2, fmt(hall, 2, "community halls")},
      {tower, std::string(tower ? "1/1" : "0/1") + " watchtower"}, {quests >= 6, fmt(quests, 6, "chapters complete")}, {(int)decorKinds.size() >= 4, fmt((int)decorKinds.size(), 4, "kinds of decoration")}};
  if (!all(four)) return {3, STAGE_NAMES[3], missing(four)};
  return {4, STAGE_NAMES[4], {}};
}

namespace village {

void syncResidents() {
  while ((int)g_res.size() > A.population) g_res.pop_back();
  while ((int)g_res.size() < A.population) {
    float a = randf() * TAU;
    float x = std::cos(a) * 34, z = std::sin(a) * 34;
    Resident r;
    r.id = g_nextResident++;
    r.x = x; r.z = z; r.y = world::getTerrainHeight(x, z);
    r.yaw = 0; r.speed = 0; r.phase = randf() * 6;
    r.tx = x; r.tz = z;
    r.mode = RM_IDLE; r.goingHome = false; r.t = 0.5f;
    r.tint = hashTint(TINTS, 16, g_nextResident, 1);
    r.home = 0; r.working = false; r.dance = randf() * TAU;
    g_res.push_back(r);
  }
  assignHomes();
  assignJobs();
}

void resetResidents() { g_res.clear(); g_nextResident = 1; }

// names and traits come from the villager's id, so they stay the same after loading a save
static const char* RNAMES[24] = {"Asha", "Bhim", "Chandni", "Dorje", "Gauri", "Hari", "Ishaan", "Jamuna", "Kesang", "Lata", "Mohan", "Nima",
                                 "Parvati", "Ramesh", "Sonam", "Tara", "Uma", "Vikram", "Yangchen", "Zara", "Pema", "Kunal", "Dechen", "Meera"};
struct TraitDef { const char* name; const char* blurb; float work; };
static const TraitDef TRAITS[5] = {{"Hard worker", "gets 20% more done", 1.2f}, {"Early riser", "starts work at first light (+10%)", 1.1f},
                                   {"Easy-going", "works at their own pace (-15%)", 0.85f}, {"Cheerful", "lifts everyone's spirits", 1.0f}, {"Steady", "reliable, no surprises", 1.0f}};
static uint32_t rhash(int id, uint32_t salt) { uint32_t h = (uint32_t)id * 2654435761u ^ salt; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; return h; }
static const char* residentName(int id) { return RNAMES[rhash(id, 7) % 24]; }
static int residentTrait(int id) { return (int)(rhash(id, 91) % 5); }

float traitMult(int j) {
  float sum = 0;
  int n = 0;
  for (auto& r : g_res)
    if (r.job == JOB_KEYS[j] || r.autoJob == JOB_KEYS[j]) { sum += TRAITS[residentTrait(r.id)].work; n++; }
  return n ? sum / n : 1.0f;
}

std::string residentInfo(float x, float z, bool full) {
  const Resident* best = nullptr;
  float bd = 2.6f;
  for (auto& r : g_res) {
    if (r.mode == RM_INSIDE) continue;
    float d = hypot2(r.x - x, r.z - z);
    if (d < bd) { bd = d; best = &r; }
  }
  if (!best) return "";
  const Resident& r = *best;
  std::string nm = residentName(r.id);
  if (!full) return nm;
  const char* JOBN[4] = {"woodcutter", "quarry worker", "herbalist", "hunter"};
  std::string job = "helping wherever needed";
  for (int k = 0; k < 4; k++) if (r.job == JOB_KEYS[k] || r.autoJob == JOB_KEYS[k]) job = JOBN[k];
  std::string mood = r.hunger > 0.7f ? "hungry" : r.tired > 0.75f ? "tired" : r.lonely > 0.65f ? "lonely" : A.coldDays > 0 ? "cold" : A.hungerDays > 0 ? "worried about food" : "content";
  const TraitDef& t = TRAITS[residentTrait(r.id)];
  return nm + " — " + job + ", feeling " + mood + ". " + t.name + ": " + t.blurb + ".";
}

// villagers with no job set by the player pick whatever the village is shortest of (timber, slate, herbs, food)
void refreshAutoJobs() {
  const char* keys[4] = {"wood", "stone", "herb", "food"};
  int stock[4];
  for (int k = 0; k < 4; k++) { auto it = A.inv.find(keys[k]); stock[k] = it == A.inv.end() ? 0 : it->second; }
  int planned[4] = {0, 0, 0, 0};
  for (auto& r : g_res) {
    if (!r.autoJob.empty() && r.job == r.autoJob) r.job.clear();  // re-decide auto jobs each tick
    r.autoJob.clear();
    if (!r.job.empty()) continue;
    int best = 0; float bs = 1e9f;
    for (int k = 0; k < 4; k++) { float sc = stock[k] + planned[k] * 15.0f; if (sc < bs) { bs = sc; best = k; } }
    planned[best]++;
    r.autoJob = JOB_KEYS[best];
    r.job = r.autoJob;
  }
}
int autoJobCount(int j) { int n = 0; for (auto& r : g_res) if (r.autoJob == JOB_KEYS[j]) n++; return n; }

void assignJobs() {
  std::vector<std::string> list;
  for (int k = 0; k < 4; k++)
    for (int i = 0; i < A.jobs[k]; i++) list.push_back(JOB_KEYS[k]);
  for (size_t i = 0; i < g_res.size(); i++) g_res[i].job = i < list.size() ? list[i] : "";
  refreshAutoJobs();
}

int residentCount() { return (int)g_res.size(); }
void snapshotResidents() { for (auto& r : g_res) { r.px = r.x; r.py = r.y; r.pz = r.z; r.pyaw = r.yaw; r.pphase = r.phase; r.pv = true; } }
void setResidentAlpha(float a) { g_ralpha = clampf(a, 0, 1); }
int nearPositions(float x, float z, float r, float* out, int maxN) {
  int n = 0;
  for (auto& v : g_res) {
    if (n >= maxN) break;
    if (v.mode == RM_INSIDE) continue;
    float dx = v.x - x, dz = v.z - z;
    if (dx * dx + dz * dz < r * r) { out[n * 3] = v.x; out[n * 3 + 1] = v.z; out[n * 3 + 2] = 0.8f; n++; }
  }
  return n;
}

void updateResidents(float dt) {
  bool night = E.daylight < 0.25f;
  bool fest = E.festival.t > 0;
  if (fest) E.festival.t = std::max(0.0f, E.festival.t - dt);
  const Vec3& pp = E.player.pos;
  for (size_t ri = 0; ri < g_res.size(); ri++) {
    Resident& r = g_res[ri];
    // far from the player: think 4x per second instead of every frame (same result, less CPU)
    float pd2 = (r.x - pp.x) * (r.x - pp.x) + (r.z - pp.z) * (r.z - pp.z);
    float rdt = dt;
    if (pd2 > 140 * 140) { r.slow += dt; if (r.slow < 0.25f) continue; rdt = r.slow; r.slow = 0; }
    float dt = rdt;
    r.talkT = std::max(0.0f, r.talkT - dt);
    // needs drift over time: hunger always, tiredness while working, loneliness while alone
    r.hunger = std::min(1.0f, r.hunger + dt / 420.0f);
    r.tired = r.mode == RM_INSIDE ? std::max(0.0f, r.tired - dt / 60.0f) : std::min(1.0f, r.tired + dt / (r.working ? 300.0f : 700.0f));
    r.lonely = r.talkT > 0 ? std::max(0.0f, r.lonely - dt / 20.0f) : std::min(1.0f, r.lonely + dt / 500.0f);
    r.lookT = std::max(0.0f, r.lookT - dt);
    if (r.mode != RM_INSIDE && !fest) {
      // greet the player: stop briefly and turn to look when they walk up close
      if (pd2 < 3.5f * 3.5f && !r.working && r.mode == RM_WALK && r.lookT <= 0 && randf() < dt * 1.5f) { r.mode = RM_IDLE; r.t = 2.5f + randf() * 2; }
      if (pd2 < 6 * 6) { r.lookT = 0.5f; r.lookX = pp.x; r.lookZ = pp.z; }
      // chat: two idle villagers close together face each other and talk for a while
      if (r.mode == RM_IDLE && !r.working && r.talkT <= 0 && randf() < dt * 0.4f) {
        for (size_t oi = 0; oi < g_res.size(); oi++) {
          Resident& o = g_res[oi];
          if (oi == ri || o.mode != RM_IDLE || o.working || o.talkT > 0) continue;
          if (hypot2(o.x - r.x, o.z - r.z) > 3.2f) continue;
          float T = 6 + randf() * 8;
          r.talkT = o.talkT = T; r.t = std::max(r.t, T); o.t = std::max(o.t, T);
          r.talkWith = (int)oi; o.talkWith = (int)ri;
          break;
        }
      }
      if (r.talkT > 0 && r.talkWith >= 0 && r.talkWith < (int)g_res.size()) {
        Resident& o = g_res[r.talkWith];
        r.yaw = turnToward(r.yaw, std::atan2(o.x - r.x, o.z - r.z), dt * 3);
        if (pd2 >= 6 * 6) { r.lookT = 0.3f; r.lookX = o.x; r.lookZ = o.z; }
      }
    }
    if (r.mode == RM_INSIDE) {
      r.t -= dt;
      if ((!night || fest) && !E.raid.active && r.t <= 0) { r.mode = RM_IDLE; r.t = 1; }
      continue;
    }
    if (fest && r.mode == RM_IDLE && hypot2(r.x, r.z) < 8) {
      r.dance += dt * 0.45f;
      float R = 4.6f + (r.id % 3) * 1.1f;
      float tx = std::cos(r.dance) * R, tz = std::sin(r.dance) * R;
      r.yaw = turnToward(r.yaw, std::atan2(tx - r.x, tz - r.z), dt * 5);
      r.speed = 0.9f;
      r.x += (tx - r.x) * std::min(1.0f, dt * 2);
      r.z += (tz - r.z) * std::min(1.0f, dt * 2);
      r.y = world::getTerrainHeight(r.x, r.z) + std::fabs(std::sin(E.clock * 6 + r.id)) * 0.12f;
      r.phase += dt * 4;
      continue;
    }
    if (r.mode == RM_IDLE) {
      r.speed = std::max(0.0f, r.speed - dt * 4);
      r.t -= dt;
      if (r.working && r.speed < 0.1f) r.phase += dt * 5;
      if (r.t <= 0 || (E.raid.active && !r.goingHome) || (fest && hypot2(r.x, r.z) > 8)) { pickTarget(r, night); if (!fest) planRoute(r); r.mode = RM_WALK; r.t = 40 + r.path.size() * 3.0f; }
    } else {
      // current steering target: next road waypoint, or the final destination
      float gx = r.tx, gz = r.tz;
      if (r.pathI < r.path.size()) {
        gx = r.path[r.pathI][0]; gz = r.path[r.pathI][1];
        if (hypot2(gx - r.x, gz - r.z) < 1.5f) { r.pathI++; continue; }
      }
      float dx = gx - r.x, dz = gz - r.z;
      if (r.pathI < r.path.size()) { r.speed += (1.25f - r.speed) * std::min(1.0f, dt * 3); r.yaw = turnToward(r.yaw, std::atan2(dx, dz), dt * 3.5f); r.t -= dt; goto moveStep; }
      dx = r.tx - r.x; dz = r.tz - r.z;
      float d = hypot2(dx, dz);
      r.speed += (1.25f - r.speed) * std::min(1.0f, dt * 3);
      r.yaw = turnToward(r.yaw, std::atan2(dx, dz), dt * 3.5f);
      r.t -= dt;
      if (d < 0.6f || r.t <= 0) {
        if (r.goingHome && night) { r.mode = RM_INSIDE; r.t = 5 + randf() * 20; }
        else if (r.goingHome && !E.raid.active && sky::rainAmount() < 0.4f) { r.mode = RM_INSIDE; r.t = 15 + randf() * 25; }  // daytime rest
        else if (r.goingHome && E.raid.active) { r.mode = RM_INSIDE; r.t = 20; }
        else { r.mode = RM_IDLE; r.t = r.working ? 14 + randf() * 16 : 4 + randf() * 9; if (r.working) r.deliver = true; else r.carry = -1; }  // haul dropped at the centre
      }
    }
  moveStep:
    r.x += std::sin(r.yaw) * r.speed * dt;
    r.z += std::cos(r.yaw) * r.speed * dt;
    // keep personal space: walkers sidestep each other instead of overlapping
    if (r.speed > 0.2f) {
      for (auto& o : g_res) {
        if (&o == &r || o.mode == RM_INSIDE) continue;
        float ox = r.x - o.x, oz = r.z - o.z, d2 = ox * ox + oz * oz;
        if (d2 < 0.8f * 0.8f && d2 > 1e-6f) { float d = std::sqrt(d2), push = (0.8f - d) * 0.5f; r.x += ox / d * push; r.z += oz / d * push; }
      }
    }
    // stuck against a wall or tree: give up on this target and pick another
    if (r.mode == RM_WALK) {
      r.stuckT += dt;
      if (r.stuckT > 1.5f) {
        if (hypot2(r.x - r.lastX, r.z - r.lastZ) < 0.4f) { r.mode = RM_IDLE; r.t = 0.3f; }
        r.stuckT = 0; r.lastX = r.x; r.lastZ = r.z;
      }
    }
    forest::resolveTreeCollision(r.x, r.z, 0.3f);
    resolveBuildingCollision(r.x, r.z, 0.3f);
    r.y = world::getTerrainHeight(r.x, r.z);
    r.phase += dt * TAU * (r.speed > 0.05f ? 0.85f + 0.3f * r.speed : 0.0f);  // stride cadence matched to speed (no foot sliding)
  }
}

void structuresChanged() { g_structDirty = true; }

void initRender() {
  const ResidentKit& K = people::residentKit();
  for (int v = 0; v < 3; v++) {
    g_torsos[v].init(&K.torsos[v]);
    g_heads[v].init(&K.heads[v]);
    g_sTorsos[v].init(&K.torsos[v]);
    g_sHeads[v].init(&K.heads[v]);
  }
  g_arms.init(&K.arm);
  g_legs.init(&K.leg);
  g_sArms.init(&K.arm);
  g_sLegs.init(&K.leg);
  Shape pole;
  pole.add(translated(cylinderGeo(0.06f, 0.08f, 4, 6), {0, 2, 0}), "#6b4426");
  g_festPole = uploadMesh(pole.build(false));
  Geo tri;
  tri.pos = {{-0.22f, 0, 0}, {0.22f, 0, 0}, {0, -0.42f, 0}};
  tri.nrm = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  tri.idx = {0, 1, 2};
  Shape flag;
  flag.add(tri, "#ffffff");
  g_festFlag = uploadMesh(flag.build(false));
  g_festPoles.init(&g_festPole);
  g_festFlags.init(&g_festFlag);
  const int POLES = 10;
  const float R = 8.5f;
  std::vector<Vec3> tops;
  for (int i = 0; i < POLES; i++) {
    float a = ((float)i / POLES) * TAU;
    float x = std::cos(a) * R, z = std::sin(a) * R;
    float y = world::getTerrainHeight(x, z);
    g_festPoles.add(Mat4::translate({x, y, z}));
    tops.push_back({x, y + 3.9f, z});
  }
  int k = 0;
  for (int i = 0; i < POLES; i++) {
    Vec3 A0 = tops[i], B0 = tops[(i + 1) % POLES];
    float yaw = std::atan2(B0.x - A0.x, B0.z - A0.z) + PI / 2;
    for (int j = 0; j < 7; j++) {
      float t = (j + 0.5f) / 7;
      Vec3 v = Vec3::lerp(A0, B0, t);
      v.y -= std::sin(t * PI) * 0.7f;
      g_festFlags.add(Mat4::compose(v, Quat::axisAngle({0, 1, 0}, yaw), {1, 1, 1}), rgbHex(FLAG_COLORS[k % 6]));
      k++;
    }
  }
  g_festPoles.upload();
  g_festFlags.upload();
  std::vector<float> seeds(60 * 3);
  Rng rng(2468);
  for (auto& s : seeds) s = rng.next();
  glGenVertexArrays(1, &g_lanternVao);
  glBindVertexArray(g_lanternVao);
  glGenBuffers(1, &g_lanternVbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_lanternVbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(seeds.size() * 4), seeds.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
  glBindVertexArray(0);
}

static Mesh g_merged;
static void rebuildStructures() {
  sky::invalidateStaticShadow();
  g_structDirty = false;
  for (auto& b : g_structBatches) b->batch.destroy();
  g_structBatches.clear();
  std::vector<std::pair<StructureKind, Look>> keep;
  for (auto& s : A.structures) keep.push_back({s.kind, s.look});
  if (E.world.placeKind != SK_COUNT && E.world.hasPlaceLook) keep.push_back({E.world.placeKind, E.world.placeLook});
  pruneStructureModels(keep);
  std::map<const Mesh*, StructBatch*> map;
  g_staticLights.clear();
  for (auto& s : A.structures) {
    const BuiltModel& m = structureModel(s.kind, s.look);
    StructBatch*& sb = map[m.mesh];
    if (!sb) {
      g_structBatches.push_back(std::make_unique<StructBatch>());
      sb = g_structBatches.back().get();
      sb->mesh = m.mesh;
      sb->batch.init(m.mesh);
    }
    sb->batch.add(Mat4::compose({s.x, s.y, s.z}, Quat::axisAngle({0, 1, 0}, s.rot), {1, 1, 1}));
    for (auto& l : m.lights) {
      Vec3 w;
      toWorld(s, l.x, l.y, l.z, w);
      g_staticLights.push_back(w);
    }
  }
  for (auto& b : g_structBatches) b->batch.upload();
  // merge every placed structure into one static mesh: one draw call for the whole village instead of one per style
  g_merged.destroy();
  MeshData md;
  md.fmt = VFmt::P3C4;
  for (auto& s : A.structures) {
    const BuiltModel& m = structureModel(s.kind, s.look);
    if (!m.data || m.data->fmt != VFmt::P3C4) { md.verts.clear(); break; }
    const MeshData& src = *m.data;
    MeshData t;  // this structure in world space
    t.fmt = VFmt::P3C4;
    t.verts = src.verts;
    t.idx = src.idx;
    t.vertexCount = src.vertexCount;
    float c = std::cos(s.rot), sn = std::sin(s.rot);
    for (int i = 0; i < t.vertexCount; i++) {
      VtxP3C4* v = (VtxP3C4*)t.verts.data() + i;
      float lx = v->p[0], lz = v->p[2];
      v->p[0] = s.x + lx * c + lz * sn;
      v->p[1] += s.y;
      v->p[2] = s.z - lx * sn + lz * c;
      md.bmin = {std::min(md.bmin.x, v->p[0]), std::min(md.bmin.y, v->p[1]), std::min(md.bmin.z, v->p[2])};
      md.bmax = {std::max(md.bmax.x, v->p[0]), std::max(md.bmax.y, v->p[1]), std::max(md.bmax.z, v->p[2])};
    }
    {
      // baked AO per building: shade under eaves, inside porches and where walls meet the ground
      AOGrid g;
      g.add(t);
      g.groundAt = [](float x, float z) { return world::getTerrainHeight(x, z) - 0.05f; };
      g.build(96, 0.12f);
      g.maxDist = 2.5f;
      g.applyColour(t, 0.75f);
    }
    uint32_t base = (uint32_t)md.vertexCount;
    md.verts.insert(md.verts.end(), t.verts.begin(), t.verts.end());
    for (uint32_t ix : t.idx) md.idx.push_back(ix + base);
    md.vertexCount += t.vertexCount;
  }
  if (!md.verts.empty() && md.vertexCount > 0) g_merged = uploadMesh(md);
}

void drawStructures() {
  if (g_structDirty) rebuildStructures();
  glEnable(GL_CULL_FACE);
  if (g_merged.valid()) {
    Shader& s = render::object(VFmt::P3C4, IM_NONE, SeasonKind::Build, OF_WIND);
    render::setModel(s, Mat4());
    g_merged.draw();
    return;
  }
  render::object(VFmt::P3C4, IM_FULL, SeasonKind::Build, OF_WIND);
  for (auto& b : g_structBatches) b->batch.draw();
}

static void fillResidentsInner(bool shadow);
static void fillResidents(bool shadow) {
  // draw residents between their last two simulated steps (smooth motion at any frame rate)
  struct S { float x, y, z, yaw, phase; };
  static std::vector<S> sv;
  sv.resize(g_res.size());
  for (size_t i = 0; i < g_res.size(); i++) {
    Resident& r = g_res[i];
    sv[i] = {r.x, r.y, r.z, r.yaw, r.phase};
    if (!r.pv || std::fabs(r.x - r.px) + std::fabs(r.z - r.pz) > 4) continue;
    float t = g_ralpha, dy = r.yaw - r.pyaw;
    while (dy > PI) dy -= TAU;
    while (dy < -PI) dy += TAU;
    r.x = r.px + (r.x - r.px) * t; r.y = r.py + (r.y - r.py) * t; r.z = r.pz + (r.z - r.pz) * t; r.yaw = r.pyaw + dy * t; r.phase = r.pphase + (r.phase - r.pphase) * t;
  }
  fillResidentsInner(shadow);
  for (size_t i = 0; i < g_res.size(); i++) { Resident& r = g_res[i]; r.x = sv[i].x; r.y = sv[i].y; r.z = sv[i].z; r.yaw = sv[i].yaw; r.phase = sv[i].phase; }
}
static float g_headDt = 0;
// MakeHuman villagers queued by fillResidentsInner and drawn by drawResidents / drawShadow
struct MhItem { Mat4 base; mh::Look look; mh::Pose pose; float dist; };
static std::vector<MhItem> g_mhMain, g_mhShadow;
struct Haul { Mat4 m; int kind; };
static std::vector<Haul> g_hauls;  // doko baskets on the backs of villagers bringing goods home
static Mesh g_dokoMesh[4];
static std::vector<Vec3> g_heldLanterns;  // villagers' hand lanterns after dark (top of the handle, world space)
static Mesh g_handLantern;
static std::vector<Vec3> g_npcLanterns;
void setNpcLanterns(const std::vector<Vec3>& h) { g_npcLanterns = h; }
static float g_dbgX = 0, g_dbgZ = 0;
static int g_dbgN = 0;

static Vec3 lin(uint32_t c) {
  auto f = [](float v) { v /= 255.0f; return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
  return {f((float)(c & 255)), f((float)((c >> 8) & 255)), f((float)((c >> 16) & 255))};
}
static void fillResidentsInner(bool shadow) {
  if (!shadow) { static float last = -1; g_headDt = last < 0 ? 0 : clampf(E.clock - last, 0, 0.1f); last = E.clock; }
  const Vec3& p = E.player.pos;
  DynBatch* T = shadow ? g_sTorsos : g_torsos;
  DynBatch* H = shadow ? g_sHeads : g_heads;
  DynBatch& AR = shadow ? g_sArms : g_arms;
  DynBatch& LG = shadow ? g_sLegs : g_legs;
  for (int v = 0; v < 3; v++) { T[v].clear(); H[v].clear(); }
  AR.clear();
  LG.clear();
  const bool useMh = mh::ready();
  if (!shadow) { g_heldLanterns = g_npcLanterns; g_hauls.clear(); }
  std::vector<MhItem>& MH = shadow ? g_mhShadow : g_mhMain;
  MH.clear();
  for (auto& r : g_res) {
    if (r.mode == RM_INSIDE) continue;
    float dx = r.x - p.x, dz = r.z - p.z;
    if (dx * dx + dz * dz > 95 * 95) continue;
    if (shadow && dx * dx + dz * dz > 46 * 46) continue;
    if (!shadow && !render::cam.sphere({r.x, r.y + 1, r.z}, 1.5f)) continue;
    if (!shadow && dx * dx + dz * dz > 40 * 40) {
      // hidden behind a hill or ridge? (ray-march the height field; cached briefly)
      if (E.clock - r.occT > 0.4f) {
        r.occT = E.clock;
        const Vec3& c = render::cam.pos;
        float L = std::sqrt(dx * dx + dz * dz);
        int n = (int)clampf(L / 6, 6, 24);
        r.hidden = true;
        for (float hy : {1.7f, 0.4f}) {
          bool blocked = false;
          for (int k = 1; k < n && !blocked; k++) {
            float t = (float)k / n;
            float x = c.x + (r.x - c.x) * t, z = c.z + (r.z - c.z) * t, y = c.y + (r.y + hy - c.y) * t;
            if (world::getTerrainHeight(x, z) > y + 0.5f) blocked = true;
          }
          if (!blocked) { r.hidden = false; break; }
        }
      }
      if (r.hidden) continue;
    }
    int v = r.id % 3;
    if (T[v].items.size() >= 48) continue;
    float bob = std::fabs(std::sin(r.phase)) * std::min(0.05f, r.speed * 0.02f);
    float sc = (v == 2 ? 0.95f : v == 1 ? 0.94f : 1.0f) * BODY_SCALE;
    Mat4 base = Mat4::compose({r.x, r.y + bob, r.z}, Quat::axisAngle({0, 1, 0}, r.yaw), {sc, sc, sc});
    if (!useMh) T[v].add(base, r.tint);
    float look = r.speed < 0.1f ? std::sin(E.clock * 0.7f + r.id) * 0.5f : 0;
    if (r.lookT > 0) { float rel = std::atan2(r.lookX - r.x, r.lookZ - r.z) - r.yaw; while (rel > PI) rel -= TAU; while (rel < -PI) rel += TAU; look = clampf(rel, -1.0f, 1.0f); }
    // ease the head toward where they want to look, same pace as the story characters (no snapping)
    if (!shadow) r.headYaw += (look - r.headYaw) * std::min(1.0f, g_headDt * 4);
    look = r.headYaw;
    if (!useMh) H[v].add(base * Mat4::compose({0, PIV_HEAD.y, 0}, Quat::axisAngle({0, 1, 0}, look), {1, 1, 1}), hashTint(CAPS, 10, r.id, 2));
    if (!useMh && r.carry >= 0 && r.speed > 0.1f && T[v].items.size() < 48) {
      // haul across the shoulders: log bundle, stone sack, herb basket, or a deer carcass
      static const uint32_t HAUL[4] = {rgbHex("#6b4423"), rgbHex("#8a8580"), rgbHex("#4d7c0f"), rgbHex("#8b6a4a")};
      Vec3 sz = r.carry == 3 ? Vec3{0.75f, 0.45f, 1.1f} : r.carry == 0 ? Vec3{1.5f, 0.3f, 0.4f} : Vec3{0.6f, 0.4f, 0.55f};
      T[v].add(base * Mat4::compose({0, PIV_ARM.y - 0.62f, -0.22f}, Quat::axisAngle({0, 0, 1}, 0), sz), HAUL[r.carry]);
    }
    float swing = std::sin(r.phase) * std::min(0.7f, r.speed * 0.35f);
    bool chop = r.working && r.mode == RM_IDLE && r.speed < 0.1f;
    float idle = chop ? -1.2f + std::sin(E.clock * 5 + r.id) * 0.9f : r.speed < 0.1f ? std::sin(E.clock * 1.6f + r.id) * 0.05f : 0;
    bool talking = r.talkT > 0 && r.speed < 0.1f;
    if (useMh) {
      // MakeHuman body: same gait, work, festival dance and gestures, driven through the skeleton
      MhItem it;
      it.look.kind = v == 1 ? mh::WOMAN : v == 2 ? mh::ELDER : mh::MAN;
      float ms = v == 1 ? 0.95f : v == 2 ? 0.95f : 0.98f;
      it.base = Mat4::compose({r.x, r.y, r.z}, Quat::axisAngle({0, 1, 0}, r.yaw), {ms, ms, ms});
      it.look.tunic = lin(r.tint);
      it.look.skin = 0.84f + 0.03f * (float)((r.id * 37) % 11);
      it.look.trousers = lin(hashTint(PANTS, 8, r.id, 3));
      it.look.cap = lin(hashTint(CAPS, 10, r.id, 2));
      it.look.sash = lin(hashTint(CAPS, 10, r.id, 5));
      it.look.shoes = lin(rgbHex(v == 1 ? "#5a2e1a" : "#3b2a1c"));
      if (v == 1) std::swap(it.look.cap, it.look.sash);
      it.pose.walkPhase = r.phase;
      it.pose.walk = std::min(1.0f, r.speed * (v == 1 ? 0.45f : 0.55f));
      it.pose.speed = r.speed;
      it.pose.headYaw = look;
      it.pose.lean = r.carry >= 0 && r.speed > 0.1f ? 0.15f : 0.0f;
      bool dance = !chop && E.festival.t > 0 && hypot2(r.x, r.z) < 8;
      if (chop) {
        // the work pose fits the job: axe / pick swings at wood and stone, stooped gathering at herbs, hands on the counter otherwise
        float w = std::sin(E.clock * 3.2f + r.id);
        if (r.nodeId >= 0 && (r.job == "woodcutter" || r.job == "quarry")) {
          float sw = 0.5f + 0.5f * w;  // raise slowly, strike
          it.pose.armL = it.pose.armR = 0.5f + 0.9f * sw * sw; it.pose.elbowL = it.pose.elbowR = 0.35f; it.pose.lean = 0.12f + 0.12f * (1 - sw);
        } else if (r.nodeId >= 0) {
          it.pose.lean = 0.55f; it.pose.armL = 0.55f + 0.15f * w; it.pose.armR = 0.6f - 0.15f * w; it.pose.elbowL = it.pose.elbowR = 0.3f; it.pose.headPitch = 0.2f;
        } else {
          it.pose.armL = it.pose.armR = 0.3f; it.pose.elbowL = it.pose.elbowR = 0.55f + 0.05f * w;
        }
      }
      else if (dance) { it.pose.armL = 2.4f - std::sin(E.clock * 7 + r.id) * 0.4f; it.pose.armR = 2.4f - std::sin(E.clock * 7 + r.id + 1) * 0.4f; it.pose.walk = std::max(it.pose.walk, 0.3f); it.pose.speed = -1; it.pose.walkPhase = E.clock * 6 + r.id; }
      else if (r.speed < 0.1f) { it.pose.armL = it.pose.armR = std::sin(E.clock * 1.6f + r.id) * 0.04f; }
      if (talking) { float g = 0.22f + std::sin(E.clock * 2.1f + r.id) * 0.1f; if (r.id & 1) it.pose.armR = g, it.pose.elbowR = 0.45f; else it.pose.armL = g, it.pose.elbowL = 0.45f; }  // small, natural hand gesture
      if (r.carry >= 0 && r.speed > 0.1f) {
        // doko basket on the back: both hands hold the shoulder straps at the chest, arms barely swing
        it.pose.armL = it.pose.armR = 0.1f; it.pose.elbowL = it.pose.elbowR = 1.3f; it.pose.armSwing = 0.25f;
        if (!shadow && g_hauls.size() < 24) {
          Vec3 sp = mh::jointWorld(it.base, it.pose, "spine_03");
          Mat4 m = Mat4::compose(sp, Quat::axisAngle({0, 1, 0}, r.yaw) * Quat::axisAngle({1, 0, 0}, it.pose.lean + 0.05f), {ms, ms, ms});
          g_hauls.push_back({m, r.carry});
        }
      }
      it.dist = std::sqrt(dx * dx + dz * dz);
      // after dark everyone out and about carries a hand lantern in the right hand, held a little forward
      if (E.daylight < 0.3f && !chop && !dance && r.carry < 0 && it.dist < 80) {
        it.pose.armR = 0.3f; it.pose.elbowR = 0.55f;
        if (!shadow && g_heldLanterns.size() < 14) g_heldLanterns.push_back(mh::jointWorld(it.base, it.pose, "hand_r"));
      }
      MH.push_back(it);
      continue;
    }
    for (int k = 0; k < 2; k++) {
      float side = k == 0 ? 1.0f : -1.0f;
      float ang = side * -swing + (!chop && E.festival.t > 0 && hypot2(r.x, r.z) < 8 ? -2.4f + std::sin(E.clock * 7 + r.id + k) * 0.4f : idle);
      if (talking && k == (r.id & 1)) ang = -0.5f + std::sin(E.clock * 3.1f + r.id) * 0.35f;  // gesturing hand
      AR.add(base * Mat4::compose({side * PIV_ARM.x, PIV_ARM.y, 0}, Quat::axisAngle({1, 0, 0}, ang), {1, 1, 1}), r.tint);
      LG.add(base * Mat4::compose({side * PIV_LEG.x, PIV_LEG.y, 0}, Quat::axisAngle({1, 0, 0}, side * swing), {1, 1, 1}), hashTint(PANTS, 8, r.id, 3));
    }
  }
  if (g_dbgN > 0 && mh::ready()) {
    static const char* tun[6] = {"#8b5a2b", "#9f1239", "#57534e", "#1e3a8a", "#166534", "#7c2d12"};
    for (int k = 0; k < g_dbgN; k++) {
      MhItem it;
      it.look.kind = (mh::Kind)(k % 3);
      float x = g_dbgX + (k - (g_dbgN - 1) * 0.5f) * 1.1f, z = g_dbgZ + (k & 1) * 0.6f;
      it.base = Mat4::compose({x, world::getTerrainHeight(x, z), z}, Quat::axisAngle({0, 1, 0}, (k - 2.5f) * 0.25f), {0.97f, 0.97f, 0.97f});
      it.look.tunic = lin(rgbHex(tun[k % 6]));
      it.look.skin = 0.84f + 0.06f * (float)(k % 5);
      it.look.trousers = lin(rgbHex(PANTS[k % 8]));
      it.look.cap = lin(rgbHex(CAPS[(k * 3) % 10]));
      it.look.sash = lin(rgbHex(CAPS[(k * 7 + 1) % 10]));
      it.look.shoes = lin(rgbHex("#3b2a1c"));
      it.pose.walkPhase = E.clock * 4 + k;
      it.pose.walk = k == 1 || k == 4 ? 0.8f : 0.0f;
      if (k == 1 || k == 4) {  // walkers seen from the side; the second one carries a doko basket of firewood
        it.pose.speed = 1.3f; it.pose.walkPhase = E.clock * TAU * (0.85f + 0.3f * 1.3f) + k;
        it.base = Mat4::compose({x, world::getTerrainHeight(x, z), z}, Quat::axisAngle({0, 1, 0}, k == 1 ? 0.0f : 2.3f), {0.97f, 0.97f, 0.97f});
        if (k == 4) {
          it.pose.lean = 0.15f; it.pose.armL = it.pose.armR = 0.1f; it.pose.elbowL = it.pose.elbowR = 1.3f; it.pose.armSwing = 0.25f;
          if (!shadow) g_hauls.push_back({Mat4::compose(mh::jointWorld(it.base, it.pose, "spine_03"), Quat::axisAngle({0, 1, 0}, 2.3f) * Quat::axisAngle({1, 0, 0}, 0.2f), {0.97f, 0.97f, 0.97f}), 0});
        }
      }
      if (k == 2) { it.pose.armR = 0.6f; it.pose.elbowR = 0.6f; }
      if (k == 5) { it.pose.armL = it.pose.armR = 1.3f; it.pose.elbowL = it.pose.elbowR = 0.3f; }
      it.pose.headYaw = (k - 2.5f) * -0.15f;
      it.dist = 5;
      it.look.skin = it.look.skin; MH.push_back(it);

    }
  }  for (int v = 0; v < 3; v++) { T[v].upload(); H[v].upload(); }
  AR.upload();
  LG.upload();

}

void debugGroup(float x, float z, int n) {
  g_dbgX = x; g_dbgZ = z; g_dbgN = n;  // drawn as a fixed lineup (the test save has no villagers)
  int k = 0;
  for (auto& r : g_res) {
    if (k >= n) break;
    float a = k * 1.3f;
    r.x = x + std::cos(a) * (1.2f + k * 0.35f); r.z = z + std::sin(a) * (1.2f + k * 0.35f);
    r.y = world::getTerrainHeight(r.x, r.z);
    r.px = r.x; r.pz = r.z; r.py = r.y;
    r.mode = RM_IDLE; r.speed = k % 2 ? 1.2f : 0.0f; r.yaw = a + PI;
    r.pyaw = r.yaw;
    r.t = 30; r.tx = r.x; r.tz = r.z; r.working = false; r.carry = -1;
    k++;
  }
}
void drawResidents() {
  fillResidents(false);
  glEnable(GL_CULL_FACE);
  render::object(VFmt::P3C4N4, IM_FULL, SeasonKind::None, OF_TINTWHITE | OF_SOFT);
  for (int v = 0; v < 3; v++) { g_torsos[v].draw(); g_heads[v].draw(); }
  g_arms.draw();
  g_legs.draw();
  if (!g_mhMain.empty()) {
    mh::begin(false);
    for (auto& it : g_mhMain) mh::draw(it.base, it.look, it.pose, it.dist, false);
    mh::end();
  }
  if (!g_heldLanterns.empty()) {
    if (!g_handLantern.valid()) {
      Shape l;  // tin hand lantern hanging from a short bail: base, four corner posts, glowing glass, cap and ring
      l.box("#2b2522", {0, -0.24f, 0}, {0.13f, 0.02f, 0.13f});
      for (int c = 0; c < 4; c++) l.box("#2b2522", {(c & 1) ? 0.055f : -0.055f, -0.175f, (c & 2) ? 0.055f : -0.055f}, {0.012f, 0.13f, 0.012f});
      l.box("#ffd98a", {0, -0.175f, 0}, {0.1f, 0.12f, 0.1f});
      l.cone("#2b2522", {0, -0.105f, 0}, 0.085f, 0.05f, 4, {0, 0.785f, 0});
      l.rod("#2b2522", {0, -0.06f, 0}, {0, 0.0f, 0}, 0.006f, 0.006f, 3);
      g_handLantern = uploadMesh(l.build(true));
    }
    Shader& s = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::None, OF_SOFT);
    for (auto& h : g_heldLanterns) { render::setModel(s, Mat4::compose(h, Quat(), {1, 1, 1})); g_handLantern.draw(); }
  }
  if (!g_hauls.empty()) {
    if (!g_dokoMesh[0].valid()) {
      // Himalayan doko: a conical woven bamboo back-basket (wide mouth, narrow foot) with a rim, wrap bands and
      // shoulder straps, filled with what the villager is bringing home: firewood, stone, herbs, or game
      for (int k = 0; k < 4; k++) {
        Shape d;
        Vec3 c{0, -0.12f, -0.24f};
        d.tube("#9a7a48", c, 0.2f, 0.12f, 0.58f, 10);
        d.tube("#6e5330", {c.x, c.y + 0.29f, c.z}, 0.215f, 0.215f, 0.04f, 10);   // rim
        d.tube("#7d6038", {c.x, c.y + 0.05f, c.z}, 0.185f, 0.18f, 0.03f, 10);    // wrap bands
        d.tube("#7d6038", {c.x, c.y - 0.15f, c.z}, 0.15f, 0.145f, 0.03f, 10);
        d.tube("#5c4428", {c.x, c.y - 0.3f, c.z}, 0.11f, 0.11f, 0.03f, 8);      // foot ring
        for (int sd = -1; sd <= 1; sd += 2) d.rod("#3f2e1e", {sd * 0.11f, 0.17f, -0.1f}, {sd * 0.1f, 0.2f, 0.03f}, 0.018f, 0.018f, 4);  // straps
        float top = c.y + 0.3f;
        if (k == 0) for (int i = 0; i < 6; i++) { float a = i * 1.05f, rr = 0.06f + 0.05f * (i & 1);
            d.rod(i & 1 ? "#6b4423" : "#7a5230", {c.x + std::cos(a) * rr, top - 0.25f, c.z + std::sin(a) * rr}, {c.x + std::cos(a) * rr * 1.3f, top + 0.3f, c.z + std::sin(a) * rr * 1.3f - 0.03f}, 0.035f, 0.03f, 5); }
        else if (k == 1) for (int i = 0; i < 4; i++) d.ball(i & 1 ? "#8a8580" : "#77726c", {c.x + (i - 1.5f) * 0.08f, top + 0.03f + 0.03f * (i & 1), c.z + 0.04f * ((i * 7) % 3 - 1)}, {0.09f, 0.07f, 0.09f});
        else if (k == 2) for (int i = 0; i < 5; i++) d.ball(i % 2 ? "#4d7c0f" : "#3f6a12", {c.x + std::cos(i * 1.3f) * 0.1f, top + 0.06f + 0.04f * (i % 3), c.z + std::sin(i * 1.3f) * 0.1f}, {0.11f, 0.09f, 0.11f});
        else { d.ball("#8b6a4a", {c.x, top + 0.05f, c.z}, {0.2f, 0.1f, 0.17f}); d.ball("#6e523a", {c.x + 0.13f, top + 0.12f, c.z + 0.05f}, {0.07f, 0.06f, 0.1f});
               for (int sd = -1; sd <= 1; sd += 2) d.rod("#5a4232", {c.x + sd * 0.1f, top + 0.05f, c.z - 0.05f}, {c.x + sd * 0.16f, top + 0.35f, c.z - 0.1f}, 0.02f, 0.012f, 4); }
        g_dokoMesh[k] = uploadMesh(d.build(false));
      }
    }
    Shader& s = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::None, OF_SOFT);
    for (auto& h : g_hauls) { render::setModel(s, h.m); g_dokoMesh[h.kind & 3].draw(); }
  }
}

void drawShadowStatic() {
  if (g_structDirty) rebuildStructures();
  if (g_merged.valid()) {
    Shader& s = render::object(VFmt::P3C4, IM_NONE, SeasonKind::Build, OF_WIND);
    render::setModel(s, Mat4());
    g_merged.draw();
    return;
  }
  render::object(VFmt::P3C4, IM_FULL, SeasonKind::Build, OF_WIND);
  for (auto& b : g_structBatches) b->batch.draw();
}

void drawShadow() {
  fillResidents(true);
  render::object(VFmt::P3C4N4, IM_FULL, SeasonKind::None, 0);
  for (int v = 0; v < 3; v++) { g_sTorsos[v].draw(); g_sHeads[v].draw(); }
  g_sArms.draw();
  g_sLegs.draw();
  if (!g_mhShadow.empty()) {
    mh::begin(true);
    for (auto& it : g_mhShadow) mh::draw(it.base, it.look, it.pose, it.dist, true);
    mh::end();
  }
}

// baked ground shade and firelight: hand the fixed occluders and lamps to the detail map (rebakes only on change)
static void feedDetailMap() {
  static std::vector<envmap::Stamp> occ;
  static std::vector<envmap::FireLight> lights;
  occ.clear();
  lights.clear();
  for (auto& s : A.structures) {
    const BuiltModel& m = structureModel(s.kind, s.look);
    if (!m.data) continue;
    const Vec3 &lo = m.data->bmin, &hi = m.data->bmax;
    float cx = (lo.x + hi.x) * 0.5f, cz = (lo.z + hi.z) * 0.5f, c = std::cos(s.rot), sn = std::sin(s.rot);
    float hx = (hi.x - lo.x) * 0.5f, hz = (hi.z - lo.z) * 0.5f;
    if (hx < 0.3f || hz < 0.3f || hi.y - lo.y < 0.8f) continue;  // fences and flat things cast no real contact shade
    occ.push_back({s.x + cx * c + cz * sn, s.z - cx * sn + cz * c, hx * 0.9f, hz * 0.9f, s.rot, 0.42f, false});
  }
  for (auto& l : g_staticLights) lights.push_back({l.x, l.y, l.z, 0.85f, 7.0f});
  static std::vector<Vec3> templeSpots = temple::lightSpots();
  for (auto& l : templeSpots) lights.push_back({l.x, l.y, l.z, 0.7f, 5.5f});
  for (auto& b : A.buildings)
    if (b.built && b.type == "campfire") lights.push_back({b.pos[0], b.pos[1] + 0.6f, b.pos[2], 1.0f, 10.0f});
  envmap::setDetailSources(occ, lights);
}

void addLights() {
  if (g_structDirty) rebuildStructures();
  feedDetailMap();
  const Vec3& p = E.player.pos;
  auto spot = [&](const Vec3& s, float size, float poolR) {
    // flame flicker: every light breathes on its own rhythm
    float ph = s.x * 1.7f + s.z * 2.3f;
    float fl = 1.0f;  // steady light (flicker removed)
    (void)ph;
    if (size > 0) fx::glow(s.x, s.y, s.z, size * fl);
    if (size > 0) fx::halo(s.x, s.y, s.z, size * 5.5f);  // the lamp lights the misty air around it
    if (poolR > 0 && hypot2(s.x - p.x, s.z - p.z) < 450) fx::pool(s.x, world::getTerrainHeight(s.x, s.z) + 0.06f, s.z, poolR * (0.94f + 0.06f * fl));
  };
  // lamps are lit at dusk and put out at dawn
  if (E.daylight < 0.55f) for (auto& l : g_staticLights) spot(l, 1.1f, 0.0f);  // ground pool is baked (envmap detail map)
  // seasonal festivals: Losar rings the hearth with butter lamps, the harvest festival hangs marigold-glow garlands
  if (int fk = events::festival()) {
    int n = fk == 1 ? 18 : 12;
    float R = fk == 1 ? 5.6f : 7.5f;
    for (int i = 0; i < n; i++) {
      float a = i * TAU / n, x = std::cos(a) * R, z = std::sin(a) * R;
      float y = world::getTerrainHeight(x, z) + (fk == 1 ? 0.25f : 2.2f + 0.3f * std::sin(a * 3));
      spot({x, y, z}, fk == 1 ? 0.55f : 0.7f, fk == 1 ? 1.4f : 0.0f);
    }
  }
  // villagers' hand lanterns: the light comes from the lantern they hold (flame glow, misty halo, warm pool on the ground)
  for (auto& h : g_heldLanterns) spot({h.x, h.y - 0.175f, h.z}, 0.42f, 3.6f);
  if (!mh::ready() && E.daylight < 0.25f && A.population >= 3 && !g_res.empty() && g_res[0].mode != RM_INSIDE)  // old models: the watchman's torch
    spot({g_res[0].x + std::sin(g_res[0].yaw) * 0.3f, g_res[0].y + 1.9f, g_res[0].z + std::cos(g_res[0].yaw) * 0.3f}, 0.6f, 4.5f);
  // hand lantern: a warm pool around the player after dark
  {
    static bool hinted = false;  // once per session, at the first dusk: tell the player about the lantern
    if (!hinted && E.playing && E.daylight < 0.3f && !E.lanternOn) { hinted = true; actions::notify("It's getting dark. Press L to light your lantern."); }
  }
  if (E.lanternOn && !E.photo) {  // the flame itself; the light it casts is real (u_lantern in the frame data)
    Vec3 f = render::cam.forward();
    Vec3 r = Vec3(-f.z, 0, f.x).normalized();
    Vec3 lp = render::cam.pos + Vec3(f.x, 0, f.z).normalized() * 0.45f - r * 0.3f + Vec3(0, -0.6f, 0);
    (void)lp;  // no visible flame sprite: held low it read as a glare on the ground; the light itself is real (u_lantern)
  }
  static std::vector<Vec3> templeSpots = temple::lightSpots();
  for (auto& l : templeSpots) spot(l, 1.3f, 0.0f);  // butter lamps: warm pools are baked into the detail map
  std::vector<Vec3> win;
  for (auto& b : A.buildings) {
    if (!b.built) continue;
    WINDOWS_FOR(b.type, win);
    float c = std::cos(b.rotY), sn = std::sin(b.rotY);
    for (auto& w : win) {
      bool fire = b.type == "campfire";
      Vec3 s(b.pos[0] + w.x * c + w.z * sn, b.pos[1] + w.y, b.pos[2] - w.x * sn + w.z * c);
      spot(s, fire ? 0 : 1.1f, fire ? 7 : 0);
      if (!fire) {
        // warm window light spilling out onto the ground in front of the wall
        Vec3 out(s.x - b.pos[0], 0, s.z - b.pos[2]);
        float L = std::sqrt(out.x * out.x + out.z * out.z);
        if (L > 0.01f) { out.x /= L; out.z /= L; }
        float wx = s.x + out.x * 1.6f, wz = s.z + out.z * 1.6f;
        if (hypot2(wx - p.x, wz - p.z) < 450) fx::pool(wx, world::getTerrainHeight(wx, wz) + 0.06f, wz, 3.0f);
      }
    }
  }
  if (mh::ready()) return;  // MakeHuman people carry real lanterns (lit above); the old bodies keep a hand glow
  npcs::forEach([&](const NpcRt& n) { spot({n.x + std::cos(n.yaw) * 0.32f, n.y + 0.95f, n.z - std::sin(n.yaw) * 0.32f}, 0.7f, 2.6f); });
  for (auto& r : g_res) {
    if (r.mode == RM_INSIDE) continue;
    spot({r.x + std::cos(r.yaw) * 0.3f, r.y + 0.95f, r.z - std::sin(r.yaw) * 0.3f}, 0.7f, 2.4f);
  }
}

void drawFestival() {
  if (E.festival.t <= 0) return;
  glEnable(GL_CULL_FACE);
  render::object(VFmt::P3C4, IM_FULL, SeasonKind::None, 0);
  g_festPoles.draw();
  glDisable(GL_CULL_FACE);
  render::program("festflag", FLAG_WIND_VS, FLAG_FS);
  g_festFlags.draw();
  float on = std::min(1.0f, E.festival.t / 5) * (0.35f + 0.65f * (1 - E.daylight));
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glDepthMask(GL_FALSE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  Shader& s = render::program("lanterns", LANTERN_VS, LANTERN_FS);
  s.set1f("u_on", on);
  glBindVertexArray(g_lanternVao);
  glDrawArrays(GL_POINTS, 0, 60);
  gfx::drawCalls++;
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

}  // namespace village
