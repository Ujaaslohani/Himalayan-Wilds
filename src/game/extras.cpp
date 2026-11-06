#include "extras.h"
#include "economy.h"
#include "season.h"
#include "wildlife.h"
#include "village.h"
#include "buildings.h"
#include "relics.h"
#include "app.h"
#include "fx.h"
#include "../world/terrain.h"
#include "../render/render.h"
#include "../models/shape.h"
#include "../models/houses.h"
#include "../models/people.h"
#include "../world/forest.h"
#include "../audio/audio.h"
#include <cmath>
#include <map>
#include <algorithm>

namespace {
// ---------------------------------------------------------------- stats
std::map<std::string, double> g_stats;
std::map<std::string, bool> g_ach;
struct AchDef { const char *id, *name, *desc, *stat; double need; };
const AchDef ACH[] = {
    {"first_hunt", "First Hunt", "Bring down your first animal", "kills", 1},
    {"hunter", "Seasoned Hunter", "Hunt 25 animals", "kills", 25},
    {"wolfbane", "Wolfbane", "Kill 10 wolves", "kill_wolf", 10},
    {"bear", "Bear Slayer", "Bring down a black bear", "kill_bear", 1},
    {"leopard", "Ghost of the Mountains", "Hunt a snow leopard", "kill_leopard", 1},
    {"walker", "Long Walk", "Walk 20 km", "walked", 20000},
    {"wheels3", "Pilgrim", "Find 3 lost prayer wheels", "wheels", 3},
    {"wheels", "Keeper of Prayers", "Find every lost prayer wheel", "wheels_all", 1},
    {"festivals", "Festive Spirit", "Celebrate 3 festivals", "festivals", 3},
    {"raids", "Wall of Arrows", "Drive off 3 wolf raids", "raids", 3},
    {"bounty5", "Village Hero", "Complete 5 bounties", "bounties", 5},
    {"bandits", "Peacekeeper", "Clear a bandit camp", "bandit_camps", 1},
    {"sleep", "Well Rested", "Sleep through 10 nights", "slept", 10},
    {"trophies", "Trophy Hall", "Mount 5 trophies", "trophies", 5},
};

// ---------------------------------------------------------------- sleep
float g_fade = 0;
int g_sleepStage = 0;  // 0 none, 1 fading out, 2 fading in

// ---------------------------------------------------------------- bounties
std::vector<extras::Bounty> g_bounties;
int g_boardDay = -1;
Mesh g_board;
float g_bountyT = 0;
const char* SP_NAME[SP_COUNT] = {"deer", "wild boar", "goral", "wolves", "black bear", "leopard"};
extras::Bounty makeBounty() {
  extras::Bounty b{};
  float r = randf();
  if (r < 0.5f) {
    static const char* ITEMS[] = {"herb", "wood", "stone", "fiber", "food", "deer_pelt", "bone", "sinew", "boar_hide"};
    static const int NEED[] = {8, 25, 20, 12, 30, 3, 6, 4, 2};
    int i = (int)(randf() * 9) % 9;
    b.kind = 0; b.item = ITEMS[i]; b.need = NEED[i] + (int)(randf() * NEED[i] * 0.5f); b.species = -1;
    b.text = "Bring " + std::to_string(b.need) + " " + itemLabel(b.item);
    b.renown = 80 + b.need * 4;
  } else {
    static const int SPS[] = {SP_DEER, SP_DEER, SP_BOAR, SP_GORAL, SP_WOLF, SP_WOLF, SP_BEAR, SP_LEOPARD};
    int sp = SPS[(int)(randf() * 8) % 8];
    b.kind = 1; b.species = sp;
    b.need = sp == SP_BEAR || sp == SP_LEOPARD ? 1 : sp == SP_WOLF ? 2 + (int)(randf() * 2) : 1 + (int)(randf() * 3);
    b.text = "Hunt " + std::to_string(b.need) + " " + SP_NAME[sp];
    b.renown = sp == SP_BEAR || sp == SP_LEOPARD ? 500 : sp == SP_WOLF ? 300 : 160;
  }
  static const Cost BONUS[] = {{{"arrow", 25}}, {{"goral_horn", 1}}, {{"wolf_fur", 1}}, {{"food", 20}}, {{"sinew", 3}}, {{"herb", 6}}};
  b.bonus = BONUS[(int)(randf() * 6) % 6];
  return b;
}

// ---------------------------------------------------------------- trophies
const char* TROPHY_IDS[5] = {"deer_pelt", "boar_hide", "goral_horn", "wolf_fur", "bear_hide"};
std::map<std::string, int> g_trophy;
Mesh g_rack, g_tro[6];

// ---------------------------------------------------------------- bandits
struct Bandit {
  float x, z, hp, cd;
  float yaw = 0, phase = 0, speed = 0, drawT = 0, strafe = 1, strafeT = 0, wanderT = 0, tx = 0, tz = 0;
  bool alert = false;
};
struct BArrow { Vec3 p, v; float life; bool stuck; };
std::vector<BArrow> g_barrows;  // bandit arrows in flight (or stuck in the ground for a few seconds)
Mesh g_arrowMesh;
const float DRAW_TIME = 0.85f;
bool g_banditOn = false;
float g_campX = 0, g_campZ = 0;
int g_banditDays = 0;
std::vector<Bandit> g_bandits;
// one camp per season, each tougher than the last until level 6, then it stays at 6
int g_campsSpawned = 0, g_campLevel = 1, g_campSeason = -1, g_campOffset = 2;
struct CampTier { int count; float hp, dmg, range, cd, hit; };
CampTier tier(int L) {
  L = std::max(1, std::min(6, L));
  return {2 + L, 80.0f + 20.0f * L, 3.0f + L, 24.0f + 2.0f * L, 2.7f - 0.15f * L, 0.45f + 0.05f * L};
}
void placeBandits(int n, float hp) {
  g_bandits.clear();
  for (int k = 0; k < n; k++) {
    float a = k * TAU / n + 0.3f, r = 3.2f + (k & 1) * 1.6f + (n > 5 ? 1.0f : 0.0f);
    g_bandits.push_back({g_campX + std::cos(a) * r, g_campZ + std::sin(a) * r, hp, 2 + k * 0.5f});
  }
}
Mesh g_tent, g_banditMesh;

void buildMeshes() {
  {
    Shape s;  // trophy rack: two carved posts and a crossbeam with a little slate roof
    s.box("#5b3a1f", {-1.4f, 1.2f, 0}, {0.16f, 2.4f, 0.16f});
    s.box("#5b3a1f", {1.4f, 1.2f, 0}, {0.16f, 2.4f, 0.16f});
    s.box("#6b4423", {0, 2.3f, 0}, {3.1f, 0.16f, 0.2f});
    s.box("#475569", {0, 2.55f, 0}, {3.4f, 0.08f, 0.7f}, {0.15f, 0, 0});
    s.box("#7f1d1d", {0, 2.42f, 0.12f}, {2.9f, 0.1f, 0.02f});
    g_rack = uploadMesh(s.build(true));
  }
  const char* col[5] = {"#8b5a2b", "#5b4636", "#d6cfc2", "#9ca3af", "#1c1917"};
  for (int i = 0; i < 5; i++) {
    Shape t;
    if (i == 2) {  // goral horns: two curved horn rods on a wooden plaque
      t.box("#6b4423", {0, 0, 0}, {0.35f, 0.3f, 0.05f});
      t.rod("#d6cfc2", {-0.06f, 0.05f, 0.03f}, {-0.16f, 0.32f, 0.08f}, 0.035f, 0.01f);
      t.rod("#d6cfc2", {0.06f, 0.05f, 0.03f}, {0.16f, 0.32f, 0.08f}, 0.035f, 0.01f);
    } else {       // stretched hide: flattened ball for the body and four leg tabs
      t.ball(col[i], {0, 0, 0}, {0.42f, 0.55f, 0.03f});
      for (int k = 0; k < 4; k++) t.box(col[i], {(k & 1) ? 0.38f : -0.38f, (k & 2) ? 0.4f : -0.4f, 0}, {0.18f, 0.12f, 0.03f}, {0, 0, (k & 1) == ((k >> 1) & 1) ? 0.6f : -0.6f});
      if (i == 4) t.ball("#e7e5e4", {0, 0.18f, 0.02f}, {0.16f, 0.07f, 0.02f});  // cream chest mark
    }
    g_tro[i] = uploadMesh(t.build(true));
  }
  {
    Shape s;  // notice board: carved deodar frame, slate roof, pinned paper notices and a prayer-flag string
    s.box("#5b3a1f", {-0.9f, 1.0f, 0}, {0.14f, 2.0f, 0.14f});
    s.box("#5b3a1f", {0.9f, 1.0f, 0}, {0.14f, 2.0f, 0.14f});
    s.box("#8b5a2b", {0, 1.35f, 0}, {1.7f, 1.0f, 0.08f});
    s.box("#6b4423", {0, 1.88f, 0}, {1.9f, 0.08f, 0.14f});
    s.box("#6b4423", {0, 0.82f, 0}, {1.9f, 0.08f, 0.14f});
    s.box("#475569", {0, 2.12f, 0.0f}, {2.2f, 0.07f, 0.55f}, {0.28f, 0, 0});
    s.box("#475569", {0, 2.12f, 0.0f}, {2.2f, 0.07f, 0.55f}, {-0.28f, 0, 0});
    const char* paper[4] = {"#f5f0e1", "#ede4cc", "#f8f4ea", "#e9dfc6"};
    for (int i = 0; i < 4; i++) s.box(paper[i], {-0.55f + i * 0.37f, 1.38f + (i & 1) * 0.08f, 0.05f}, {0.28f, 0.36f, 0.01f}, {0, 0, (i - 1.5f) * 0.05f});
    for (int i = 0; i < 4; i++) s.box("#b91c1c", {-0.55f + i * 0.37f, 1.53f + (i & 1) * 0.08f, 0.06f}, {0.03f, 0.03f, 0.02f});
    const char* flags[5] = {"#1d4ed8", "#f8fafc", "#dc2626", "#16a34a", "#eab308"};
    for (int i = 0; i < 5; i++) s.box(flags[i], {-0.72f + i * 0.36f, 1.98f, 0.12f}, {0.2f, 0.16f, 0.01f});
    g_board = uploadMesh(s.build(true));
  }
  {
    Shape s;  // ragged canvas tent
    s.cone("#78716c", {0, 0, 0}, 1.4f, 2.0f, 4, {0, 0.785f, 0});
    s.box("#44403c", {0, 0.05f, 0}, {2.2f, 0.1f, 2.2f});
    g_tent = uploadMesh(s.build(true));
    Shape b;  // bandit: dark robe, face cloth, quiver
    b.cone("#1f2937", {0, 0, 0}, 0.33f, 1.2f, 8);
    b.tube("#1f2937", {0, 0.85f, 0}, 0.19f, 0.25f, 0.5f, 8);
    b.ball("#b08060", {0, 1.52f, 0}, {0.13f, 0.15f, 0.13f});
    b.tube("#7f1d1d", {0, 1.42f, 0.02f}, 0.135f, 0.135f, 0.09f, 8);
    b.tube("#111827", {0, 1.6f, 0}, 0.15f, 0.14f, 0.08f, 8);
    b.box("#6b4423", {0.0f, 1.15f, -0.22f}, {0.12f, 0.5f, 0.1f}, {0.3f, 0, 0});
    g_banditMesh = uploadMesh(b.build(true));
  }
}

void hurt(float dmg) {
  auto& P = E.player;
  if (E.world.gearArmour) dmg = std::max(1.0f, std::round(dmg * 0.6f));
  P.hp = std::max(0.0f, P.hp - dmg);
  P.hurtT = E.clock;
  HudEvent e;
  e.kind = HudEvent::Hurt;
  e.amount = dmg;
  emitEvent(e);
  audio::tone(Wave::Square, 900, 300, 0.12f, 0.05f);
  if (P.hp <= 0) {
    P.hp = P.maxHp * 0.6f;
    P.pos = {0, 1.7f, 9};
    P.velY = 0;
    popup("Overpowered", rgbHex("#f87171"));
    actions::notify("The bandits drove you off. Villagers carried you back to the hearth.");
  }
}

void startBandits() {
  for (int tries = 0; tries < 400; tries++) {
    float a = randf() * TAU, r = settlementSafe() + 130 + randf() * 120;
    float x = std::cos(a) * r, z = std::sin(a) * r;
    if (std::fabs(x) > MAP_HALF - 40 || std::fabs(z) > MAP_HALF - 40) continue;
    if (world::slopeAt(x, z) > 0.4f || world::pathDistance(x, z) < 25 || world::riverMask(x, z) > 0.02f || world::lakeAt(x, z, 6) >= 0) continue;
    g_campX = x; g_campZ = z;
    g_campsSpawned++;
    g_campLevel = std::min(6, g_campsSpawned);
    CampTier T = tier(g_campLevel);
    placeBandits(T.count, T.hp);
    g_banditOn = true;
    g_banditDays = 0;
    actions::notify("Bandits (level " + std::to_string(g_campLevel) + ", " + std::to_string(T.count) + " archers) have made camp in the hills and are robbing travellers. Traders won't come until you clear them out. The camp is marked on your map.");
    return;
  }
}
void drawBandits(Shader& s) {
  const PersonModel& M = people::bandit();
  const Mesh& bow = people::banditBow();
  if (!g_arrowMesh.valid()) {
    Shape a;
    a.rod("#8b6a45", {0, 0, -0.38f}, {0, 0, 0.36f}, 0.008f, 0.008f, 4)
        .cone("#4b5563", {0, 0, 0.4f}, 0.016f, 0.06f, 4, {PI / 2, 0, 0})
        .box("#78716c", {0, 0.012f, -0.33f}, {0.003f, 0.022f, 0.1f})
        .box("#78716c", {0.012f, 0, -0.33f}, {0.022f, 0.003f, 0.1f});
    g_arrowMesh = uploadMesh(a.build(true));
  }
  for (auto& b : g_bandits) {
    if (b.hp <= 0) continue;
    float y = world::getTerrainHeight(b.x, b.z);
    if (!render::sphereVisible({b.x, y + 1, b.z}, 1.6f)) continue;
    float bob = std::fabs(std::sin(b.phase)) * std::min(0.05f, b.speed * 0.02f);
    Mat4 root = Mat4::compose({b.x, y + bob, b.z}, Quat::axisAngle({0, 1, 0}, b.yaw), {BODY_SCALE, BODY_SCALE, BODY_SCALE});
    float swing = std::sin(b.phase) * std::min(0.7f, b.speed * 0.3f);
    render::setModel(s, root);
    M.torso.draw();
    render::setModel(s, root * Mat4::compose(PIV_HEAD, Quat::fromEulerXYZ(b.alert ? 0.05f : 0.0f, 0, 0), {1, 1, 1}));
    M.head.draw();
    // left arm holds the bow: raised and pointed at you when alert, lowered while walking the camp
    float pull = b.drawT > 0 ? std::min(1.0f, b.drawT / (DRAW_TIME * 0.7f)) : 0.0f;
    float axL = b.alert ? -1.45f : -0.25f + swing * 0.3f, azL = b.alert ? 0.08f : 0.1f;
    float axR = b.alert ? -1.4f + 0.3f * pull : swing, azR = b.alert ? 0.3f + 0.4f * pull : -0.12f;  // drawing hand comes in to the chest
    Mat4 armL = root * Mat4::compose(PIV_ARM, Quat::fromEulerXYZ(axL, 0, azL), {1, 1, 1});
    render::setModel(s, armL);
    M.arm.draw();
    bow.draw();
    if (b.drawT > 0) {  // nocked arrow along the arm, from the bow back toward the drawing hand
      render::setModel(s, armL * Mat4::compose({-0.03f, -0.42f - 0.12f * (1 - pull), 0.078f}, Quat::axisAngle({1, 0, 0}, PI / 2), {1, 1, 1}));
      g_arrowMesh.draw();
    }
    render::setModel(s, root * Mat4::compose(Vec3(-PIV_ARM.x, PIV_ARM.y, 0), Quat::fromEulerXYZ(axR, 0, azR), {1, 1, 1}));
    M.arm.draw();
    float lx[2] = {swing, -swing};
    for (int k = 0; k < 2; k++) {
      Vec3 pv = k == 0 ? PIV_LEG : Vec3(-PIV_LEG.x, PIV_LEG.y, 0);
      render::setModel(s, root * Mat4::compose(pv, Quat::fromEulerXYZ(lx[k], 0, 0), {1, 1, 1}));
      M.leg.draw();
    }
  }
}
}  // namespace

namespace extras {

void reset() {
  g_stats.clear(); g_ach.clear(); g_trophy.clear(); g_bounties.clear(); g_bountyT = 0;
  g_banditOn = false; g_bandits.clear(); g_barrows.clear(); g_banditDays = 0; g_boardDay = -1;
  g_campsSpawned = 0; g_campLevel = 1; g_campSeason = -1; g_campOffset = 2;
}

void save(Json& d) {
  Json st = Json::object();
  for (auto& kv : g_stats) st.set(kv.first, Json::number(kv.second));
  d.set("stats", st);
  Json ac = Json::array();
  for (auto& kv : g_ach) if (kv.second) ac.push(Json::string(kv.first));
  d.set("achievements", ac);
  Json tr = Json::object();
  for (auto& kv : g_trophy) tr.set(kv.first, Json::number(kv.second));
  d.set("trophies", tr);
  Json bs = Json::array();
  for (auto& b : g_bounties) {
    Json o = Json::object();
    o.set("kind", Json::number(b.kind)); o.set("item", Json::string(b.item)); o.set("sp", Json::number(b.species));
    o.set("acc", Json::boolean(b.accepted));
    o.set("need", Json::number(b.need)); o.set("have", Json::number(b.have)); o.set("text", Json::string(b.text)); o.set("renown", Json::number(b.renown));
    Json bn = Json::object();
    for (auto& c : b.bonus) bn.set(c.first, Json::number(c.second));
    o.set("bonus", bn);
    bs.push(o);
  }
  d.set("bounties", bs);
  Json bd = Json::object();
  bd.set("on", Json::boolean(g_banditOn)); bd.set("x", Json::number(g_campX)); bd.set("z", Json::number(g_campZ)); bd.set("days", Json::number(g_banditDays));
  bd.set("spawned", Json::number(g_campsSpawned)); bd.set("level", Json::number(g_campLevel));
  bd.set("season", Json::number(g_campSeason)); bd.set("offset", Json::number(g_campOffset));
  Json bl = Json::array();
  for (auto& b : g_bandits) bl.push(Json::number(b.hp));
  bd.set("hp", bl);
  d.set("bandits", bd);
}

void load(const Json& d) {
  reset();
  if (const Json* st = d.get("stats")) for (auto& kv : st->obj) g_stats[kv.first] = kv.second.n;
  if (const Json* ac = d.get("achievements")) for (auto& a : ac->arr) g_ach[a.s] = true;
  if (const Json* tr = d.get("trophies")) for (auto& kv : tr->obj) g_trophy[kv.first] = (int)kv.second.n;
  if (const Json* bs = d.get("bounties"))
    for (auto& o : bs->arr) {
      Bounty b{};
      b.kind = (int)o.num("kind", 0); b.item = o.str("item"); b.species = (int)o.num("sp", -1);
      b.accepted = o.flag("acc");
      b.need = (int)o.num("need", 1); b.have = (int)o.num("have", 0); b.text = o.str("text"); b.renown = (int)o.num("renown", 100);
      if (const Json* bn = o.get("bonus")) for (auto& kv : bn->obj) b.bonus.push_back({kv.first, (int)kv.second.n});
      g_bounties.push_back(b);
    }
  if (const Json* bd = d.get("bandits")) {
    g_banditOn = bd->flag("on");
    g_campX = (float)bd->num("x", 0); g_campZ = (float)bd->num("z", 0); g_banditDays = (int)bd->num("days", 0);
    g_campsSpawned = (int)bd->num("spawned", 0); g_campLevel = (int)bd->num("level", 1);
    g_campSeason = (int)bd->num("season", -1); g_campOffset = (int)bd->num("offset", 2);
    if (g_banditOn) {
      int k = 0;
      if (const Json* hp = bd->get("hp"))
      {
        placeBandits((int)hp->arr.size(), 1);
        for (auto& h : hp->arr) g_bandits[k++].hp = (float)h.n;
      }
    }
  }
}

// ---------------------------------------------------------------- sleep
bool sleepSpot(float x, float z) {
  bool home = false;
  for (auto& s : A.structures) {
    if (KINDS[s.kind].group != KG_HOME) continue;
    home = true;
    if (hypot2(x - s.x, z - s.z) < structureRadius(s.kind) + 2.5f) return true;
  }
  return !home && hypot2(x, z) < 5.0f;  // no home yet: a bedroll by the hearth
}
std::string sleepBlocked() {
  if (E.raid.active) return "You can't sleep during a wolf raid.";
  if (E.clock - E.player.hurtT < 15) return "You can't sleep with danger this close.";
  float h = E.time;
  if (!(h >= 20.0f || h < 5.0f)) return "You can only sleep after 8 in the evening.";
  return "";
}
void sleep() {
  std::string why = sleepBlocked();
  if (!why.empty()) { actions::notify(why); audio::crispClick(); return; }
  if (g_sleepStage) return;
  g_sleepStage = 1;
  g_fade = 0;
}
float sleepFade() { return g_fade; }

// ---------------------------------------------------------------- bounties
const std::vector<Bounty>& bounties() { return g_bounties; }
void boardPos(float& x, float& z) { x = 6.0f; z = -5.0f; }
bool nearBoard(float x, float z) { float bx, bz; boardPos(bx, bz); return hypot2(x - bx, z - bz) < 3.2f; }
void accept(int i) {
  if (i < 0 || i >= (int)g_bounties.size() || g_bounties[i].accepted) return;
  g_bounties[i].accepted = true;
  audio::confirmSelect();
  actions::notify("Task accepted: " + g_bounties[i].text + ". Come back to the notice board to turn it in.");
}
void deliver(int i) {
  if (i < 0 || i >= (int)g_bounties.size()) return;
  Bounty& b = g_bounties[i];
  if (!b.accepted || !nearBoard(E.player.pos.x, E.player.pos.z)) return;
  if (b.kind == 0) {
    if (invCount(b.item) < b.need) { actions::notify("You need " + std::to_string(b.need) + " " + itemLabel(b.item) + "."); audio::crispClick(); return; }
    actions::addItems({{b.item, -b.need}});
  } else if (b.have < b.need) return;
  actions::addItems(b.bonus);
  actions::addScore(b.renown);
  audio::powerupChime();
  popup("Bounty complete +" + std::to_string(b.renown), rgbHex("#fbbf24"));
  actions::notify("Bounty complete: " + b.text + ". The village rewards you with " + std::to_string(b.renown) + " renown and extra supplies.");
  stat("bounties");
  b.text = "";  // done for today (slot stays empty until tomorrow's notices go up)
  g_bounties.erase(g_bounties.begin() + i);
}
void onKill(int sp) {
  stat("kills");
  static const char* K[SP_COUNT] = {"kill_deer", "kill_boar", "kill_goral", "kill_wolf", "kill_bear", "kill_leopard"};
  if (sp >= 0 && sp < SP_COUNT) stat(K[sp]);
  for (auto& b : g_bounties)
    if (b.kind == 1 && b.accepted && b.species == sp && b.have < b.need) {
      b.have++;
      if (b.have >= b.need) actions::notify("Task done: " + b.text + ". Turn it in at the notice board by the hearth.");
    }
}

// ---------------------------------------------------------------- trophies
int mounted(const std::string& id) { auto it = g_trophy.find(id); return it == g_trophy.end() ? 0 : it->second; }
void mount(const std::string& id) {
  if (invCount(id) <= 0) return;
  actions::addItems({{id, -1}});
  g_trophy[id]++;
  stat("trophies");
  audio::confirmSelect();
  actions::notify(std::string(itemName(id)) + " mounted on the trophy rack by the hearth. Village appeal +2.");
}
int trophyAppeal() { int n = 0; for (auto& kv : g_trophy) n += kv.second; return std::min(30, n * 2); }

// ---------------------------------------------------------------- bandits
bool banditsActive() { return g_banditOn; }
bool banditCamp(float& x, float& z) { x = g_campX; z = g_campZ; return g_banditOn; }
int banditLevel() { return g_campLevel; }
void debugBandits(float x, float z, int level) {
  g_campX = x; g_campZ = z; g_campLevel = level; g_campsSpawned = level;
  CampTier T = tier(level);
  placeBandits(T.count, T.hp);
  g_banditOn = true;
}
bool arrowHit(const Vec3& a, const Vec3& b, float dmg) {
  if (!g_banditOn) return false;
  for (auto& bd : g_bandits) {
    if (bd.hp <= 0) continue;
    float y0 = world::getTerrainHeight(bd.x, bd.z);
    // segment vs upright body cylinder: closest approach in the ground plane
    Vec3 d = b - a;
    float L2 = d.x * d.x + d.z * d.z;
    float t = L2 > 1e-6f ? clampf(((bd.x - a.x) * d.x + (bd.z - a.z) * d.z) / L2, 0, 1) : 0;
    float px = a.x + d.x * t, py = a.y + d.y * t, pz = a.z + d.z * t;
    if (hypot2(px - bd.x, pz - bd.z) > 0.42f || py < y0 || py > y0 + 1.8f) continue;
    bool head = py > y0 + 1.4f;
    bd.hp -= dmg * (head ? 2.0f : 1.0f);
    HudEvent ev; ev.kind = HudEvent::Hit; ev.headshot = head; ev.kill = bd.hp <= 0; emitEvent(ev);
    audio::meleePunch();
    if (bd.hp <= 0) {
      actions::addScore(150);
      bool any = false;
      for (auto& o : g_bandits) if (o.hp > 0) any = true;
      if (!any) {
        g_banditOn = false;
        stat("bandit_camps");
        int L = g_campLevel, food = 15 + 5 * L, arrows = 20 + 5 * L, furs = 1 + L / 3;
        actions::addItems({{"food", food}, {"arrow", arrows}, {"wolf_fur", furs}});
        actions::addScore(400 + 100 * L);
        audio::victoryFanfare();
        actions::notify("The level " + std::to_string(L) + " bandit camp is cleared! You recover " + std::to_string(food) + " food, " + std::to_string(arrows) + " arrows and " + std::to_string(furs) + " wolf fur. Traders can travel safely again." + (L < 6 ? " Next season's band will be tougher." : ""));
      } else popup("Bandit down", rgbHex("#fca5a5"));
    }
    return true;
  }
  return false;
}

// ---------------------------------------------------------------- stats
void stat(const std::string& key, double add) { g_stats[key] += add; }
double getStat(const std::string& key) { auto it = g_stats.find(key); return it == g_stats.end() ? 0 : it->second; }
std::vector<Achievement> achievements() {
  std::vector<Achievement> o;
  for (auto& a : ACH) o.push_back({a.name, a.desc, g_ach.count(a.id) > 0});
  return o;
}

void update(float dt) {
  // sleep fade: out (1.2 s), jump to dawn, back in
  if (g_sleepStage == 1) {
    g_fade = std::min(1.0f, g_fade + dt / 1.2f);
    if (g_fade >= 1) {
      if (E.time >= 12) nextDay();
      E.time = 5.98f;  // the dawn tick runs in a moment: food, firewood, events and the autosave
      E.player.hp = E.player.maxHp;
      stat("slept");
      g_sleepStage = 2;
      actions::notify("You slept soundly and woke at dawn, fully rested.");
    }
  } else if (g_sleepStage == 2) {
    g_fade = std::max(0.0f, g_fade - dt / 1.5f);
    if (g_fade <= 0) g_sleepStage = 0;
  }
  // distance walked
  static Vec3 last = E.player.pos;
  float step = hypot2(E.player.pos.x - last.x, E.player.pos.z - last.z);
  if (step < 5) stat("walked", step);
  last = E.player.pos;
  g_stats["wheels"] = relics::count();
  g_stats["wheels_all"] = relics::count() >= relics::total() && relics::total() > 0 ? 1 : 0;
  // achievements
  for (auto& a : ACH)
    if (!g_ach.count(a.id) && getStat(a.stat) >= a.need) {
      g_ach[a.id] = true;
      audio::levelUpStinger();
      popup(std::string("Achievement: ") + a.name, rgbHex("#fde047"));
      actions::addScore(100);
    }
  // notice board: three fresh, randomly chosen tasks go up every morning; yesterday's unfinished ones are taken down
  {
    int day = season.start * 7 + season.day;
    if (day != g_boardDay && (E.time >= 6.0f || g_boardDay < 0)) {
      bool first = g_boardDay < 0 && !g_bounties.empty();  // just loaded: keep today's board
      g_boardDay = day;
      if (!first) {
        int expired = 0;
        for (auto& b : g_bounties) if (b.accepted) expired++;
        g_bounties.clear();
        int n = 3 + (A.stage >= 3 ? 1 : 0);
        for (int k = 0; k < n; k++) {
          Bounty b = makeBounty();
          bool dup = false;
          for (auto& o : g_bounties) if (o.kind == b.kind && o.item == b.item && o.species == b.species) dup = true;
          if (dup) { k--; continue; }
          g_bounties.push_back(b);
        }
        if (E.playing) actions::notify(std::string("New tasks are pinned to the notice board by the hearth.") + (expired ? " Yesterday's unfinished tasks were taken down." : ""));
      }
    }
  }
  (void)g_bountyT;
  // bandits: a camp may appear every so often; while it stands, bandits shoot at you when you come close
  static float lastHour = E.time;
  bool dawn = lastHour < 6 && E.time >= 6;
  lastHour = E.time;
  if (dawn) {
    g_banditDays++;
    int day = season.start * 7 + season.day, sn = day / DAYS_PER_SEASON, dos = day % DAYS_PER_SEASON;
    if (sn != g_campSeason && dos >= g_campOffset) {
      g_campSeason = sn;                    // this season's camp (an uncleared camp counts as it)
      g_campOffset = 1 + (int)(randf() * 3.0f);  // next season's camp shows up on day 2-4
      if (!g_banditOn) startBandits();
    }
    int rob = 2 + g_campLevel;
    if (g_banditOn && invCount("food") > 0) actions::addItems({{"food", -std::min(rob, invCount("food"))}});  // they rob your foragers
  }
  if (g_banditOn) {
    // bandit archers: wander the camp; when you come near they spread out, keep their distance, strafe, plant their
    // feet to draw and loose real arrows at you (aimed, with spread that tightens as the camps get tougher)
    const Vec3& p = E.player.pos;
    CampTier T = tier(g_campLevel);
    float pCamp = hypot2(p.x - g_campX, p.z - g_campZ);
    for (size_t i = 0; i < g_bandits.size(); i++) {
      Bandit& b = g_bandits[i];
      if (b.hp <= 0) continue;
      float dx = p.x - b.x, dz = p.z - b.z, d = hypot2(dx, dz);
      float fromCamp = hypot2(b.x - g_campX, b.z - g_campZ);
      b.alert = d < T.range + 14 && pCamp < 95 && E.playing;
      float mx = 0, mz = 0, spd = 0;
      if (b.alert) {
        float keep = T.range * 0.6f;
        float ux = dx / std::max(d, 0.01f), uz = dz / std::max(d, 0.01f);
        b.strafeT -= dt;
        if (b.strafeT <= 0) { b.strafe = randf() < 0.5f ? -1.0f : 1.0f; b.strafeT = 1.5f + randf() * 2.5f; }
        if (d > keep + 3) { mx = ux; mz = uz; spd = d > keep + 12 ? 3.6f : 2.6f; }
        else if (d < keep - 5) { mx = -ux; mz = -uz; spd = 2.4f; }
        else { mx = -uz * b.strafe; mz = ux * b.strafe; spd = 1.8f; }
        if (fromCamp > 70) { mx = (g_campX - b.x) / fromCamp; mz = (g_campZ - b.z) / fromCamp; spd = 3.0f; }
        b.yaw = std::atan2(dx, dz);
        b.cd -= dt;
        if (b.drawT > 0) {
          spd = 0;  // feet planted while drawing
          b.drawT += dt;
          if (b.drawT >= DRAW_TIME) {
            b.drawT = 0;
            b.cd = T.cd + randf();
            // loose: aim at the chest with a spread (metres at the target) that shrinks with the camp level
            Vec3 from{b.x + std::sin(b.yaw) * 0.55f + std::cos(b.yaw) * 0.2f, world::getTerrainHeight(b.x, b.z) + 1.35f, b.z + std::cos(b.yaw) * 0.55f - std::sin(b.yaw) * 0.2f};
            float spread = (1.7f - 0.18f * g_campLevel) * (0.4f + d / 40.0f);
            float ea = randf() * TAU, er = std::sqrt(randf()) * spread;
            Vec3 to{p.x + std::cos(ea) * er, p.y - 0.55f + std::sin(ea) * er * 0.6f, p.z + std::sin(ea) * er};
            Vec3 dir = to - from;
            float L = dir.length(), sp = 34.0f, tt = L / sp;
            Vec3 v = dir * (sp / std::max(L, 0.01f));
            v.y += 0.5f * 9.8f * tt;  // lob to cancel the drop
            g_barrows.push_back({from, v, 3.0f, false});
            audio::tone(Wave::Triangle, 700, 200, 0.18f, 0.04f);  // bowstring twang
          }
        } else if (b.cd <= 0 && d < T.range) b.drawT = 0.001f;
      } else {
        b.drawT = 0;
        b.wanderT -= dt;
        if (b.wanderT <= 0) {
          float a = randf() * TAU, r = 2.5f + randf() * 6.0f;
          b.tx = g_campX + std::cos(a) * r; b.tz = g_campZ + std::sin(a) * r;
          b.wanderT = 4 + randf() * 6;
        }
        float wx = b.tx - b.x, wz = b.tz - b.z, wd = hypot2(wx, wz);
        if (wd > 0.6f) { mx = wx / wd; mz = wz / wd; spd = fromCamp > 20 ? 3.0f : 1.1f; b.yaw = std::atan2(wx, wz); }
      }
      // keep apart from the other bandits
      for (size_t j = 0; j < g_bandits.size(); j++) {
        if (j == i || g_bandits[j].hp <= 0) continue;
        float ox = b.x - g_bandits[j].x, oz = b.z - g_bandits[j].z, od = hypot2(ox, oz);
        if (od < 1.6f && od > 0.01f) { b.x += ox / od * (1.6f - od) * 0.5f; b.z += oz / od * (1.6f - od) * 0.5f; }
      }
      b.speed += (spd - b.speed) * std::min(1.0f, dt * 6);
      if (b.speed > 0.05f) {
        float nx = b.x + mx * b.speed * dt, nz = b.z + mz * b.speed * dt;
        forest::resolveTreeCollision(nx, nz, 0.35f);
        if (world::slopeAt(nx, nz) < 0.75f && world::waterHeightAt(nx, nz) < world::getTerrainHeight(nx, nz) + 0.3f) { b.x = nx; b.z = nz; }
        else b.strafe = -b.strafe;
      }
      b.phase += dt * b.speed * 3.4f;
    }
  }
  // bandit arrows: fly with gravity, hit you if they pass close to your body, stick in the ground
  for (size_t i = 0; i < g_barrows.size();) {
    BArrow& a = g_barrows[i];
    a.life -= dt;
    bool dead = a.life <= 0;
    if (!dead && !a.stuck) {
      a.v.y -= 9.8f * dt;
      Vec3 np = a.p + a.v * dt;
      const Vec3& pp = E.player.pos;
      Vec3 c{pp.x, pp.y - 0.65f, pp.z};
      Vec3 ab = np - a.p;
      float t = clampf((c - a.p).dot(ab) / std::max(1e-6f, ab.dot(ab)), 0, 1);
      Vec3 q = a.p + ab * t;
      float hd = hypot2(q.x - c.x, q.z - c.z), vy = q.y - c.y;
      if (hd < 0.42f && vy > -0.9f && vy < 0.75f) { hurt(tier(g_campLevel).dmg); dead = true; }
      else if (np.y < world::getTerrainHeight(np.x, np.z)) { a.stuck = true; a.life = 4.0f; a.p = np; a.p.y = world::getTerrainHeight(np.x, np.z) + 0.05f; }
      else a.p = np;
    }
    if (dead) { g_barrows[i] = g_barrows.back(); g_barrows.pop_back(); }
    else i++;
  }
}

void draw() {
  if (!g_rack.valid()) buildMeshes();
  Shader& s = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::None, OF_SOFT);
  {
    float bx, bz;
    boardPos(bx, bz);
    float by = world::getTerrainHeight(bx, bz);
    if (render::sphereVisible({bx, by + 1.2f, bz}, 2)) {
      render::setModel(s, Mat4::compose({bx, by, bz}, Quat::axisAngle({0, 1, 0}, std::atan2(-bx, -bz)), {1, 1, 1}));  // faces the hearth
      g_board.draw();
    }
  }
  // trophy rack beside the hearth, filled with what you've mounted
  int total = 0;
  for (auto& kv : g_trophy) total += kv.second;
  if (total > 0) {
    float rx = -6.5f, rz = 3.0f, ry = world::getTerrainHeight(rx, rz);
    if (render::sphereVisible({rx, ry + 1.5f, rz}, 3)) {
      Mat4 base = Mat4::compose({rx, ry, rz}, Quat::axisAngle({0, 1, 0}, 1.2f), {1, 1, 1});
      render::setModel(s, base);
      g_rack.draw();
      int slot = 0;
      for (int i = 0; i < 5 && slot < 8; i++)
        for (int k = 0; k < mounted(TROPHY_IDS[i]) && slot < 8; k++, slot++) {
          float lx = -1.15f + (slot % 4) * 0.77f, ly = slot < 4 ? 1.75f : 0.95f;
          render::setModel(s, base * Mat4::compose({lx, ly, 0.12f}, Quat::axisAngle({0, 0, 1}, 0.04f * (slot % 3 - 1)), {0.8f, 0.8f, 0.8f}));
          g_tro[i].draw();
        }
    }
  }
  if (g_banditOn) {
    float cy = world::getTerrainHeight(g_campX, g_campZ);
    if (render::sphereVisible({g_campX, cy + 1, g_campZ}, 8)) {
      int nt = 2 + g_campLevel / 2;
      for (int k = 0; k < nt; k++) {
        float ta = k * TAU / nt + 0.8f, tr = 6.5f + (k & 1) * 1.5f;
        float tx = g_campX + std::cos(ta) * tr, tz = g_campZ + std::sin(ta) * tr;
        render::setModel(s, Mat4::compose({tx, world::getTerrainHeight(tx, tz), tz}, Quat::axisAngle({0, 1, 0}, k * 0.7f), {1, 1, 1}));
        g_tent.draw();
      }
      fx::campfire(g_campX, cy, g_campZ, 0.8f);
    }
    drawBandits(s);
  }
  if (!g_barrows.empty()) {
    if (!g_arrowMesh.valid()) {
      Shape a;  // arrow along +z: shaft, iron head, grey fletching
      a.rod("#8b6a45", {0, 0, -0.38f}, {0, 0, 0.36f}, 0.008f, 0.008f, 4)
          .cone("#4b5563", {0, 0, 0.4f}, 0.016f, 0.06f, 4, {PI / 2, 0, 0})
          .box("#78716c", {0, 0.012f, -0.33f}, {0.003f, 0.022f, 0.1f})
          .box("#78716c", {0.012f, 0, -0.33f}, {0.022f, 0.003f, 0.1f});
      g_arrowMesh = uploadMesh(a.build(true));
    }
    for (auto& a : g_barrows) {
      if (!render::sphereVisible(a.p, 0.6f)) continue;
      Vec3 dir = a.v; dir.normalize();
      render::setModel(s, Mat4::compose(a.p, Quat::fromUnitVectors({0, 0, 1}, dir), {1, 1, 1}));
      g_arrowMesh.draw();
    }
  }
}

}  // namespace extras
