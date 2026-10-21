#pragma once
#include "../core/math.h"
#include <string>
#include <vector>
#include <map>
#include <functional>

using Cost = std::vector<std::pair<std::string, int>>;

enum StructureKind {
  SK_COTTAGE, SK_RAISED_HOUSE, SK_FAMILY_HOUSE, SK_COMMUNAL_HOUSE, SK_MARKET_STALL, SK_ANIMAL_SHELTER,
  SK_COMMUNITY_HALL, SK_FOOD_STORE, SK_FENCE, SK_PATH, SK_LAMP_POST, SK_FLAGPOLE, SK_BENCH, SK_COUNT
};

enum DecoKey { DK_POTS, DK_LANTERN, DK_FIREWOOD, DK_BENCH, DK_BARRELS, DK_FENCE, DK_CLOTH, DK_COUNT };

struct Look {
  int roof = 0, wall = 0, wood = 0, door = 0, accent = 1;
  int deco = 1;
  bool has(DecoKey k) const { return (deco >> k) & 1; }
  bool operator==(const Look& o) const { return roof == o.roof && wall == o.wall && wood == o.wood && door == o.door && accent == o.accent && deco == o.deco; }
};

struct Structure {
  int uid = 0;
  StructureKind kind = SK_COTTAGE;
  float x = 0, y = 0, z = 0, rot = 0;
  Look look;
};

struct Building {
  std::string id, type, name;
  int level = 0, maxLevel = 1;
  float pos[3] = {0, 0, 0};
  float rotY = 0;
  Cost cost;
  bool built = false;
  std::string desc;
};

struct Specialist {
  std::string id, name, title, profession;
  bool recruited = false;
  std::string location, dialogue, bonus;
};

struct Quest {
  std::string id;
  int chapter;
  std::string title, giver, desc, objective;
  int progress = 0, target = 1;
  bool completed = false;
  std::string reward;
};

struct Clue {
  std::string id, type, title, desc;
  float x = 0, z = 0;
  bool discovered = false;
};

struct HudEvent {
  enum Kind { Popup, Hit, Knock, Hurt } kind = Popup;
  std::string text;
  uint32_t color = 0xffffffff;
  bool headshot = false, kill = false;
  float amount = 0;
};

enum MarkerIcon { MI_WOOD, MI_STONE, MI_FIBER, MI_HERB, MI_HARVEST, MI_RECRUIT, MI_CLUE, MI_QUEST, MI_ANIMAL, MI_BUILDING, MI_NPC, MI_DANGER, MI_COUNT };

struct Marker {
  float x, y, z;
  MarkerIcon icon;
  std::string label;
};

struct QuestTarget {
  bool valid = false;
  float x = 0, z = 0;
  std::string label;
};

enum class Status { Start, Playing, Victory };
enum class Tab { None, Inventory, Quests, Buildings, Map };

struct Engine {
  bool playing = false;
  bool uiOpen = false;
  float time = 7.5f;
  bool isNight = false;
  float daylight = 1;

  struct Player {
    Vec3 pos{0, 1.7f, 9};
    float yaw = 0, pitch = -0.05f, velY = 0;
    bool onGround = true, crouching = false, sprinting = false, moving = false;
    Vec3 knock;
    float hp = 100, maxHp = 100;
    std::vector<std::string> blessed;
    float hurtT = -99;
  } player;

  struct Bow {
    bool drawing = false;
    float draw = 0;
    bool aiming = false;
    float cooldown = 0, lastShotPower = 0;
  } bow;

  struct World {
    std::string placing;
    StructureKind placeKind = SK_COUNT;
    bool hasPlaceLook = false;
    Look placeLook;
    int placeUid = 0;
    Vec3 previewPos;
    float previewRot = 0;
    bool previewValid = false;
    std::string previewReason;
    bool gearCoat = false, gearArmour = false;
    bool hungry = false;
    struct Bonus { float peltMult = 1; int woodExtra = 0, stoneExtra = 0, herbExtra = 0; float animalRadar = 45; } bonus;
  } world;

  std::string nearbyPrompt;
  QuestTarget questTarget;
  struct Noise { float x = 0, z = 0, r = 0, t = -99; } noise;
  float clock = 0;
  float simMs = 0;
  struct Festival { float t = 0; } festival;
  struct Cutscene { float t = 0, dur = 0, x = 0, y = 0, z = 0, yaw = 0; std::string name; } cutscene;
  struct Raid { bool active = false; float x = 0, z = 0; } raid;
  std::vector<HudEvent> events;
};

struct App {
  Status status = Status::Start;
  int score = 0, level = 1;
  std::map<std::string, int> inv;
  std::vector<Specialist> specialists;
  std::vector<Building> buildings;
  std::vector<Quest> quests;
  std::vector<Clue> clues;
  std::vector<Structure> structures;
  int population = 0;
  int jobs[4] = {0, 0, 0, 0};
  int hungerDays = 0;
  int uid = 1;
  int stage = 0;
  std::map<int, Look> drafts;
  Tab tab = Tab::None;
  int buildSection = 1;
  int dialogue = -1;
  bool menuOpen = false, howToOpen = false, showStyle = false;
  bool musicMuted = false, sfxMuted = false;
  std::string notification;
  double notifyAt = -100;
  int startSeason = 2;
  float jobAcc = 0, arrivalAcc = 0, secAcc = 0, saveAcc = 0;
  std::map<std::string, float> prodAcc;
  float lastHour = 7.5f;
  bool hasSave = false;
  float victoryAt = -1;
  std::vector<std::pair<double, std::function<void()>>> timers;
};

extern Engine E;
extern App A;

extern const char* ITEM_ORDER[];
extern const int ITEM_COUNT;
const char* itemLabel(const std::string& id);
const char* itemName(const std::string& id);
const char* itemDesc(const std::string& id);
const char* itemIcon(const std::string& id);

int invCount(const std::string& id);
void emitEvent(const HudEvent& e);
void popup(const std::string& text, uint32_t color = 0xffffffff);
void makeNoise(float x, float z, float r);
float relativeBearing(float x, float z);
void collectMarkers(std::vector<Marker>& out);
void registerMarkers(const std::string& key, std::function<void(std::vector<Marker>&)> fn);
uint32_t rgbHex(const char* hex);
float randf();

namespace actions {
void addItems(const Cost& delta);
void addScore(int n);
void notify(const std::string& text);
void openDialogue(const std::string& specialistId);
void discoverClue(const std::string& clueId);
void placeBuilding(const std::string& id, Vec3 pos, float rot);
void cancelPlacement();
void openTab(Tab t);
void cycleLook(int slot);
void later(double seconds, std::function<void()> fn);
}
