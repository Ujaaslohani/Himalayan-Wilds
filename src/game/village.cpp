#include "../render/sky.h"
#include "village.h"
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

const char* STAGE_NAMES[3] = {"Clearing", "Growing Village", "Established Settlement"};

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
};
enum { RM_WALK, RM_IDLE, RM_INSIDE };
std::vector<Resident> g_res;
int g_nextResident = 1;
const char* TINTS[10] = {"#4d5d3a", "#8b5a2b", "#9f1239", "#57534e", "#1e3a8a", "#7c2d12", "#166534", "#a16207", "#6b21a8", "#0f766e"};
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
    if (n && hypot2(n->x, n->z) < 85) {
      float a = randf() * TAU;
      ox = n->x + std::cos(a) * 1.6f;
      oz = n->z + std::sin(a) * 1.6f;
      return true;
    }
  }
  float a = randf() * TAU, d = res >= 0 ? 30 + randf() * 15 : 48 + randf() * 25;
  ox = std::cos(a) * d;
  oz = std::sin(a) * d;
  return true;
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
  if (hypot2(x, z) > 85) return res("Too far from the village hearth");
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
  return {2, STAGE_NAMES[2], {}};
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
    r.tint = rgbHex(TINTS[g_nextResident % 10]);
    r.home = 0; r.working = false; r.dance = randf() * TAU;
    g_res.push_back(r);
  }
  assignHomes();
  assignJobs();
}

void resetResidents() { g_res.clear(); }

void assignJobs() {
  std::vector<std::string> list;
  for (int k = 0; k < 4; k++)
    for (int i = 0; i < A.jobs[k]; i++) list.push_back(JOB_KEYS[k]);
  for (size_t i = 0; i < g_res.size(); i++) g_res[i].job = i < list.size() ? list[i] : "";
}

int residentCount() { return (int)g_res.size(); }

void updateResidents(float dt) {
  bool night = E.daylight < 0.25f;
  bool fest = E.festival.t > 0;
  if (fest) E.festival.t = std::max(0.0f, E.festival.t - dt);
  for (auto& r : g_res) {
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
      if (r.t <= 0 || (E.raid.active && !r.goingHome) || (fest && hypot2(r.x, r.z) > 8)) { pickTarget(r, night); r.mode = RM_WALK; r.t = 40; }
    } else {
      float dx = r.tx - r.x, dz = r.tz - r.z;
      float d = hypot2(dx, dz);
      r.speed += (1.25f - r.speed) * std::min(1.0f, dt * 3);
      r.yaw = turnToward(r.yaw, std::atan2(dx, dz), dt * 3.5f);
      r.t -= dt;
      if (d < 0.6f || r.t <= 0) {
        if (r.goingHome && night) { r.mode = RM_INSIDE; r.t = 5 + randf() * 20; }
        else if (r.goingHome && E.raid.active) { r.mode = RM_INSIDE; r.t = 20; }
        else { r.mode = RM_IDLE; r.t = r.working ? 14 + randf() * 16 : 4 + randf() * 9; }
      }
    }
    r.x += std::sin(r.yaw) * r.speed * dt;
    r.z += std::cos(r.yaw) * r.speed * dt;
    forest::resolveTreeCollision(r.x, r.z, 0.3f);
    resolveBuildingCollision(r.x, r.z, 0.3f);
    r.y = world::getTerrainHeight(r.x, r.z);
    r.phase += dt * r.speed * 3.4f;
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
}

void drawStructures() {
  if (g_structDirty) rebuildStructures();
  glEnable(GL_CULL_FACE);
  render::object(VFmt::P3C4, IM_FULL, SeasonKind::Build, OF_WIND);
  for (auto& b : g_structBatches) b->batch.draw();
}

static void fillResidents(bool shadow) {
  const Vec3& p = E.player.pos;
  DynBatch* T = shadow ? g_sTorsos : g_torsos;
  DynBatch* H = shadow ? g_sHeads : g_heads;
  DynBatch& AR = shadow ? g_sArms : g_arms;
  DynBatch& LG = shadow ? g_sLegs : g_legs;
  for (int v = 0; v < 3; v++) { T[v].clear(); H[v].clear(); }
  AR.clear();
  LG.clear();
  for (auto& r : g_res) {
    if (r.mode == RM_INSIDE) continue;
    float dx = r.x - p.x, dz = r.z - p.z;
    if (dx * dx + dz * dz > 95 * 95) continue;
    if (shadow && dx * dx + dz * dz > 46 * 46) continue;
    if (!shadow && !render::cam.sphere({r.x, r.y + 1, r.z}, 1.5f)) continue;
    int v = r.id % 3;
    if (T[v].items.size() >= 48) continue;
    float bob = std::fabs(std::sin(r.phase)) * std::min(0.05f, r.speed * 0.02f);
    float sc = (v == 2 ? 0.95f : v == 1 ? 0.94f : 1.0f) * BODY_SCALE;
    Mat4 base = Mat4::compose({r.x, r.y + bob, r.z}, Quat::axisAngle({0, 1, 0}, r.yaw), {sc, sc, sc});
    T[v].add(base, r.tint);
    float look = r.speed < 0.1f ? std::sin(E.clock * 0.7f + r.id) * 0.5f : 0;
    H[v].add(base * Mat4::compose({0, PIV_HEAD.y, 0}, Quat::axisAngle({0, 1, 0}, look), {1, 1, 1}));
    float swing = std::sin(r.phase) * std::min(0.7f, r.speed * 0.35f);
    bool chop = r.working && r.mode == RM_IDLE && r.speed < 0.1f;
    float idle = chop ? -1.2f + std::sin(E.clock * 5 + r.id) * 0.9f : r.speed < 0.1f ? std::sin(E.clock * 1.6f + r.id) * 0.05f : 0;
    for (int k = 0; k < 2; k++) {
      float side = k == 0 ? 1.0f : -1.0f;
      float ang = side * -swing + (!chop && E.festival.t > 0 && hypot2(r.x, r.z) < 8 ? -2.4f + std::sin(E.clock * 7 + r.id + k) * 0.4f : idle);
      AR.add(base * Mat4::compose({side * PIV_ARM.x, PIV_ARM.y, 0}, Quat::axisAngle({1, 0, 0}, ang), {1, 1, 1}), r.tint);
      LG.add(base * Mat4::compose({side * PIV_LEG.x, PIV_LEG.y, 0}, Quat::axisAngle({1, 0, 0}, side * swing), {1, 1, 1}));
    }
  }
  for (int v = 0; v < 3; v++) { T[v].upload(); H[v].upload(); }
  AR.upload();
  LG.upload();
}

void drawResidents() {
  fillResidents(false);
  glEnable(GL_CULL_FACE);
  render::object(VFmt::P3C4N4, IM_FULL, SeasonKind::None, OF_TINTWHITE | OF_SOFT);
  for (int v = 0; v < 3; v++) { g_torsos[v].draw(); g_heads[v].draw(); }
  g_arms.draw();
  g_legs.draw();
}

void drawShadowStatic() {
  if (g_structDirty) rebuildStructures();
  render::object(VFmt::P3C4, IM_FULL, SeasonKind::Build, OF_WIND);
  for (auto& b : g_structBatches) b->batch.draw();
}

void drawShadow() {
  fillResidents(true);
  render::object(VFmt::P3C4N4, IM_FULL, SeasonKind::None, 0);
  for (int v = 0; v < 3; v++) { g_sTorsos[v].draw(); g_sHeads[v].draw(); }
  g_sArms.draw();
  g_sLegs.draw();
}

void addLights() {
  if (g_structDirty) rebuildStructures();
  const Vec3& p = E.player.pos;
  auto spot = [&](const Vec3& s, float size, float poolR) {
    if (size > 0) fx::glow(s.x, s.y, s.z, size);
    if (poolR > 0 && hypot2(s.x - p.x, s.z - p.z) < 450) fx::pool(s.x, world::getTerrainHeight(s.x, s.z) + 0.06f, s.z, poolR);
  };
  for (auto& l : g_staticLights) spot(l, 1.1f, 3.2f);
  static std::vector<Vec3> templeSpots = temple::lightSpots();
  for (auto& l : templeSpots) spot(l, 1.3f, 0);
  std::vector<Vec3> win;
  for (auto& b : A.buildings) {
    if (!b.built) continue;
    WINDOWS_FOR(b.type, win);
    float c = std::cos(b.rotY), sn = std::sin(b.rotY);
    for (auto& w : win) {
      bool fire = b.type == "campfire";
      Vec3 s(b.pos[0] + w.x * c + w.z * sn, b.pos[1] + w.y, b.pos[2] - w.x * sn + w.z * c);
      spot(s, fire ? 0 : 1.1f, fire ? 7 : 3.4f);
    }
  }
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
