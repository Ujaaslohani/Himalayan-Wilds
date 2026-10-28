#include "player.h"
#include "engine.h"
#include "archery.h"
#include "wildlife.h"
#include "resources.h"
#include "npcs.h"
#include "village.h"
#include "buildings.h"
#include "props.h"
#include "season.h"
#include "../world/terrain.h"
#include "../world/forest.h"
#include "../models/temple.h"
#include "../models/houses.h"
#include "../models/models.h"
#include "../platform/platform.h"
#include "../render/render.h"
#include "../audio/audio.h"
#include <set>

namespace {
float g_feetY = 0;
float g_eyeH = 1.65f;
float g_footT = 0;
float g_slowT = 0;
Vec3 g_vel;
bool g_lmb = false, g_rmb = false;
float g_fov = 75;
Mat4 g_camWorld;
Vec3 g_fwd{0, 0, -1};
std::set<std::string> g_synced;

void popupItems(const Cost& delta) {
  for (auto& d : delta)
    if (d.second > 0) popup("+" + std::to_string(d.second) + " " + itemLabel(d.first));
}

QuestTarget targetForMaterial(const std::string& item) {
  const Vec3& p = E.player.pos;
  int rt = item == "wood" ? RES_WOOD : item == "stone" ? RES_STONE : item == "fiber" ? RES_FIBER : item == "herb" ? RES_HERB : -1;
  if (rt >= 0) {
    const ResNode* n = resources::nearestOfType((ResType)rt, p.x, p.z);
    if (n) return {true, n->x, n->z, std::string("Gather ") + itemLabel(item)};
    return {};
  }
  int sp = item == "deer_pelt" ? SP_DEER : item == "boar_hide" ? SP_BOAR : item == "goral_horn" ? SP_GORAL : -1;
  if (sp >= 0) {
    float x, z;
    if (wildlife::nearestHerd((Species)sp, p.x, p.z, x, z)) {
      const char* nm = sp == SP_DEER ? "deer" : sp == SP_BOAR ? "boar" : "goral";
      return {true, x, z, std::string("Hunt ") + nm + " for " + itemLabel(item)};
    }
  }
  return {};
}

QuestTarget buildTarget(const std::string& id) {
  for (auto& b : A.buildings) {
    if (b.id != id) continue;
    if (b.built) return {};
    for (auto& c : b.cost)
      if (invCount(c.first) < c.second) return targetForMaterial(c.first);
    const Vec3& p = E.player.pos;
    if (hypot2(p.x, p.z) > 22) return {true, 0, 0, "Return to the village to build the " + b.name + " [B]"};
    return {};
  }
  return {};
}

QuestTarget recruitTarget(const std::string& id) {
  for (auto& s : A.specialists) {
    if (s.id != id) continue;
    if (s.recruited) return {};
    float x, z;
    const Camp* c = npcs::camp(id);
    if (!npcs::position(id, x, z)) { x = c->campX; z = c->campZ; }
    return {true, x, z, "Find " + s.name + " in " + c->area};
  }
  return {};
}

std::string activeQuestId() {
  for (auto& q : A.quests)
    if (!q.completed) return q.id;
  return "";
}

QuestTarget computeQuestTarget() {
  const Vec3& p = E.player.pos;
  std::string q = activeQuestId();
  Animal* carcass = wildlife::findCarcass(p.x, p.z, 80);
  if (carcass && (q == "ch1" || q == "ch7")) return {true, carcass->x, carcass->z, "Harvest your kill [E]"};
  auto first = [](std::initializer_list<QuestTarget> l) {
    for (auto& t : l) if (t.valid) return t;
    return QuestTarget{};
  };
  if (q == "ch1") {
    float x, z;
    if (wildlife::nearestHerd(SP_DEER, p.x, p.z, x, z)) return {true, x, z, "Hunt deer — crouch [C] to sneak closer"};
    return {};
  }
  if (q == "ch2") return buildTarget("hunter_hut");
  if (q == "ch3") return first({recruitTarget("carpenter"), recruitTarget("herbalist")});
  if (q == "ch4") {
    QuestTarget best;
    float bd = 1e30f;
    for (auto& c : A.clues) {
      if (c.discovered) continue;
      float d = hypot2(c.x - p.x, c.z - p.z);
      if (d < bd) { bd = d; best = {true, c.x, c.z, "Discover: " + c.title}; }
    }
    return best;
  }
  if (q == "ch5") return buildTarget("herbalist_lodge");
  if (q == "ch6") return first({buildTarget("carpenter_workshop"), buildTarget("watchtower")});
  if (q == "ch7") return buildTarget("bridge");
  if (q == "ch8")
    return first({recruitTarget("builder"), recruitTarget("carpenter"), recruitTarget("herbalist"), buildTarget("hunter_hut"), buildTarget("herbalist_lodge"), buildTarget("carpenter_workshop"), buildTarget("watchtower")});
  return {};
}

void startSync(const Shrine& sh) {
  auto& C = E.cutscene;
  C.t = 0; C.dur = 11; C.x = sh.x; C.y = sh.y; C.z = sh.z; C.yaw = sh.yaw; C.name = sh.name;
  ambience::ringBell(3, 2.4f);
  auto& bl = E.player.blessed;
  if (g_synced.count(sh.name) || std::find(bl.begin(), bl.end(), sh.name) != bl.end()) return;
  g_synced.insert(sh.name);
  bl.push_back(sh.name);
  props::markSummit(sh.name.c_str());
  E.player.maxHp += 25;
  E.player.hp = E.player.maxHp;
  actions::addScore(500);
  int found = 0;
  for (auto& c : A.clues)
    if (!c.discovered && hypot2(c.x - sh.x, c.z - sh.z) < 190) { actions::discoverClue(c.id); found++; }
  std::string name = sh.name;
  int maxHp = (int)E.player.maxHp;
  actions::later(10.5, [name, maxHp, found] {
    std::string msg = "Your prayers echo across " + name + ". +500 renown, maximum health +25 (now " + std::to_string(maxHp) + "), fully healed.";
    if (found) msg += " " + std::to_string(found) + " landmark" + (found > 1 ? "s" : "") + " revealed from the summit.";
    actions::notify(msg);
    audio::victoryFanfare();
  });
}

std::string promptText() {
  const Vec3& p = E.player.pos;
  const Shrine* sh = temple::nearShrine(p.x, p.z, 12);
  if (sh && p.y > sh->y) return "[E] Pray at the " + sh->name + " temple";
  for (auto& G : world::GAPS) {
    float ox = p.x - G.rx, oz = p.z - G.rz;
    if (ox * ox + oz * oz < 30) return G.pillars ? "Stepping pillars ahead: jump from one to the next [Space]" : "Gap ahead: sprint [Shift] and jump [Space] at the edge";
  }
  if (wildlife::findCarcass(p.x, p.z, 3.2f)) return "[E] Harvest";
  NpcRt* npc = npcs::find(p.x, p.z, 3.6f);
  ResNode* node = resources::find(p.x, p.z, 3.0f);
  if (node && !(npc && hypot2(npc->x - p.x, npc->z - p.z) < hypot2(node->x - p.x, node->z - p.z) + 0.8f))
    return std::string("[E] Gather ") + RES_INFO[node->type].name + " (" + std::to_string(node->left) + " left)";
  if (npc) return "[E] Talk to " + npc->name;
  for (auto& c : A.clues)
    if (!c.discovered && hypot2(c.x - p.x, c.z - p.z) < 4.5f) return "[E] Inspect " + c.title;
  if (invCount("arrow") <= 0) return "Out of arrows: press [R] to craft 10 from 1 timber";
  return "";
}

}  // namespace

namespace player {

void reset() {
  g_feetY = world::getTerrainHeight(0, 9);
  g_eyeH = 1.65f;
  g_vel = {0, 0, 0};
  g_lmb = g_rmb = false;
  g_synced.clear();
}

Mat4 cameraWorld() { return g_camWorld; }
Vec3 cameraForward() { return g_fwd; }
float fov() { return g_fov; }

void releaseBow() {
  auto& B = E.bow;
  if (!B.drawing) return;
  B.drawing = false;
  if (B.draw > 0.05f && invCount("arrow") > 0 && B.cooldown <= 0) {
    archery::fire(render::cam.pos, g_fwd, std::max(0.3f, B.draw), B.aiming);
    actions::addItems({{"arrow", -1}});
    B.cooldown = 0.4f;
    B.lastShotPower = B.draw;
    audio::tone(Wave::Triangle, 240, 70, 0.12f, 0.14f);
  }
  B.draw = 0;
}

void jump() {
  auto& P = E.player;
  if (P.onGround) { P.velY = 6.6f; P.onGround = false; P.crouching = false; }
}

void tryPlace() {
  auto& w = E.world;
  if (w.placing.empty()) return;
  if (!w.previewValid) {
    actions::notify(w.previewReason.empty() ? "Cannot build here" : w.previewReason);
    audio::crispClick();
    return;
  }
  actions::placeBuilding(w.placing, w.previewPos, w.previewRot);
}

void interact() {
  const Vec3& p = E.player.pos;
  const Shrine* sh = temple::nearShrine(p.x, p.z, 12);
  if (sh && p.y > sh->y) { startSync(*sh); return; }
  if (Animal* carcass = wildlife::findCarcass(p.x, p.z, 3.2f)) {
    CarcassYield r = wildlife::harvestCarcass(carcass);
    actions::addItems(r.yields);
    actions::addScore(120);
    popupItems(r.yields);
    audio::coinPickup();
    std::string nm = r.name;
    for (auto& c : nm) c = (char)std::tolower((unsigned char)c);
    actions::notify("Harvested the " + nm + ".");
    return;
  }
  NpcRt* npc = npcs::find(p.x, p.z, 3.6f);
  ResNode* node = resources::find(p.x, p.z, 3.0f);
  if (node && npc && hypot2(npc->x - p.x, npc->z - p.z) < hypot2(node->x - p.x, node->z - p.z) + 0.8f) node = nullptr;
  if (node) {
    HarvestResult r = resources::harvest(*node);
    actions::addItems({{r.item, r.amount}});
    actions::addScore(15);
    popupItems({{r.item, r.amount}});
    if (node->type == RES_WOOD) audio::footstepWood();
    else if (node->type == RES_STONE) audio::footstepStone();
    else audio::tone(Wave::Sine, 660, 990, 0.12f, 0.07f);
    if (r.depleted) actions::notify(r.name + " gathered clean. It will regrow in a while.");
    return;
  }
  if (npc) {
    actions::openDialogue(npc->id);
    audio::confirmSelect();
    return;
  }
  for (auto& c : A.clues)
    if (!c.discovered && hypot2(c.x - p.x, c.z - p.z) < 4.5f) {
      actions::discoverClue(c.id);
      audio::powerupChime();
      return;
    }
  actions::notify("Nothing to gather here. Follow the compass icons to timber, slate, nettle and herbs.");
  audio::crispClick();
}

void onKeyPressed(int key) {
  if (!E.playing) return;
  auto& w = E.world;
  if (key == K_TAB || key == K_I) { actions::openTab(Tab::Inventory); return; }
  if (key == K_J) { actions::openTab(Tab::Quests); return; }
  if (key == K_B) { actions::openTab(Tab::Buildings); return; }
  if (key == K_M) { actions::openTab(Tab::Map); return; }
  if (E.uiOpen) return;
  if (!w.placing.empty()) {
    if (key == K_Q) w.previewRot -= PI / 8;
    if (key == K_E || key == K_R) w.previewRot += PI / 8;
    if (key == K_ESC) actions::cancelPlacement();
    if (key == K_ENTER) tryPlace();
    int slot = key >= K_1 && key <= K_6 ? key - K_1 : -1;
    if (slot >= 0 && w.placeKind != SK_COUNT) actions::cycleLook(slot);
    return;
  }
  if (key == K_E) interact();
  if (key == K_C || key == K_CTRL) E.player.crouching = !E.player.crouching;
  if (key == K_SPACE) jump();
  if (key == K_F) { E.bow.drawing = true; E.bow.draw = 0.8f; releaseBow(); }
}

void onMouseDown(int button) {
  if (!E.playing || E.uiOpen) return;
  if (button == 0) {
    if (!E.world.placing.empty()) { tryPlace(); return; }
    g_lmb = true;
    if (invCount("arrow") <= 0) { actions::notify("Out of arrows! Press [R] to craft 10 from 1 timber."); return; }
    E.bow.drawing = true;
  } else if (button == 1) {
    if (!E.world.placing.empty()) { actions::cancelPlacement(); return; }
    g_rmb = true;
  }
}

void onMouseUp(int button) {
  if (button == 0) { g_lmb = false; releaseBow(); }
  if (button == 1) g_rmb = false;
}

void update(float dt) {
  Input& in = platform::input();
  auto& CS = E.cutscene;
  if (CS.dur > 0) {
    CS.t += dt;
    float u = std::min(1.0f, CS.t / CS.dur);
    float e = u < 0.5f ? 2 * u * u : 1 - std::pow(-2 * u + 2, 2.0f) / 2;
    float arc = std::sin(u * PI);
    float ang = CS.yaw + e * TAU;
    float R = 18 + arc * 46;
    Vec3 cp(CS.x + std::sin(ang) * R, CS.y + 8 + arc * 26, CS.z + std::cos(ang) * R);
    Vec3 tgt(CS.x, CS.y + 11 - arc * 8, CS.z);
    Mat4 view = Mat4::lookAt(cp, tgt, {0, 1, 0});
    g_camWorld = view.inverse();
    g_fwd = (tgt - cp).normalized();
    if (u >= 1) {
      CS.dur = 0;
      E.player.yaw = CS.yaw;
      E.player.pitch = -0.05f;
    }
    return;
  }
  E.clock += dt;
  auto& P = E.player;
  auto& B = E.bow;
  bool active = E.playing && !E.uiOpen;
  if (!active) { g_lmb = false; g_rmb = false; if (B.drawing) releaseBow(); }

  if (E.playing) {
    float h = E.time;
    bool twilight = (h >= 4.0f && h < 8.0f) || (h >= 16.5f && h < 21.5f);
    bool night = h >= 21.5f || h < 4.0f;
    float next = E.time + dt * (twilight ? 0.022f : night ? 0.1f : 0.04f);
    if (next >= 24) nextDay();
    E.time = std::fmod(next, 24.0f);
  }

  if (active && in.captured) {
    float sens = g_settings.mouseSensitivity * (B.aiming ? 0.55f : 1);
    P.yaw -= in.dx * sens;
    P.pitch = clampf(P.pitch - in.dy * sens, -1.35f, 1.35f);
  }

  B.aiming = active && E.world.placing.empty() && g_rmb;
  B.cooldown = std::max(0.0f, B.cooldown - dt);
  if (B.drawing && active && invCount("arrow") > 0 && B.cooldown <= 0) B.draw = std::min(1.0f, B.draw + dt / 0.7f);
  else if (!B.drawing) B.draw = std::max(0.0f, B.draw - dt * 4);

  float mz = 0, mx = 0;
  if (active) {
    mz = (in.down[K_W] || in.down[K_UP] ? 1.0f : 0.0f) - (in.down[K_S] || in.down[K_DOWN] ? 1.0f : 0.0f);
    mx = (in.down[K_D] || in.down[K_RIGHT] ? 1.0f : 0.0f) - (in.down[K_A] || in.down[K_LEFT] ? 1.0f : 0.0f);
  }
  P.sprinting = active && in.down[K_SHIFT] && mz > 0 && !P.crouching && !B.drawing;
  Vec3 fwd(-std::sin(P.yaw), 0, -std::cos(P.yaw));
  Vec3 side(std::cos(P.yaw), 0, -std::sin(P.yaw));
  Vec3 move = fwd * mz + side * mx;
  P.moving = move.lengthSq() > 0.01f;
  if (move.lengthSq() > 1) move.normalize();
  float speed = g_settings.playerSpeed;
  if (P.sprinting) speed *= g_settings.sprintMultiplier;
  if (P.crouching) speed *= 0.45f;
  if (B.drawing || B.aiming) speed *= 0.6f;
  float ground0 = walkHeight(P.pos.x, P.pos.z);
  if (ground0 < world::waterHeightAt(P.pos.x, P.pos.z) - 0.2f) speed *= 0.6f;
  g_vel = Vec3::lerp(g_vel, move * speed, std::min(1.0f, dt * (P.onGround ? 12 : 7)));

  float px0 = P.pos.x, pz0 = P.pos.z;
  P.pos.x += (g_vel.x + P.knock.x) * dt;
  P.pos.z += (g_vel.z + P.knock.z) * dt;
  P.knock *= std::max(0.0f, 1 - dt * 5);
  { int pi_ = forest::profOpen ? forest::profOpen("treeCollision") : -1; forest::resolveTreeCollision(P.pos.x, P.pos.z, 0.38f); bridgeRailCollision(P.pos.x, P.pos.z, P.pos.y - 1.7f); if (forest::profClose) forest::profClose(pi_); }
  resolveBuildingCollision(P.pos.x, P.pos.z, 0.38f);
  P.pos.x = clampf(P.pos.x, -MAP_HALF + 4, MAP_HALF - 4);
  P.pos.z = clampf(P.pos.z, -MAP_HALF + 4, MAP_HALF - 4);

  float ground = walkHeight(P.pos.x, P.pos.z);
  bool onTerrain = ground <= world::getTerrainHeight(P.pos.x, P.pos.z) + 0.02f;
  float g0 = walkHeight(px0, pz0);
  float moved = hypot2(P.pos.x - px0, P.pos.z - pz0);
  float grade = (ground - g0) / (moved > 0 ? moved : 1e-4f);
  if (!P.onGround && !onTerrain && ground > g_feetY && ground - g_feetY <= 0.4f) {
    g_feetY = ground;
    if (P.velY > 0) P.velY = 0;
  } else if (ground > g_feetY + (P.onGround ? 0.45f : 0.06f) || (!P.onGround && onTerrain && ground > g_feetY - 0.25f && grade > 1.3f) || (P.onGround && onTerrain && grade > 0.95f)) {
    P.pos.x = px0;
    P.pos.z = pz0;
    if (P.onGround) g_vel *= 0.6f;
    ground = g0;
  }
  if (P.onGround && ground <= world::getTerrainHeight(P.pos.x, P.pos.z) + 0.02f) {
    float sl = world::localSlope(P.pos.x, P.pos.z);
    if (sl > 0.95f) {
      float gx = world::getTerrainHeight(P.pos.x + 0.6f, P.pos.z) - world::getTerrainHeight(P.pos.x - 0.6f, P.pos.z);
      float gz = world::getTerrainHeight(P.pos.x, P.pos.z + 0.6f) - world::getTerrainHeight(P.pos.x, P.pos.z - 0.6f);
      float gl = hypot2(gx, gz);
      if (gl < 1e-6f) gl = 1;
      float push = std::min(7.0f, (sl - 0.95f) * 6 + 2.5f) * dt;
      P.pos.x -= (gx / gl) * push;
      P.pos.z -= (gz / gl) * push;
      ground = walkHeight(P.pos.x, P.pos.z);
    }
  }
  P.velY += g_settings.gravity * dt;
  g_feetY += P.velY * dt;
  if (g_feetY <= ground || (P.onGround && g_feetY - ground < 0.5f && P.velY <= 0)) {
    g_feetY = ground;
    P.velY = 0;
    P.onGround = true;
  } else P.onGround = false;
  if (P.onGround) {
    const Gap* G = world::gapAt(P.pos.x, P.pos.z, g_feetY);
    if (G) {
      P.pos.x = G->rx;
      P.pos.z = G->rz;
      P.yaw = G->yaw;
      g_feetY = walkHeight(G->rx, G->rz);
      g_vel = {0, 0, 0};
      P.hp = std::max(5.0f, P.hp - 10);
      actions::notify(G->pillars ? "You slipped off the pillars. Take a running jump from one to the next." : "You fell into the ravine. Sprint [Shift] and jump [Space] right at the edge.");
      audio::tone(Wave::Sine, 300, 120, 0.35f, 0.12f);
    }
  }
  g_eyeH += ((P.crouching ? 1.05f : 1.65f) - g_eyeH) * std::min(1.0f, dt * 10);
  float headBob = P.moving && P.onGround ? std::sin(E.clock * (P.sprinting ? 12 : 8.5f)) * (P.sprinting ? 0.05f : 0.025f) : 0;
  P.pos.y = g_feetY + g_eyeH + headBob;

  if (P.moving && P.onGround && active) {
    g_footT += dt * (P.sprinting ? 3.3f : P.crouching ? 1.2f : 2.0f);
    if (g_footT >= 1) {
      g_footT = 0;
      if (ground < world::waterHeightAt(P.pos.x, P.pos.z) + 0.2f || ground > 60) audio::footstepStone();
      else if (!P.crouching) audio::footstepWood();
    }
  }

  Quat q = Quat::fromEulerYXZ(P.pitch, P.yaw, 0);
  g_camWorld = Mat4::compose(P.pos, q, {1, 1, 1});
  g_fwd = q.rotate({0, 0, -1});
  float target = B.aiming ? 42 : g_settings.fov;
  if (std::fabs(g_fov - target) > 0.05f) g_fov += (target - g_fov) * std::min(1.0f, dt * 12);

  auto& w = E.world;
  if (!w.placing.empty() && w.placeKind != SK_COUNT) {
    float reach = 3.5f + structureRadius(w.placeKind);
    SpotCheck r = checkStructureSpot(w.placeKind, P.pos.x + fwd.x * reach, P.pos.z + fwd.z * reach, w.previewRot, w.placeUid);
    w.previewPos = {r.x, r.y, r.z};
    w.previewValid = r.ok;
    w.previewReason = r.reason;
  } else if (!w.placing.empty()) {
    for (auto& b : A.buildings) {
      if (b.id != w.placing) continue;
      float reach = b.type == "bridge" ? 9 : 7.5f;
      PlacementCheck r = checkPlacement(b, P.pos.x + fwd.x * reach, P.pos.z + fwd.z * reach, w.previewRot);
      w.previewPos = {r.x, r.y, r.z};
      if (b.type == "bridge") w.previewRot = r.rot;
      w.previewValid = r.ok;
      w.previewReason = r.reason;
    }
  }

  archery::update(dt);
  { int pi_ = forest::profOpen ? forest::profOpen("wildlife.update") : -1; wildlife::update(dt); if (forest::profClose) forest::profClose(pi_); }
  { int pi_ = forest::profOpen ? forest::profOpen("npcs.update") : -1; npcs::update(dt); if (forest::profClose) forest::profClose(pi_); }
  { int pi_ = forest::profOpen ? forest::profOpen("village.residents") : -1; village::updateResidents(dt); if (forest::profClose) forest::profClose(pi_); }
  props::checkSummits();

  g_slowT += dt;
  if (g_slowT > 0.25f) {
    g_slowT = 0;
    E.nearbyPrompt = active ? promptText() : "";
    E.questTarget = computeQuestTarget();
    resources::update();
  }
}

void updateCamera(float aspect) {
  render::cam.fov = g_fov;
  render::cam.aspect = aspect;
  render::cam.view = g_camWorld.inverse();
  render::cam.pos = Vec3(g_camWorld.m[12], g_camWorld.m[13], g_camWorld.m[14]);
  render::cam.proj = Mat4::perspective(g_fov, aspect, render::cam.nearZ, render::cam.farZ);
  render::cam.update();
}

}  // namespace player
