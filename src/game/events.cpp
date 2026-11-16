#include "extras.h"
#include "events.h"
#include "../world/terrain.h"
#include "../render/render.h"
#include "../models/shape.h"
#include "engine.h"
#include "economy.h"
#include "season.h"
#include "app.h"
#include "resources.h"
#include "../audio/audio.h"
#include "../render/mhuman.h"
#include "../assets/assets.h"
#include "../platform/platform.h"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace {
enum Ev { EV_TRADER, EV_HARVEST, EV_SICK, EV_FESTIVAL, EV_ROCKFALL, EV_STORM, EV_WANDERER, EV_LOSAR, EV_HARVESTFEST, EV_COUNT };
const char* NAMES[EV_COUNT] = {"Travelling trader", "Good harvest", "Sick livestock", "Festival day", "Rockfall", "Storm day", "Wanderer", "Losar", "Harvest festival"};
int g_active = -1, g_days = 0;
unsigned g_sold = 0;   // bridge-caravan offers already taken this visit
float g_work = 1.0f;

bool hasKind(StructureKind k) { for (auto& s : A.structures) if (s.kind == k) return true; return false; }
}  // namespace

namespace events {

void reset() { g_active = -1; g_days = 0; g_work = 1.0f; }
int active() { return g_active; }
int festival() { return g_active == EV_LOSAR ? 1 : g_active == EV_HARVESTFEST ? 2 : 0; }
const char* activeName() { return g_active >= 0 ? NAMES[g_active] : ""; }
int daysSince() { return g_days; }
void restore(int a, int d) {
  g_active = a >= 0 && a < EV_COUNT ? a : -1;
  g_days = d;
  g_work = g_active == EV_FESTIVAL ? 1.25f : (g_active == EV_LOSAR || g_active == EV_HARVESTFEST) ? 1.2f : 1.0f;
}

float workMult(const std::string&) { return g_work; }
float tradeMult() { return g_active == EV_TRADER ? 2.0f : 1.0f; }
bool forceRain() { return g_active == EV_STORM; }

// ---- travelling trader caravan: a robed trader and a laden mule beside your market stall while the event lasts
bool bridgeBuilt() { for (auto& b : A.buildings) if (b.id == "bridge") return b.built; return false; }
bool caravanBig() { return g_active == EV_TRADER && bridgeBuilt(); }
bool caravanSold(int i) { return (g_sold >> i) & 1u; }
void caravanMarkSold(int i) { g_sold |= 1u << i; }
bool traderPos(float& x, float& y, float& z, float& yaw) {
  if (g_active != EV_TRADER) return false;
  if (bridgeBuilt() && !hasKind(SK_MARKET_STALL)) {   // no stall yet: the caravan unloads by the hearth
    x = 5.5f; z = 7.5f; y = world::getTerrainHeight(x, z); yaw = 0.6f;
    return true;
  }
  for (auto& st : A.structures)
    if (st.kind == SK_MARKET_STALL) {
      float c = std::cos(st.rot), sn = std::sin(st.rot);
      x = st.x + sn * 3.2f + c * 1.2f; z = st.z + c * 3.2f - sn * 1.2f;  // in front of the stall
      y = world::getTerrainHeight(x, z);
      yaw = st.rot;
      return true;
    }
  return false;
}
// ---- caravan figures: traders on the MakeHuman body, pack yaks from the user's yak model (decimated 10.4K -> 3.1K
// triangles offline, assets/models/yak.hwm + yak.png). The yak's head and neck carry a weight; each yak turns and
// dips its head on its own slow rhythm (vertices bent on the CPU: 5.4K per yak, a few at a time). Arrows pass
// through them: nothing here is a target.
namespace {
struct YakV { float p[3], n[3], uv[2], w; };
std::vector<YakV> g_yakSrc;
std::vector<uint32_t> g_yakIdx;
Vec3 g_yakPivot;
Mesh g_yakMesh[3];
Texture g_yakTex;
bool g_yakTried = false;
std::vector<VtxGlb> g_yakTmp;
std::vector<uint32_t> g_yakHead;   // vertices with any head weight: the only ones re-posed
uint32_t g_yakHeadLo = 0, g_yakHeadHi = 0;
float g_yakPosedAt[3] = {-1, -1, -1};
bool loadYak() {
  g_yakTried = true;
  std::vector<uint8_t> b;
  if (!platform::readFile(platform::assetPath("models/yak.hwm"), b) || b.size() < 24 || std::memcmp(b.data(), "HWM1", 4) != 0) return false;
  uint32_t nv, ni;
  std::memcpy(&nv, b.data() + 4, 4); std::memcpy(&ni, b.data() + 8, 4);
  float pv[3];
  std::memcpy(pv, b.data() + 12, 12);
  if (b.size() < 24 + (size_t)nv * sizeof(YakV) + (size_t)ni * 4) return false;
  g_yakPivot = {pv[0], pv[1], pv[2]};
  g_yakSrc.resize(nv);
  std::memcpy(g_yakSrc.data(), b.data() + 24, (size_t)nv * sizeof(YakV));
  g_yakIdx.resize(ni);
  std::memcpy(g_yakIdx.data(), b.data() + 24 + (size_t)nv * sizeof(YakV), (size_t)ni * 4);
  int w = 0, h = 0;
  std::vector<uint8_t> px = assets::loadImageRGBA("models/yak.png", w, h);
  if (px.empty()) return false;
  g_yakTex = uploadTextureRGBA(px.data(), w, h, true, true, false);
  // rest pose once; afterwards only the head range is rewritten
  g_yakTmp.resize(nv);
  g_yakHeadLo = nv; g_yakHeadHi = 0;
  for (uint32_t i = 0; i < nv; i++) {
    const YakV& sv = g_yakSrc[i];
    VtxGlb& o = g_yakTmp[i];
    o.p[0] = sv.p[0]; o.p[1] = sv.p[1]; o.p[2] = sv.p[2];
    o.n[0] = (int8_t)std::lround(clampf(sv.n[0], -1, 1) * 127); o.n[1] = (int8_t)std::lround(clampf(sv.n[1], -1, 1) * 127);
    o.n[2] = (int8_t)std::lround(clampf(sv.n[2], -1, 1) * 127); o.n[3] = 0;
    o.uv[0] = floatToHalf(sv.uv[0]); o.uv[1] = floatToHalf(1.0f - sv.uv[1]);
    o.c = 0xffffffffu;
    if (sv.w > 0.001f) { g_yakHead.push_back(i); g_yakHeadLo = std::min(g_yakHeadLo, i); g_yakHeadHi = std::max(g_yakHeadHi, i + 1); }
  }
  for (int k = 0; k < 3; k++) {
    MeshData md;
    md.fmt = VFmt::GLB;
    md.vertexCount = (int)nv;
    md.verts.resize((size_t)nv * sizeof(VtxGlb));
    std::memcpy(md.verts.data(), g_yakTmp.data(), md.verts.size());
    md.idx = g_yakIdx;
    for (auto& v : g_yakSrc) {
      md.bmin = {std::min(md.bmin.x, v.p[0]), std::min(md.bmin.y, v.p[1]), std::min(md.bmin.z, v.p[2])};
      md.bmax = {std::max(md.bmax.x, v.p[0]), std::max(md.bmax.y, v.p[1]), std::max(md.bmax.z, v.p[2])};
    }
    g_yakMesh[k] = uploadMesh(md);
  }
  return true;
}
// bend the head and neck (weight w) about the neck pivot: yaw turns it, pitch dips it toward the grass
void poseYak(int k, float yaw, float pitch) {
  for (uint32_t i : g_yakHead) {
    const YakV& sv = g_yakSrc[i];
    float a = yaw * sv.w, bb = pitch * sv.w;
    float ca = std::cos(a), sa = std::sin(a), cb = std::cos(bb), sb = std::sin(bb);
    auto rot = [&](float x, float y, float z) {
      float qy = y * cb + z * sb, qz = -y * sb + z * cb;   // pitch about x (+: nose down)
      return Vec3(x * ca + qz * sa, qy, -x * sa + qz * ca);  // yaw about y
    };
    Vec3 p = rot(sv.p[0] - g_yakPivot.x, sv.p[1] - g_yakPivot.y, sv.p[2] - g_yakPivot.z) + g_yakPivot;
    Vec3 nn = rot(sv.n[0], sv.n[1], sv.n[2]);
    VtxGlb& o = g_yakTmp[i];
    o.p[0] = p.x; o.p[1] = p.y; o.p[2] = p.z;
    o.n[0] = (int8_t)std::lround(clampf(nn.x, -1, 1) * 127); o.n[1] = (int8_t)std::lround(clampf(nn.y, -1, 1) * 127);
    o.n[2] = (int8_t)std::lround(clampf(nn.z, -1, 1) * 127);
  }
  if (g_yakHeadHi <= g_yakHeadLo) return;
  glBindBuffer(GL_ARRAY_BUFFER, g_yakMesh[k].vbo);
  glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(g_yakHeadLo * sizeof(VtxGlb)), (GLsizeiptr)((g_yakHeadHi - g_yakHeadLo) * sizeof(VtxGlb)), g_yakTmp.data() + g_yakHeadLo);
}
Vec3 lin(const char* h) { Color c = Color::hex(h); return {std::pow(c.r, 2.2f), std::pow(c.g, 2.2f), std::pow(c.b, 2.2f)}; }
}  // namespace

static void caravanSpots(float x, float z, float yaw, std::vector<Vec3>& yaks, std::vector<Vec3>& traders) {
  float c = std::cos(yaw), sn = std::sin(yaw);
  float mx = x + c * 1.9f, mz = z - sn * 1.9f;
  yaks.push_back({mx, 0, mz});
  if (caravanBig()) {
    for (int k = 1; k <= 2; k++) yaks.push_back({mx + c * 3.3f * k, 0, mz - sn * 3.3f * k});
    traders.push_back({mx + c * 4.6f + sn * 1.6f, 0, mz - sn * 4.6f + c * 1.6f});
  }
}

void drawCaravan(bool shadow) {
  float x, y, z, yaw;
  if (!traderPos(x, y, z, yaw)) return;
  const Vec3& pp = E.player.pos;
  float dc = hypot2(x - pp.x, z - pp.z);
  if (dc > (shadow ? 46.0f : 140.0f)) return;   // the caravan is only ever in the village: nothing to do from afar
  if (!shadow && !render::cam.sphere({x + std::cos(yaw) * 4, y + 1, z - std::sin(yaw) * 4}, 10)) return;
  std::vector<Vec3> yaks, guards;
  caravanSpots(x, z, yaw, yaks, guards);
  // pack yaks
  if (!g_yakTried) loadYak();
  if (!g_yakSrc.empty()) {
    Shader& s = render::object(VFmt::GLB, IM_NONE, SeasonKind::None, OF_SOFT);
    AssetMaterial m;
    m.tex = &g_yakTex;
    m.kind = SeasonKind::None;
    render::bindMaterial(s, m);
    glDisable(GL_CULL_FACE);
    for (size_t k = 0; k < yaks.size() && k < 3; k++) {
      float t = E.clock + k * 7.3f;
      // grazing rhythm: long slow looks around, now and then the head dips to crop the grass
      float hy = 0.32f * std::sin(t * 0.37f) + 0.12f * std::sin(t * 0.91f + 1.3f);
      float dip = std::max(0.0f, std::sin(t * 0.21f + k)) ;
      float hp = 0.06f * std::sin(t * 0.53f) + 0.55f * dip * dip * dip;
      // re-pose about 10 times a second, and only within 50 m (beyond that the head turns are too small to see)
      if (!shadow && dc < 50 && E.clock - g_yakPosedAt[k] > 0.1f) { poseYak((int)k, hy, hp); g_yakPosedAt[k] = E.clock; }
      float ax = yaks[k].x, az = yaks[k].z;
      float ry = yaw + 1.2f + 0.05f * std::sin(E.clock * 0.3f + k);
      render::setModel(s, Mat4::compose({ax, world::getTerrainHeight(ax, az) - 0.03f, az}, Quat::axisAngle({0, 1, 0}, ry), {0.82f, 0.82f, 0.82f}));
      g_yakMesh[k].draw();
    }
  }
  // traders: chuba robes in maroon and saffron, fur caps
  if (!mh::ready()) return;
  struct TR { Vec3 p; float yaw; bool second; };
  std::vector<TR> tr;
  tr.push_back({{x, y, z}, std::atan2(pp.x - x, pp.z - z), false});
  for (auto& g : guards) tr.push_back({g, yaw + 2.6f, true});
  mh::begin(shadow);
  for (auto& t : tr) {
    mh::Look L;
    L.kind = t.second ? mh::ELDER : mh::MAN;
    L.tunic = lin(t.second ? "5b3a1e" : "7f1d1d");
    L.trousers = lin("3b2a20");
    L.shoes = lin("2a1a10");
    L.sash = lin(t.second ? "b45309" : "f59e0b");
    L.cap = lin("4a3424");
    L.skin = t.second ? 0.85f : 0.9f;
    mh::Pose P;
    float ph = E.clock + t.p.x * 0.7f;
    // idle: weight shifts, the head looks about (and toward you while you trade), hands rest on the sash
    P.headYaw = 0.35f * std::sin(ph * 0.45f) + 0.12f * std::sin(ph * 1.3f);
    P.headPitch = 0.05f * std::sin(ph * 0.8f) - 0.04f;
    P.armL = P.armR = 0.05f + 0.03f * std::sin(ph * 1.1f);
    P.elbowL = P.elbowR = 0.35f;
    P.lean = 0.02f * std::sin(ph * 0.6f);
    float ty = world::getTerrainHeight(t.p.x, t.p.z);
    float d = hypot2(t.p.x - pp.x, t.p.z - pp.z);
    Mat4 base = Mat4::compose({t.p.x, ty, t.p.z}, Quat::axisAngle({0, 1, 0}, t.yaw), {0.98f, 0.98f, 0.98f});
    mh::draw(base, L, P, d, shadow);
  }
  mh::end();
}

void dawn() {
  // yesterday's event ends at dawn
  g_active = -1;
  g_work = 1.0f;
  g_sold = 0;
  if (A.population <= 0 && A.structures.empty()) return;
  g_days++;
  // seasonal festivals on fixed days: Losar (new year) opens winter, the harvest festival falls mid-autumn
  if ((season.index == 3 && season.dayOfSeason == 1) || (season.index == 2 && season.dayOfSeason == 4)) {
    bool losar = season.index == 3;
    g_active = losar ? EV_LOSAR : EV_HARVESTFEST;
    extras::stat("festivals");
    g_days = 0;
    g_work = 1.2f;
    E.festival.t = 400;
    int feast = std::max(0, std::min(invCount("food"), A.population));
    if (feast > 0) actions::addItems({{"food", -feast}});
    A.arrivalAcc += 15;  // word of the celebration travels: the next settler comes sooner
    audio::victoryFanfare();
    if (losar) {
      ambience::ringBell(5, 2.0f);
      actions::notify("Losar! The new year begins. Butter lamps ring the hearth, prayer flags are renewed and the village feasts (" + std::to_string(feast) + " food). All work +20% today.");
    } else
      actions::notify("Harvest festival! Garlands hang between the homes and the village shares a feast (" + std::to_string(feast) + " food). All work +20% today.");
    return;
  }
  // once the bridge is built the trade trail is open: a caravan crosses it on the second day of every season
  if (bridgeBuilt() && season.dayOfSeason == 2 && !extras::banditsActive()) {
    g_active = EV_TRADER;
    g_days = 0;
    actions::notify("The seasonal caravan has crossed your bridge and is unloading in the village. Rare goods, a standing order and a teaching, today only. Every trade gives double.");
    audio::coinPickup();
    return;
  }
  float chance = 0.12f + 0.1f * g_days + (A.stage >= 3 ? 0.08f : 0.0f);
  if (g_days < 7 && randf() > chance) return;  // a week never passes without something happening
  // pick an event that makes sense right now
  int pick = -1;
  for (int tries = 0; tries < 12 && pick < 0; tries++) {
    int e = (int)(randf() * EV_COUNT) % EV_COUNT;
    if (A.stage >= 3 && randf() < 0.25f) e = EV_TRADER;  // market towns draw more traders
    if (e == EV_TRADER && (!hasKind(SK_MARKET_STALL) || extras::banditsActive())) continue;  // bandits scare traders off
    if (e == EV_SICK && !hasKind(SK_ANIMAL_SHELTER)) continue;
    if (e == EV_STORM && season.snow > 0.4f) continue;
    if (e == EV_WANDERER && A.population >= app::capacity()) continue;
    if (e == EV_HARVEST && season.index == 3) continue;
    pick = e;
  }
  if (pick < 0) return;
  g_active = pick;
  g_days = 0;
  switch (pick) {
    case EV_TRADER:
      actions::notify("A travelling trader has set up by your market stall. Today every trade gives double.");
      audio::coinPickup();
      break;
    case EV_HARVEST: {
      int f = 10 + A.population * 2;
      actions::addItems({{"food", f}});
      actions::notify("A good harvest! The fields and forest gave plenty: +" + std::to_string(f) + " food.");
      break;
    }
    case EV_SICK:
      if (invCount("herb") >= 5) {
        actions::addItems({{"herb", -5}});
        actions::notify("The livestock fell sick overnight. Your herbalists used 5 herbs and nursed them back to health.");
      } else {
        g_work = 0.8f;
        actions::notify("The livestock are sick and there aren't enough herbs (need 5). Villagers are tending them: all work -20% today.");
      }
      break;
    case EV_FESTIVAL:
      g_work = 1.25f;
      E.festival.t = 150;
      extras::stat("festivals");
      audio::victoryFanfare();
      actions::notify("Festival day! Spirits are high: all work +25% today.");
      break;
    case EV_ROCKFALL: {
      int s = 10 + (int)(randf() * 10);
      actions::addItems({{"stone", s}});
      actions::notify("A rockfall slid onto a mountain trail overnight. Villagers cleared it and kept the stone: +" + std::to_string(s) + " stone.");
      break;
    }
    case EV_STORM:
      actions::notify("Dark clouds are rolling down the valley. A storm will last all day.");
      break;
    case EV_WANDERER:
      A.population += 1;
      actions::notify("A wanderer from the high passes asked to stay, and moved into a free bed.");
      break;
  }
}

}  // namespace events
