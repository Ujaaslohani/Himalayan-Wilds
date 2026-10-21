#include "app.h"
#include "season.h"
#include "village.h"
#include "resources.h"
#include "wildlife.h"
#include "npcs.h"
#include "player.h"
#include "props.h"
#include "archery.h"
#include "buildings.h"
#include "../models/houses.h"
#include "../models/models.h"
#include "../world/terrain.h"
#include "../world/forest.h"
#include "../platform/platform.h"
#include "../audio/audio.h"
#include "../core/json.h"
#include <ctime>

const int UNLOCK[SK_COUNT] = {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0};

namespace {

const Cost START_COUNTS = {{"bow", 1}, {"arrow", 30}, {"wood", 12}, {"stone", 8}, {"fiber", 6}, {"herb", 2}, {"food", 20}};

std::vector<Specialist> initialSpecialists() {
  return {
      {"hunter", "Kalyan the Tracker", "Master Hunter", "Hunter", true, "The village",
       "A herd grazes in the open meadows near the village. Crouch [C] and come in slowly — they hear a sprinting man from forty paces. Draw the bow fully for a clean kill.",
       "Build his Lodge to double pelt yields and track wildlife on your radar."},
      {"carpenter", "Devan the Woodworker", "Carpenter", "Carpenter", false, npcs::camp("carpenter")->area,
       "You want a real village? Bring me 20 timber and 10 nettle fibre and I will frame Kath-Kuni houses the old way.", "Felled logs regrow 40% faster."},
      {"herbalist", "Meera of the Valleys", "Herbalist", "Herbalist", false, npcs::camp("herbalist")->area,
       "The purple artemisia blooms all over these meadows. Gather 8 bundles for me and I will tend your people.", "Herb bushes regrow 40% faster."},
      {"builder", "Gopal Stonecarver", "Master Builder", "Builder", false, npcs::camp("builder")->area,
       "These ruins were raised by my ancestors. Bring 30 slate and 4 deer pelts and together we will bridge the river.", "Slate outcrops regrow 40% faster."},
  };
}

std::vector<Building> initialBuildings() {
  std::vector<Building> v;
  auto add = [&](const char* id, const char* name, int maxLevel, float x, float z, float rot, Cost cost, bool built, const char* desc) {
    Building b;
    b.id = id;
    b.type = id;
    b.name = name;
    b.level = built ? 1 : 0;
    b.maxLevel = maxLevel;
    b.pos[0] = x; b.pos[1] = 0.1f; b.pos[2] = z;
    b.rotY = rot;
    b.cost = cost;
    b.built = built;
    b.desc = desc;
    v.push_back(b);
  };
  add("campfire", "Village Hearth", 1, 0, 0, 0, {}, true, "The heart of the settlement.");
  add("hunter_hut", "Hunter's Lodge", 2, 6, -4, 0.2f, {{"wood", 15}, {"fiber", 6}, {"stone", 6}}, false, "Fletches arrows for you and doubles pelt yields.");
  add("carpenter_workshop", "Carpenter's Workshop", 2, -8, 6, -0.3f, {{"wood", 22}, {"fiber", 8}, {"stone", 10}}, false, "Devan saws timber for the village stockpile.");
  add("herbalist_lodge", "Herbalist's Sanctuary", 2, 10, 8, -0.6f, {{"wood", 12}, {"stone", 10}, {"herb", 6}}, false, "Meera grows herbs and twists nettle fibre.");
  add("watchtower", "Mountain Watchtower", 2, -6, -10, 0.8f, {{"wood", 28}, {"stone", 18}, {"fiber", 10}}, false, "Spots wildlife across the valley and quarries slate.");
  add("bridge", "River Bridge", 1, world::BRIDGE_SITE[0], world::BRIDGE_SITE[1], 0, {{"wood", 40}, {"stone", 25}, {"fiber", 15}, {"deer_pelt", 4}}, false, "Reopens the old trade trail across the river.");
  v.back().pos[1] = 0;
  return v;
}

std::vector<Quest> initialQuests() {
  return {
      {"ch1", 1, "Chapter 1: The First Hunt", "Kalyan the Tracker", "Follow the gold objective marker to the herd near the village. Crouch, draw fully, and bring back pelts.", "Collect 2 Deer Pelts", 0, 2, false, "+500 renown."},
      {"ch2", 2, "Chapter 2: A Place to Call Home", "Kalyan the Tracker", "Gather timber, slate and nettle (follow the compass icons), then open Build [B] and raise the Hunter's Lodge.", "Build the Hunter's Lodge", 0, 1, false, "The lodge now fletches arrows for you."},
      {"ch3", 3, "Chapter 3: Knowledge Returns", "Kalyan the Tracker", "Two specialists camp in the wilds. Find Devan and Meera (gold \"!\" markers) and bring what they ask for.", "Recruit the Carpenter & Herbalist", 0, 2, false, "Better yields from timber and herbs."},
      {"ch4", 4, "Chapter 4: Old Paths", "Meera of the Valleys", "Prayer-flag cairns mark old landmarks across the valley. Discover any three of them.", "Discover 3 landmarks", 0, 3, false, "+500 renown."},
      {"ch5", 5, "Chapter 5: The Healer's House", "Meera of the Valleys", "Build the Herbalist's Sanctuary so Meera can grow herbs and fibre for the village.", "Build the Herbalist's Sanctuary", 0, 1, false, "Passive herb & fibre income."},
      {"ch6", 6, "Chapter 6: A Village Reborn", "Devan the Woodworker", "Raise the Carpenter's Workshop and the Mountain Watchtower.", "Build the Workshop & Watchtower", 0, 2, false, "Timber & slate income, wildlife on radar."},
      {"ch7", 7, "Chapter 7: Across the River", "Gopal Stonecarver", "The old trade trail crosses the river west of the village. Build the bridge where the river narrows.", "Build the River Bridge", 0, 1, false, "The trade trail is open again."},
      {"ch8", 8, "Chapter 8: A Living Mountain Village", "Village Elders", "Recruit all four specialists, build five structures and earn 2,000 renown.", "All specialists · 5 buildings · 2,000 renown", 0, 1, false, "VICTORY! The Himalayan haven endures."},
  };
}

std::vector<Clue> initialClues() {
  const P2& hd = world::RIVER.head;
  return {
      {"track_1", "deer_tracks", "Fresh Hoofprints", "Deep prints in damp soil heading toward the open meadows.", 25, -30, false},
      {"track_2", "deer_tracks", "Grazing Scrape", "Bark stripped from young rhododendron. A herd feeds nearby.", -91, 32, false},
      {"ruins_1", "ruins_marker", "Ancient Stone Inscription", "A weathered tablet describing the Kath-Kuni masonry of the ancestors.", 28, -134, false},
      {"viewpoint_1", "viewpoint", "Nanda Overlook", "A sweeping view over the valley and the snowline beyond.", 114, -51, false},
      {"falls_1", "viewpoint", "Glacier Falls", "Meltwater thunders off the cliff — the source of the valley river.", hd[0] + 13, hd[1] + 16, false},
  };
}

const char* SETTLER_NAMES[20] = {"Asha", "Bhim", "Chandni", "Dorje", "Gauri", "Hari", "Ishaan", "Jamuna", "Kesang", "Lata", "Mohan", "Nima", "Parvati", "Ramesh", "Sonam", "Tara", "Uma", "Vikram", "Yangchen", "Zara"};
const std::vector<std::vector<DecoKey>> DECO_SETS = {{}, {DK_POTS}, {DK_POTS, DK_LANTERN}, {DK_FIREWOOD, DK_BENCH}, {DK_BARRELS, DK_FENCE}, {DK_CLOTH, DK_LANTERN, DK_POTS}, {DK_FIREWOOD, DK_BARRELS, DK_BENCH, DK_LANTERN}};

std::string costText(const Cost& c) {
  std::string s;
  for (auto& e : c) {
    if (!s.empty()) s += ", ";
    s += std::to_string(e.second) + " " + itemLabel(e.first);
  }
  return s;
}

std::string savePath() { return platform::saveDir() + "/save.json"; }

void resetWorld() {
  wildlife::spawn(7);
  resources::spawn(11);
  npcs::reset();
  village::resetResidents();
  archery::reset();
  props::resetSummits();
  auto& P = E.player;
  P.pos = {0, 1.7f, 9};
  P.yaw = 0;
  P.pitch = -0.05f;
  P.velY = 0;
  P.crouching = false;
  P.knock = {0, 0, 0};
  P.maxHp = 100;
  P.blessed.clear();
  P.hp = 100;
  E.festival.t = 0;
  E.raid.active = false;
  P.hurtT = -99;
  E.bow.draw = 0;
  E.bow.drawing = false;
  E.time = 7.5f;
  E.cutscene.dur = 0;
  E.events.clear();
  player::reset();
}

void blockersChanged() {
  std::vector<Blocker> list;
  for (auto& b : A.buildings) {
    if (!b.built) continue;
    list.push_back({b.pos[0], b.pos[2], b.type == "bridge" ? 11.0f : 6.0f});
  }
  for (auto& n : resources::nodes()) list.push_back({n.x, n.z, n.type == RES_WOOD ? 2.6f : 1.8f});
  for (auto& c : npcs::CAMPS) list.push_back({c.campX, c.campZ, 6});
  for (auto& c : A.clues) list.push_back({c.x, c.z, 4});
  for (auto& s : A.structures) {
    bool small = s.kind == SK_FENCE || s.kind == SK_LAMP_POST || s.kind == SK_BENCH || s.kind == SK_FLAGPOLE;
    list.push_back({s.x, s.z, s.kind == SK_PATH ? 1.3f : structureRadius(s.kind) + (small ? 0.8f : 2.5f)});
  }
  list.push_back({0, 9, 3});
  forest::setBlockers(list);
}

int g_lastStage = 0;
float g_questT = 0;

void worldChanged() {
  blockersChanged();
  village::structuresChanged();
  props::rebuildCasters();
  app::syncDerived();
}

void updateQuests() {
  if (A.status != Status::Playing) return;
  auto built = [](const char* id) {
    for (auto& b : A.buildings)
      if (b.id == id) return b.built ? 1 : 0;
    return 0;
  };
  int builtCount = 0;
  for (auto& b : A.buildings) if (b.built) builtCount++;
  bool allRec = true;
  for (auto& s : A.specialists) if (!s.recruited) allRec = false;
  for (auto& q : A.quests) {
    if (q.completed) continue;
    int cur = 0;
    if (q.id == "ch1") cur = invCount("deer_pelt");
    else if (q.id == "ch2") cur = built("hunter_hut");
    else if (q.id == "ch3") { for (auto& s : A.specialists) if (s.recruited && (s.id == "carpenter" || s.id == "herbalist")) cur++; }
    else if (q.id == "ch4") { for (auto& c : A.clues) if (c.discovered) cur++; }
    else if (q.id == "ch5") cur = built("herbalist_lodge");
    else if (q.id == "ch6") cur = built("carpenter_workshop") + built("watchtower");
    else if (q.id == "ch7") cur = built("bridge");
    else if (q.id == "ch8") cur = allRec && builtCount >= 6 && A.score >= 2000 ? 1 : 0;
    bool done = cur >= q.target;
    if (done) {
      audio::levelUpStinger();
      actions::notify("Quest complete: " + q.title + "! " + q.reward);
      A.score += 500;
      A.level = std::min(8, A.level + 1);
      if (q.id == "ch8") A.victoryAt = E.clock + 2;
    }
    q.progress = std::min(cur, q.target);
    q.completed = done;
  }
}

void secondTick() {
  A.jobAcc += 1;
  if (A.jobAcc >= JOB_EVERY) {
    A.jobAcc = 0;
    std::map<std::string, float> give;
    float k = A.hungerDays > 0 ? 0.5f : 1;
    for (int j = 0; j < JOB_COUNT; j++) {
      int n = A.jobs[j];
      if (!n) continue;
      for (auto& g : JOBS[j].give) give[g.first] += g.second * n * seasonOutput(g.first) * k;
    }
    if (!give.empty()) {
      Cost out;
      std::string text = "Villagers:";
      for (auto& g : give) {
        int v = std::max(1, (int)std::lround(g.second));
        out.push_back({g.first, v});
        text += (out.size() > 1 ? ", +" : " +") + std::to_string(v) + " " + itemLabel(g.first);
      }
      actions::addItems(out);
      popup(text, rgbHex("#bef264"));
    }
  }
  float h = E.time;
  bool dawn = A.lastHour < 6 && h >= 6;
  A.lastHour = h;
  int pop = A.population;
  if (dawn && pop > 0) {
    int need = dailyNeed(pop, season.index == 3);
    int have = invCount("food");
    if (have >= need) {
      actions::addItems({{"food", -need}});
      if (A.hungerDays > 0) actions::notify("Everyone ate well this morning. The village is no longer hungry.");
      A.hungerDays = 0;
      int left = have - need;
      if (left < need) actions::notify("Food is running low: " + std::to_string(left) + " left and " + std::to_string(need) + " needed tomorrow. Hunt, assign hunters, or trade at the market.");
    } else {
      actions::addItems({{"food", -have}});
      A.hungerDays += 1;
      if (A.hungerDays >= 2) {
        A.population = std::max(0, A.population - 1);
        actions::notify("A hungry family packed up and left the village. Find food quickly!");
      } else actions::notify("Not enough food this morning (" + std::to_string(have) + "/" + std::to_string(need) + "). The village is hungry: work slows and no one new will arrive.");
    }
  }
  int cap = app::capacity();
  if (A.population >= cap || A.hungerDays > 0) A.arrivalAcc = 0;
  else {
    A.arrivalAcc += 1;
    float wait = std::max(12.0f, std::min(45.0f, 40 - villageAppeal() * 1.5f + A.population * 0.6f)) * settlerPace();
    if (A.arrivalAcc >= wait) {
      A.arrivalAcc = 0;
      A.population += 1;
      std::string name = SETTLER_NAMES[(int)(randf() * 20) % 20];
      actions::notify(name + " has arrived and moved in. Residents: " + std::to_string(A.population) + "/" + std::to_string(cap) + ".");
      popup("New settler", rgbHex("#7dd3fc"));
      actions::addScore(40);
    }
  }
  for (auto& b : A.buildings) {
    const Production* P = production(b.id);
    if (!P || !b.built) continue;
    float& acc = A.prodAcc[b.id];
    acc += 1;
    if (acc >= P->every) {
      acc = 0;
      Cost give;
      std::string text = b.name + ":";
      for (auto& g : P->give) {
        int v = std::max(1, (int)std::lround(g.second * std::max(1, b.level) * seasonOutput(g.first)));
        give.push_back({g.first, v});
        text += (give.size() > 1 ? ", +" : " +") + std::to_string(v) + " " + itemLabel(g.first);
      }
      actions::addItems(give);
      popup(text, rgbHex("#a7f3d0"));
    }
  }
}

Json costJson(const Cost& c) {
  Json o = Json::object();
  for (auto& e : c) o.set(e.first, Json::number(e.second));
  return o;
}

Json lookJson(const Look& l) {
  Json o = Json::object();
  o.set("roof", Json::number(l.roof));
  o.set("wall", Json::number(l.wall));
  o.set("wood", Json::number(l.wood));
  o.set("door", Json::number(l.door));
  o.set("accent", Json::number(l.accent));
  o.set("deco", Json::number(l.deco));
  return o;
}

Look lookFrom(const Json* j) {
  Look l;
  if (!j) return l;
  l.roof = (int)j->num("roof", 0);
  l.wall = (int)j->num("wall", 0);
  l.wood = (int)j->num("wood", 0);
  l.door = (int)j->num("door", 0);
  l.accent = (int)j->num("accent", 1);
  l.deco = (int)j->num("deco", 1);
  return l;
}

Json buildSave() {
  Json d = Json::object();
  d.set("v", Json::number(1));
  d.set("at", Json::number((double)std::time(nullptr)));
  d.set("score", Json::number(A.score));
  d.set("level", Json::number(A.level));
  Json inv = Json::object();
  for (auto& e : A.inv) inv.set(e.first, Json::number(e.second));
  d.set("inventory", inv);
  Json bs = Json::array();
  for (auto& b : A.buildings) {
    Json o = Json::object();
    o.set("id", Json::string(b.id));
    o.set("built", Json::boolean(b.built));
    o.set("level", Json::number(b.level));
    Json p = Json::array();
    for (float v : b.pos) p.push(Json::number(v));
    o.set("position", p);
    o.set("rotationY", Json::number(b.rotY));
    bs.push(o);
  }
  d.set("buildings", bs);
  Json rec = Json::array();
  for (auto& s : A.specialists) if (s.recruited) rec.push(Json::string(s.id));
  d.set("recruited", rec);
  Json qs = Json::array();
  for (auto& q : A.quests) {
    Json o = Json::object();
    o.set("id", Json::string(q.id));
    o.set("completed", Json::boolean(q.completed));
    qs.push(o);
  }
  d.set("quests", qs);
  Json cl = Json::array();
  for (auto& c : A.clues) if (c.discovered) cl.push(Json::string(c.id));
  d.set("clues", cl);
  Json st = Json::array();
  for (auto& s : A.structures) {
    Json o = Json::object();
    o.set("uid", Json::number(s.uid));
    o.set("kind", Json::string(KINDS[s.kind].id));
    o.set("x", Json::number(s.x));
    o.set("y", Json::number(s.y));
    o.set("z", Json::number(s.z));
    o.set("rot", Json::number(s.rot));
    o.set("look", lookJson(s.look));
    st.push(o);
  }
  d.set("structures", st);
  d.set("uid", Json::number(A.uid));
  d.set("population", Json::number(A.population));
  Json jobs = Json::object();
  for (int j = 0; j < JOB_COUNT; j++) jobs.set(JOBS[j].key, Json::number(A.jobs[j]));
  d.set("jobs", jobs);
  d.set("hungerDays", Json::number(A.hungerDays));
  d.set("stage", Json::number(A.stage));
  Json se = Json::object();
  se.set("start", Json::number(season.start));
  se.set("day", Json::number(season.day));
  d.set("season", se);
  d.set("time", Json::number(E.time));
  Json pl = Json::object();
  Json pos = Json::array();
  pos.push(Json::number(E.player.pos.x));
  pos.push(Json::number(E.player.pos.y));
  pos.push(Json::number(E.player.pos.z));
  pl.set("pos", pos);
  pl.set("yaw", Json::number(E.player.yaw));
  pl.set("hp", Json::number(E.player.hp));
  pl.set("maxHp", Json::number(E.player.maxHp));
  Json bl = Json::array();
  for (auto& b : E.player.blessed) bl.push(Json::string(b));
  pl.set("blessed", bl);
  d.set("player", pl);
  return d;
}

bool readSave(Json& out) {
  std::vector<uint8_t> bytes;
  if (!platform::readFile(savePath(), bytes)) return false;
  if (!Json::parse(std::string(bytes.begin(), bytes.end()), out)) return false;
  return out.num("v", 0) == 1;
}

void flushTimers() {
  for (size_t i = 0; i < A.timers.size();) {
    if (E.clock >= A.timers[i].first) {
      auto fn = A.timers[i].second;
      A.timers.erase(A.timers.begin() + i);
      fn();
    } else i++;
  }
}

}  // namespace

namespace actions {

void addItems(const Cost& delta) {
  for (auto& d : delta) A.inv[d.first] = std::max(0, A.inv[d.first] + d.second);
  int fcap = foodCap();
  if (A.inv["food"] > fcap) A.inv["food"] = fcap;
  E.world.gearCoat = invCount("fur_coat") > 0;
  E.world.gearArmour = invCount("leather_armour") > 0;
}

void addScore(int n) { A.score += n; }

void notify(const std::string& text) {
  A.notification = text;
  A.notifyAt = platform::time();
}

void openDialogue(const std::string& id) {
  for (size_t i = 0; i < A.specialists.size(); i++)
    if (A.specialists[i].id == id) A.dialogue = (int)i;
}

void discoverClue(const std::string& id) {
  for (auto& c : A.clues) {
    if (c.id != id || c.discovered) continue;
    c.discovered = true;
    addScore(100);
    popup("Landmark discovered", rgbHex("#5eead4"));
    notify(c.title + ": " + c.desc);
  }
}

void placeBuilding(const std::string& id, Vec3 pos, float rot) {
  auto& w = E.world;
  if (id.rfind("s:", 0) == 0 || id.rfind("m:", 0) == 0) {
    if (!w.hasPlaceLook) return;
    Look look = w.placeLook;
    if (id.rfind("m:", 0) == 0) {
      int uid = std::stoi(id.substr(2));
      for (auto& s : A.structures) {
        if (s.uid != uid) continue;
        s.x = pos.x; s.y = pos.y; s.z = pos.z; s.rot = rot; s.look = look;
        resources::clearAround(pos.x, pos.z, structureRadius(s.kind) + 2.5f);
        notify(std::string(KINDS[s.kind].name) + " moved.");
        audio::confirmSelect();
        cancelPlacement();
        worldChanged();
        return;
      }
      return;
    }
    StructureKind kind = kindFromId(id.substr(2));
    if (kind == SK_COUNT) return;
    const KindInfo& info = KINDS[kind];
    if (!app::has(info.cost)) {
      notify(std::string("Not enough materials for the ") + info.name + ". Still need: " + app::shortfall(info.cost) + ".");
      audio::crispClick();
      cancelPlacement();
      return;
    }
    app::spend(info.cost);
    Structure s;
    s.uid = A.uid++;
    s.kind = kind;
    s.x = pos.x; s.y = pos.y; s.z = pos.z; s.rot = rot; s.look = look;
    A.structures.push_back(s);
    resources::clearAround(pos.x, pos.z, structureRadius(kind) + (info.group == KG_DECOR ? 0.8f : 2.5f));
    worldChanged();
    if (info.group == KG_DECOR) {
      audio::crispClick();
      bool twice = true;
      for (auto& c : info.cost) if (invCount(c.first) < c.second * 2) twice = false;
      if (!twice) cancelPlacement();
      return;
    }
    addScore(100 + info.appeal * 25);
    audio::levelUpStinger();
    notify(info.capacity ? std::string(info.name) + " built! It has " + std::to_string(info.capacity) + " beds. Settlers will arrive over time." : std::string(info.name) + " built! The village grows more appealing.");
    cancelPlacement();
    return;
  }
  for (auto& b : A.buildings) {
    if (b.id != id) continue;
    bool wasBuilt = b.built;
    if (!b.built) {
      if (!app::has(b.cost)) {
        notify("Not enough materials for the " + b.name + ". Still need: " + app::shortfall(b.cost) + ".");
        audio::crispClick();
        return;
      }
      app::spend(b.cost);
      addScore(400);
    }
    b.built = true;
    b.level = std::max(1, b.level);
    b.pos[0] = pos.x; b.pos[1] = pos.y; b.pos[2] = pos.z;
    b.rotY = rot;
    if (b.type != "bridge") resources::clearAround(pos.x, pos.z, footprint(b.type) + 2.5f);
    audio::levelUpStinger();
    const Production* P = production(b.id);
    notify(wasBuilt ? b.name + " moved." : b.name + " built!" + (P ? std::string(" It produces ") + P->label + "." : ""));
    cancelPlacement();
    worldChanged();
    return;
  }
}

void cancelPlacement() {
  E.world.placing.clear();
  E.world.placeKind = SK_COUNT;
  E.world.placeUid = 0;
  A.showStyle = false;
}

void openTab(Tab t) { A.tab = A.tab == t ? Tab::None : t; }

void cycleLook(int slot) {
  auto& w = E.world;
  if (w.placing.empty() || w.placeKind == SK_COUNT || !w.hasPlaceLook) return;
  w.placeLook = app::nextLook(w.placeKind, w.placeLook, slot);
  if (w.placing.rfind("s:", 0) == 0) A.drafts[w.placeKind] = w.placeLook;
}

}  // namespace actions

namespace app {

bool has(const Cost& c) {
  for (auto& e : c) if (invCount(e.first) < e.second) return false;
  return true;
}

std::string shortfall(const Cost& c) {
  std::string s;
  for (auto& e : c) {
    int h = invCount(e.first);
    if (h >= e.second) continue;
    if (!s.empty()) s += ", ";
    s += std::to_string(e.second - h) + " " + itemLabel(e.first);
  }
  return s;
}

void spend(const Cost& c) {
  Cost neg;
  for (auto& e : c) neg.push_back({e.first, -e.second});
  actions::addItems(neg);
}

Cost recruitCost(const std::string& id) {
  if (id == "carpenter") return {{"wood", 20}, {"fiber", 10}};
  if (id == "herbalist") return {{"herb", 8}};
  if (id == "builder") return {{"stone", 30}, {"deer_pelt", 4}};
  return {};
}

Cost upgradeCost(const Building& b) {
  Cost c;
  for (auto& e : b.cost) c.push_back({e.first, (int)std::ceil(e.second * 0.75f)});
  Cost extra;
  if (b.id == "hunter_hut") extra = {{"boar_hide", 2}, {"sinew", 2}};
  else if (b.id == "carpenter_workshop") extra = {{"bone", 4}};
  else if (b.id == "herbalist_lodge") extra = {{"herb", 6}};
  else if (b.id == "watchtower") extra = {{"goral_horn", 2}};
  for (auto& e : extra) {
    bool found = false;
    for (auto& x : c) if (x.first == e.first) { x.second += e.second; found = true; }
    if (!found) c.push_back(e);
  }
  return c;
}

int capacity() { return housingCapacity(); }

bool uiOpen() { return A.tab != Tab::None || A.dialogue >= 0 || A.menuOpen || A.howToOpen || A.status != Status::Playing; }

void syncDerived() {
  auto built = [](const char* id) { for (auto& b : A.buildings) if (b.id == id) return b.built; return false; };
  auto rec = [](const char* id) { for (auto& s : A.specialists) if (s.id == id) return s.recruited; return false; };
  auto& bn = E.world.bonus;
  bn.peltMult = built("hunter_hut") ? 2 : 1;
  bn.woodExtra = rec("carpenter") ? 2 : 0;
  bn.stoneExtra = rec("builder") ? 2 : 0;
  bn.herbExtra = rec("herbalist") ? 1 : 0;
  bn.animalRadar = built("watchtower") ? 140.0f : built("hunter_hut") ? 80.0f : 45.0f;
  E.world.gearCoat = invCount("fur_coat") > 0;
  E.world.gearArmour = invCount("leather_armour") > 0;
  E.world.hungry = A.hungerDays > 0;
  int cap = capacity();
  if (A.population > cap) A.population = cap;
  fitJobs(A.jobs, A.population);
  village::syncResidents();
}

void init() {
  A.specialists = initialSpecialists();
  A.buildings = initialBuildings();
  A.quests = initialQuests();
  A.clues = initialClues();
  Json d;
  A.hasSave = readSave(d);
}

void newGame() {
  resetWorld();
  setSeasonStart(A.startSeason);
  A.prodAcc.clear();
  A.status = Status::Playing;
  A.score = 0;
  A.level = 1;
  A.inv.clear();
  for (int i = 0; i < ITEM_COUNT; i++) A.inv[ITEM_ORDER[i]] = 0;
  for (auto& c : START_COUNTS) A.inv[c.first] = c.second;
  A.specialists = initialSpecialists();
  A.buildings = initialBuildings();
  A.quests = initialQuests();
  A.clues = initialClues();
  A.dialogue = -1;
  actions::cancelPlacement();
  A.structures.clear();
  A.population = 0;
  for (auto& j : A.jobs) j = 0;
  A.hungerDays = 0;
  A.uid = 1;
  A.jobAcc = 0;
  A.lastHour = E.time;
  A.stage = 0;
  g_lastStage = 0;
  A.arrivalAcc = 0;
  A.secAcc = 0;
  A.saveAcc = 0;
  A.victoryAt = -1;
  A.timers.clear();
  ambience::start();
  A.menuOpen = false;
  A.howToOpen = false;
  A.tab = Tab::None;
  npcs::sync();
  props::rebuildLandmarks();
  worldChanged();
  actions::notify("Kalyan has spotted deer nearby. Follow the gold marker on your compass.");
  audio::victoryFanfare();
}

bool continueGame() {
  Json d;
  if (!readSave(d)) return false;
  resetWorld();
  const Json* se = d.get("season");
  float t = (float)d.num("time", 7.5);
  restoreSeason(se ? (int)se->num("start", 2) : 2, se ? (int)se->num("day", 0) : 0, t);
  E.time = t;
  A.lastHour = t;
  const Json* pl = d.get("player");
  if (pl) {
    const Json* pos = pl->get("pos");
    if (pos && pos->arr.size() == 3) E.player.pos = {(float)pos->arr[0].n, (float)pos->arr[1].n + 0.5f, (float)pos->arr[2].n};
    E.player.yaw = (float)pl->num("yaw", 0);
    E.player.maxHp = (float)pl->num("maxHp", 100);
    E.player.blessed.clear();
    if (const Json* bl = pl->get("blessed"))
      for (auto& b : bl->arr) { E.player.blessed.push_back(b.s); props::markSummit(b.s.c_str()); }
    E.player.hp = std::max(40.0f, std::min(E.player.maxHp, (float)pl->num("hp", 100)));
  }
  player::reset();
  A.prodAcc.clear();
  A.jobAcc = 0;
  A.arrivalAcc = 0;
  A.secAcc = 0;
  A.saveAcc = 0;
  A.stage = (int)d.num("stage", 0);
  g_lastStage = A.stage;
  A.uid = (int)d.num("uid", 1);
  A.inv.clear();
  for (int i = 0; i < ITEM_COUNT; i++) A.inv[ITEM_ORDER[i]] = 0;
  if (const Json* inv = d.get("inventory"))
    for (auto& e : inv->obj) A.inv[e.first] = (int)e.second.n;
  A.buildings = initialBuildings();
  if (const Json* bs = d.get("buildings"))
    for (auto& o : bs->arr)
      for (auto& b : A.buildings) {
        if (b.id != o.str("id")) continue;
        b.built = o.flag("built");
        b.level = (int)o.num("level", 0);
        if (const Json* p = o.get("position"))
          if (p->arr.size() == 3) for (int k = 0; k < 3; k++) b.pos[k] = (float)p->arr[k].n;
        b.rotY = (float)o.num("rotationY", 0);
      }
  A.specialists = initialSpecialists();
  if (const Json* rec = d.get("recruited"))
    for (auto& r : rec->arr)
      for (auto& s : A.specialists) if (s.id == r.s) s.recruited = true;
  A.quests = initialQuests();
  if (const Json* qs = d.get("quests"))
    for (auto& o : qs->arr)
      for (auto& q : A.quests)
        if (q.id == o.str("id") && o.flag("completed")) { q.completed = true; q.progress = q.target; }
  A.clues = initialClues();
  if (const Json* cl = d.get("clues"))
    for (auto& c : cl->arr)
      for (auto& x : A.clues) if (x.id == c.s) x.discovered = true;
  A.structures.clear();
  if (const Json* st = d.get("structures"))
    for (auto& o : st->arr) {
      Structure s;
      s.uid = (int)o.num("uid", 0);
      s.kind = kindFromId(o.str("kind"));
      if (s.kind == SK_COUNT) continue;
      s.x = (float)o.num("x", 0); s.y = (float)o.num("y", 0); s.z = (float)o.num("z", 0); s.rot = (float)o.num("rot", 0);
      s.look = lookFrom(o.get("look"));
      A.structures.push_back(s);
    }
  A.population = (int)d.num("population", 0);
  for (int j = 0; j < JOB_COUNT; j++) A.jobs[j] = 0;
  if (const Json* jobs = d.get("jobs"))
    for (int j = 0; j < JOB_COUNT; j++) A.jobs[j] = (int)jobs->num(JOBS[j].key, 0);
  A.hungerDays = (int)d.num("hungerDays", 0);
  A.status = Status::Playing;
  A.score = (int)d.num("score", 0);
  A.level = (int)d.num("level", 1);
  A.dialogue = -1;
  actions::cancelPlacement();
  A.menuOpen = false;
  A.howToOpen = false;
  A.tab = Tab::None;
  A.victoryAt = -1;
  A.timers.clear();
  ambience::start();
  npcs::reset();
  for (auto& s : A.structures) resources::clearAround(s.x, s.z, structureRadius(s.kind) + 2.5f);
  props::rebuildLandmarks();
  worldChanged();
  actions::notify(std::string("Welcome back. ") + SEASON_NAMES[season.index] + ", day " + std::to_string(season.dayOfSeason) + ". " + std::to_string(A.population) + " villagers are waiting for you.");
  audio::confirmSelect();
  return true;
}

void quitToTitle() {
  saveNow();
  A.menuOpen = false;
  A.status = Status::Start;
  Json d;
  A.hasSave = readSave(d);
}

void saveNow() {
  if (A.status != Status::Playing) return;
  std::string text = buildSave().dump();
  if (platform::writeFile(savePath(), text)) A.hasSave = true;
}

bool readSaveSummary(std::string& summary) {
  Json d;
  if (!readSave(d)) return false;
  const Json* se = d.get("season");
  int start = se ? (int)se->num("start", 2) : 2, day = se ? (int)se->num("day", 0) : 0;
  int idx = ((start * 7 + day) / 7) % 4;
  const Json* st = d.get("structures");
  summary = std::string(SEASON_NAMES[idx]) + " · " + std::to_string((int)d.num("population", 0)) + " villagers · " + std::to_string(st ? (int)st->arr.size() : 0) + " buildings · " + std::to_string((int)d.num("score", 0)) + " renown";
  return true;
}

void craftArrows(bool bundle) {
  Cost cost = bundle ? Cost{{"wood", 2}, {"fiber", 1}} : Cost{{"wood", 1}};
  if (!has(cost)) { actions::notify("Need " + shortfall(cost) + " to fletch arrows."); audio::crispClick(); return; }
  spend(cost);
  int n = bundle ? 25 : 10;
  actions::addItems({{"arrow", n}});
  audio::confirmSelect();
  popup("+" + std::to_string(n) + " Arrows");
}

void recruit(int i) {
  if (i < 0 || i >= (int)A.specialists.size()) return;
  Specialist& s = A.specialists[i];
  Cost cost = recruitCost(s.id);
  if (!has(cost)) {
    actions::notify(s.name.substr(0, s.name.find(' ')) + " still needs " + shortfall(cost) + ".");
    audio::crispClick();
    return;
  }
  spend(cost);
  s.recruited = true;
  audio::powerupChime();
  actions::addScore(400);
  actions::notify(s.name + " joins the village and heads home. " + s.bonus);
  A.dialogue = -1;
  npcs::sync();
  syncDerived();
}

void upgrade(Building& b) {
  Cost cost = upgradeCost(b);
  if (!has(cost)) { actions::notify("Upgrade needs " + shortfall(cost) + "."); audio::crispClick(); return; }
  spend(cost);
  b.level += 1;
  actions::addScore(250);
  audio::levelUpStinger();
  actions::notify(b.name + " upgraded to level " + std::to_string(b.level) + " — production doubled.");
  syncDerived();
}

void trade(const Trade& t) {
  bool market = false;
  for (auto& s : A.structures) if (s.kind == SK_MARKET_STALL) market = true;
  if (!market) { actions::notify("Build a Market Stall first."); return; }
  if (!has(t.give)) { actions::notify("Need " + shortfall(t.give) + "."); return; }
  spend(t.give);
  actions::addItems(t.get);
  audio::coinPickup();
  popup("Traded for " + costText(t.get), rgbHex("#fde68a"));
}

void craftGear(const GearDef& g) {
  if (invCount(g.id) > 0) return;
  if (!has(g.cost)) { actions::notify(std::string(g.name) + " needs " + shortfall(g.cost) + "."); audio::crispClick(); return; }
  spend(g.cost);
  actions::addItems({{g.id, 1}});
  audio::powerupChime();
  actions::notify(std::string("You crafted the ") + g.name + ". " + g.blurb);
}

Look defaultLookFor(StructureKind k) {
  Look l;
  KindGroup g = KINDS[k].group;
  l.roof = k == SK_MARKET_STALL ? 2 : 0;
  l.wall = 0;
  l.wood = 0;
  l.door = 0;
  l.accent = g == KG_DECOR ? 3 : 1;
  l.deco = g == KG_HOME ? (1 << DK_POTS) : 0;
  return l;
}

Look nextLook(StructureKind k, const Look& l, int slot) {
  if (slot < 0 || slot >= LO_COUNT) return l;
  const auto& opts = KINDS[k].options;
  if (std::find(opts.begin(), opts.end(), (LookOpt)slot) == opts.end()) return l;
  Look o = l;
  if (slot == LO_DECO) {
    int idx = -1;
    for (size_t i = 0; i < DECO_SETS.size(); i++) {
      int m = 0;
      for (auto d : DECO_SETS[i]) m |= 1 << d;
      if (m == l.deco) idx = (int)i;
    }
    int m = 0;
    for (auto d : DECO_SETS[(idx + 1) % DECO_SETS.size()]) m |= 1 << d;
    o.deco = m;
    return o;
  }
  int* f[5] = {&o.roof, &o.wall, &o.wood, &o.door, &o.accent};
  *f[slot] = (*f[slot] + 1) % LOOK_COUNTS[slot];
  return o;
}

void startStructure(StructureKind k) {
  const KindInfo& info = KINDS[k];
  StageInfo st = computeStage();
  if (st.level < UNLOCK[k]) { actions::notify(std::string("The ") + info.name + " unlocks when your settlement grows."); return; }
  if (!has(info.cost)) { actions::notify("Still need " + shortfall(info.cost) + " for the " + info.name + "."); audio::crispClick(); return; }
  auto it = A.drafts.find(k);
  E.world.placeLook = it != A.drafts.end() ? it->second : defaultLookFor(k);
  E.world.hasPlaceLook = true;
  E.world.placing = std::string("s:") + info.id;
  E.world.placeKind = k;
  E.world.placeUid = 0;
  E.world.previewRot = E.player.yaw;
  A.tab = Tab::None;
  audio::crispClick();
}

void moveStructure(int uid) {
  for (auto& s : A.structures) {
    if (s.uid != uid) continue;
    E.world.placeLook = s.look;
    E.world.hasPlaceLook = true;
    E.world.placing = "m:" + std::to_string(uid);
    E.world.placeKind = s.kind;
    E.world.placeUid = uid;
    E.world.previewRot = E.player.yaw;
    A.tab = Tab::None;
  }
}

void removeStructure(int uid) {
  for (size_t i = 0; i < A.structures.size(); i++) {
    Structure s = A.structures[i];
    if (s.uid != uid) continue;
    Cost back;
    for (auto& c : KINDS[s.kind].cost) back.push_back({c.first, c.second / 2});
    actions::addItems(back);
    A.structures.erase(A.structures.begin() + i);
    actions::notify(std::string(KINDS[s.kind].name) + " taken down. Half the materials were returned.");
    worldChanged();
    return;
  }
}

void restyleStructure(int uid, const Look& l) {
  for (auto& s : A.structures)
    if (s.uid == uid) s.look = l;
  village::structuresChanged();
}

void placeWorkshop(const std::string& id) {
  for (auto& b : A.buildings) {
    if (b.id != id) continue;
    if (!b.built && !has(b.cost)) {
      actions::notify("Still need " + shortfall(b.cost) + " for the " + b.name + ".");
      audio::crispClick();
      return;
    }
    E.world.placing = id;
    E.world.placeKind = SK_COUNT;
    E.world.hasPlaceLook = false;
    E.world.placeUid = 0;
    E.world.previewRot = E.player.yaw + PI;
    A.tab = Tab::None;
    audio::crispClick();
  }
}

void setJob(int j, int n) {
  int used = 0;
  for (int k = 0; k < JOB_COUNT; k++) used += A.jobs[k];
  used -= A.jobs[j];
  A.jobs[j] = std::max(0, std::min(n, A.population - used));
  village::assignJobs();
}

void update(float dt) {
  flushTimers();
  E.playing = A.status == Status::Playing;
  E.uiOpen = uiOpen();
  if (A.victoryAt > 0 && E.clock >= A.victoryAt) { A.victoryAt = -1; A.status = Status::Victory; }
  if (A.status != Status::Playing) return;
  if (!A.menuOpen) {
    A.secAcc += dt;
    while (A.secAcc >= 1) {
      A.secAcc -= 1;
      secondTick();
    }
  }
  A.saveAcc += dt;
  if (A.saveAcc >= 30) { A.saveAcc = 0; saveNow(); }
  g_questT += dt;
  if (g_questT > 0.25f) {
    g_questT = 0;
    updateQuests();
    StageInfo st = computeStage();
    if (st.level > g_lastStage) {
      audio::victoryFanfare();
      actions::addScore(1000);
      popup(st.name, rgbHex("#fbbf24"));
      int food = 10 + st.level * 10;
      actions::notify("Festival! Your settlement is now a " + st.name + ". " + (st.level == 1 ? "Family houses, the communal house and the community hall are unlocked." : "Travellers speak of your village across the valley.") +
                      " The villagers gather at the hearth and share a feast (+" + std::to_string(food) + " food).");
      E.festival.t = 150;
      actions::addItems({{"food", food}});
    }
    g_lastStage = st.level;
    A.stage = st.level;
    syncDerived();
  }
}

}  // namespace app
