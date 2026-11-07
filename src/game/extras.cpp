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
#include "../render/mhuman.h"
#include "../world/forest.h"
#include "../audio/audio.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
Mesh g_board, g_note, g_tick;
float g_bountyT = 0;
const char* SP_NAME[SP_COUNT] = {"deer", "wild boar", "mountain goats", "wolves", "black bear", "leopard"};
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
  float dodgeT = 0, dodgeCd = 0, dodgeDir = 1;  // quick sidestep out of the player's aim
  float deadT = 0;                               // seconds since killed (death fall, then the body lies a while)
  bool alert = false;
};
struct BArrow { Vec3 p, v; float life; bool stuck; };
std::vector<BArrow> g_barrows;  // bandit arrows in flight (or stuck in the ground for a few seconds)
Mesh g_arrowMesh;
const float DRAW_TIME = 0.7f;
bool g_banditOn = false;
float g_campX = 0, g_campZ = 0;
int g_banditDays = 0;
std::vector<Bandit> g_bandits;
// one camp per season, each tougher than the last until level 6, then it stays at 6
int g_campsSpawned = 0, g_campLevel = 1, g_campSeason = -1, g_campOffset = 2;
struct CampTier { int count; float hp, dmg, range, cd, hit; };
CampTier tier(int L) {
  L = std::max(1, std::min(6, L));
  return {2 + L, 80.0f + 20.0f * L, 6.0f + 2.0f * L, 26.0f + 2.0f * L, 1.75f - 0.13f * L, 0.45f + 0.05f * L};  // dmg 8..18, a shot every ~1.0..1.9 s
}
void placeBandits(int n, float hp) {
  g_bandits.clear();
  for (int k = 0; k < n; k++) {
    float a = k * TAU / n + 0.3f, r = 3.2f + (k & 1) * 1.6f + (n > 5 ? 1.0f : 0.0f);
    g_bandits.push_back({g_campX + std::cos(a) * r, g_campZ + std::sin(a) * r, hp, 2 + k * 0.5f});
  }
}
Mesh g_tent, g_banditMesh;

// detailed village furniture (vertex-coloured; ~4-6K triangles each, drawn once): deodar timber in the
// Kath-Kuni style with carved rings and brackets, stacked slate roofs, brass, prayer flags
std::string shade(const char* hex, float k) {  // brighten/darken a #rrggbb colour by k (0.8 = 20 % darker)
  unsigned v = (unsigned)std::strtoul(hex + 1, nullptr, 16);
  int r = (int)std::min(255.0f, ((v >> 16) & 255) * k), g = (int)std::min(255.0f, ((v >> 8) & 255) * k), b = (int)std::min(255.0f, (v & 255) * k);
  char buf[8];
  std::snprintf(buf, sizeof buf, "#%02x%02x%02x", r, g, b);
  return buf;
}
struct Rnd { unsigned s; float operator()() { s = s * 1664525u + 1013904223u; return (s >> 8) * (1.0f / 16777216.0f); } };
// pitched roof of overlapping slate tiles over [-w/2, w/2] in x, ridge at height `ry`, slopes `run` deep each side
void slateRoof(Shape& s, Rnd& R, float w, float ry, float run, float pitch) {
  const char* slate[3] = {"#4b5563", "#57606b", "#3f4650"};
  int rows = (int)(run / 0.11f);
  for (int side = -1; side <= 1; side += 2)
    for (int r = 0; r < rows; r++) {
      float d = 0.06f + r * 0.11f;
      float y = ry - std::sin(pitch) * d + 0.012f * r, z = side * std::cos(pitch) * d;
      float off = (r & 1) ? 0.09f : 0.0f;
      for (float x = -w * 0.5f + off; x < w * 0.5f; x += 0.18f) {
        float jy = (R() - 0.5f) * 0.012f, jr = (R() - 0.5f) * 0.06f;
        s.box(shade(slate[(int)(R() * 3) % 3], 0.85f + R() * 0.3f).c_str(), {x + 0.09f, y + jy, z}, {0.17f, 0.022f, 0.15f}, {side * pitch, jr, 0});
      }
    }
  s.box("#3b2a1a", {0, ry + 0.04f, 0}, {w + 0.1f, 0.07f, 0.09f});  // ridge beam
  for (float x = -w * 0.5f; x <= w * 0.5f + 0.01f; x += w / 6) s.box("#374151", {x, ry + 0.085f, 0}, {0.12f, 0.035f, 0.12f}, {0, 0.785f, 0});  // ridge caps
}
// carved deodar post: square shaft with ringed bands, a stone footing and a stepped capital bracket
void carvedPost(Shape& s, float x, float z, float h, float t) {
  s.box("#78716c", {x, 0.09f, z}, {t * 2.2f, 0.18f, t * 2.2f}).box("#6b6560", {x, 0.21f, z}, {t * 1.7f, 0.08f, t * 1.7f});
  s.box("#5b3a1f", {x, 0.25f + h * 0.5f, z}, {t, h, t});
  for (int k = 0; k < 4; k++) {
    float y = 0.45f + k * (h - 0.5f) / 3.0f;
    s.box("#7c4f2a", {x, y, z}, {t * 1.25f, 0.05f, t * 1.25f}).box("#3b2414", {x, y + 0.04f, z}, {t * 1.12f, 0.02f, t * 1.12f});
  }
  for (int k = 0; k < 3; k++) s.box(k == 1 ? "#7c4f2a" : "#6b4423", {x, 0.25f + h + 0.04f + k * 0.07f, z}, {t * (1.4f + k * 0.35f), 0.07f, t * (1.3f + k * 0.2f)});
}
void buildMeshes() {
  Rnd R{12345u};
  {
    // trophy showcase: a carved shrine-like wall. Stone plinth, two ringed posts, plank back wall in two tiers of
    // shield plaques (8 slots), a carved dentil frieze, slate roof, a horned skull on the gable, brass bells,
    // prayer flags and an offering shelf with butter lamps
    Shape s;
    for (int k = 0; k < 9; k++) {  // dry-stone plinth
      float x = -1.6f + k * 0.4f;
      s.box(shade("#7c776f", 0.85f + R() * 0.3f).c_str(), {x, 0.13f, 0.05f}, {0.38f + R() * 0.04f, 0.26f, 0.46f}, {0, (R() - 0.5f) * 0.06f, 0});
      s.box(shade("#6b665e", 0.85f + R() * 0.3f).c_str(), {x + 0.2f, 0.33f, 0.04f}, {0.38f, 0.16f, 0.42f}, {0, (R() - 0.5f) * 0.06f, 0});
    }
    carvedPost(s, -1.62f, 0, 2.55f, 0.18f);
    carvedPost(s, 1.62f, 0, 2.55f, 0.18f);
    for (int k = 0; k < 14; k++) {  // vertical plank back wall
      float x = -1.43f + k * 0.22f;
      s.box(shade("#6b4423", 0.8f + R() * 0.35f).c_str(), {x, 1.62f, -0.06f}, {0.21f, 2.55f, 0.05f});
      s.box("#2a1a0e", {x + 0.105f, 1.62f, -0.035f}, {0.012f, 2.55f, 0.01f});
    }
    for (int t = 0; t < 3; t++) {  // horizontal rails between the tiers
      float y = 0.5f + t * 1.0f;
      s.box("#7c4f2a", {0, y, 0.0f}, {3.2f, 0.1f, 0.1f}).box("#3b2414", {0, y + 0.06f, 0.03f}, {3.2f, 0.02f, 0.06f});
    }
    s.box("#6b4423", {0, 2.9f, 0.02f}, {3.6f, 0.14f, 0.2f});  // lintel
    for (int k = 0; k < 26; k++) s.box(k & 1 ? "#4a2f19" : "#8b5a2b", {-1.75f + k * 0.14f, 2.79f, 0.1f}, {0.07f, 0.08f, 0.06f});  // dentils
    for (int k = 0; k < 13; k++) s.ball("#a16207", {-1.68f + k * 0.28f, 2.92f, 0.12f}, {0.025f, 0.025f, 0.015f}, {0, 0, 0}, 0);  // brass studs
    slateRoof(s, R, 4.0f, 3.55f, 0.85f, 0.62f);
    s.box("#5b3a1f", {0, 3.05f, 0.45f}, {4.0f, 0.06f, 0.06f}).box("#5b3a1f", {0, 3.05f, -0.45f}, {4.0f, 0.06f, 0.06f});  // eaves
    for (int k = 0; k < 12; k++) s.box("#4a2f19", {-1.85f + k * 0.335f, 3.0f, 0.5f}, {0.04f, 0.1f, 0.04f});  // carved eave fringe
    // gable trophy: an ibex skull with long ridged scimitar horns
    s.ball("#e7e2d6", {0, 3.25f, 0.42f}, {0.11f, 0.13f, 0.09f}).ball("#d9d3c4", {0, 3.12f, 0.48f}, {0.06f, 0.1f, 0.06f});
    s.ball("#1c1917", {-0.05f, 3.27f, 0.5f}, {0.025f, 0.02f, 0.01f}).ball("#1c1917", {0.05f, 3.27f, 0.5f}, {0.025f, 0.02f, 0.01f});
    for (int side = -1; side <= 1; side += 2)
      for (int k = 0; k < 8; k++) {
        float a0 = k / 8.0f, a1 = (k + 1) / 8.0f;
        auto P = [&](float a) { return Vec3{side * (0.06f + 0.22f * a), 3.33f + 0.42f * std::sin(a * 1.9f), 0.4f - 0.3f * a * a}; };
        s.rod(k & 1 ? "#6b5d4a" : "#7c6c56", P(a0), P(a1), 0.04f * (1 - a0 * 0.75f), 0.04f * (1 - a1 * 0.75f), 6);
      }
    // brass bells on cords under the eaves
    for (int side = -1; side <= 1; side += 2) {
      s.rod("#3b2a1a", {side * 1.9f, 3.0f, 0.45f}, {side * 1.9f, 2.62f, 0.45f}, 0.006f, 0.006f, 3);
      s.cone("#b45309", {side * 1.9f, 2.5f, 0.45f}, 0.07f, 0.13f, 8).ball("#ca8a04", {side * 1.9f, 2.62f, 0.45f}, {0.035f, 0.03f, 0.035f}, {0, 0, 0}, 0);
    }
    // prayer flags strung between the post tops
    const char* fl[5] = {"#1d4ed8", "#f8fafc", "#dc2626", "#16a34a", "#eab308"};
    for (int k = 0; k < 15; k++) {
      float u = k / 14.0f, x = -1.6f + u * 3.2f, y = 2.98f - 0.22f * std::sin(u * PI);
      s.box(fl[k % 5], {x, y - 0.09f, 0.32f}, {0.15f, 0.15f, 0.008f}, {0, 0, (R() - 0.5f) * 0.15f});
    }
    for (int k = 0; k < 14; k++) {
      float u0 = k / 14.0f, u1 = (k + 1) / 14.0f;
      s.rod("#d6d3d1", {-1.6f + u0 * 3.2f, 2.99f - 0.22f * std::sin(u0 * PI), 0.32f}, {-1.6f + u1 * 3.2f, 2.99f - 0.22f * std::sin(u1 * PI), 0.32f}, 0.005f, 0.005f, 3);
    }
    // offering shelf with butter lamps and a brass bowl
    s.box("#7c4f2a", {0, 0.62f, 0.22f}, {1.4f, 0.06f, 0.3f});
    for (int k = 0; k < 2; k++) s.box("#4a2f19", {(k ? 0.55f : -0.55f), 0.55f, 0.15f}, {0.06f, 0.12f, 0.18f});
    for (int k = 0; k < 5; k++) {
      float x = -0.5f + k * 0.25f;
      s.tube("#b45309", {x, 0.68f, 0.24f}, 0.045f, 0.03f, 0.06f, 8).ball("#fde68a", {x, 0.73f, 0.24f}, {0.012f, 0.03f, 0.012f}, {0, 0, 0}, 0);
    }
    // eight carved shield plaques (two tiers) the trophies hang on, each with brass nail heads
    for (int slot = 0; slot < 8; slot++) {
      float lx = -1.15f + (slot % 4) * 0.77f, ly = slot < 4 ? 2.05f : 1.05f;
      s.box("#8b5a2b", {lx, ly, 0.0f}, {0.62f, 0.78f, 0.04f}).box("#5b3a1f", {lx, ly + 0.43f, 0.01f}, {0.5f, 0.08f, 0.05f}).box("#5b3a1f", {lx, ly - 0.43f, 0.01f}, {0.4f, 0.08f, 0.05f});
      for (int c = 0; c < 4; c++) s.ball("#ca8a04", {lx + ((c & 1) ? 0.26f : -0.26f), ly + ((c & 2) ? 0.33f : -0.33f), 0.03f}, {0.018f, 0.018f, 0.01f}, {0, 0, 0}, 0);
    }
    g_rack = uploadMesh(s.build(true));
  }
  // trophies: stretched hides laced into a stick frame, a goral skull plaque
  for (int i = 0; i < 5; i++) {
    Shape t;
    if (i == 2) {  // goral: skull on a shield with short back-curved horns
      t.ball("#e7e2d6", {0, 0.02f, 0.05f}, {0.1f, 0.13f, 0.07f}).ball("#d9d3c4", {0, -0.1f, 0.08f}, {0.055f, 0.09f, 0.05f});
      t.ball("#1c1917", {-0.045f, 0.04f, 0.11f}, {0.022f, 0.018f, 0.01f}).ball("#1c1917", {0.045f, 0.04f, 0.11f}, {0.022f, 0.018f, 0.01f});
      for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 6; k++) {
          float a0 = k / 6.0f, a1 = (k + 1) / 6.0f;
          auto P = [&](float a) { return Vec3{side * (0.05f + 0.06f * a), 0.12f + 0.22f * a, 0.06f - 0.14f * a * a}; };
          t.rod(k & 1 ? "#292524" : "#3f3a36", P(a0), P(a1), 0.028f * (1 - a0 * 0.7f), 0.028f * (1 - a1 * 0.7f), 6);
        }
      t.rod("#7c4f2a", {-0.2f, -0.22f, 0.05f}, {0.2f, -0.22f, 0.05f}, 0.012f, 0.012f, 4);
    } else {
      const char* base[5] = {"#8b5a2b", "#4a3b2e", "", "#8a8a85", "#1c1917"};
      const char* belly[5] = {"#d6b48a", "#6b5848", "", "#d6d3d1", "#2a2522"};
      float W = i == 4 ? 0.3f : 0.26f, H = i == 4 ? 0.34f : 0.3f;
      // frame of lashed sticks
      t.rod("#6b4423", {-W - 0.04f, -H - 0.04f, 0}, {W + 0.04f, -H - 0.04f, 0}, 0.012f, 0.012f, 4).rod("#6b4423", {-W - 0.04f, H + 0.04f, 0}, {W + 0.04f, H + 0.04f, 0}, 0.012f, 0.012f, 4);
      t.rod("#6b4423", {-W - 0.03f, -H - 0.06f, 0}, {-W - 0.03f, H + 0.06f, 0}, 0.012f, 0.012f, 4).rod("#6b4423", {W + 0.03f, -H - 0.06f, 0}, {W + 0.03f, H + 0.06f, 0}, 0.012f, 0.012f, 4);
      // hide body: overlapping flattened lobes give an irregular outline, darker along the spine
      t.ball(base[i], {0, 0, 0.01f}, {W * 0.72f, H * 0.95f, 0.018f}, {0, 0, 0}, 2);
      for (int k = 0; k < 4; k++) t.ball(base[i], {(k & 1 ? 1 : -1) * W * 0.68f, (k & 2 ? 1 : -1) * H * 0.62f, 0.008f}, {W * 0.3f, H * 0.22f, 0.012f}, {0, 0, (k & 1) == ((k >> 1) & 1) ? 0.7f : -0.7f}, 1);
      // a hide is opened along the belly: pale belly fur forms the rim, the back runs down the middle a shade darker
      t.ball(belly[i], {0, 0, 0.0f}, {W * 0.86f, H * 1.06f, 0.012f}, {0, 0, 0}, 2);
      t.ball(shade(base[i], 0.8f).c_str(), {0, 0, 0.024f}, {W * 0.07f, H * 0.8f, 0.006f}, {0, 0, 0}, 1);  // spine line
      t.ball(shade(base[i], 0.9f).c_str(), {0, H * 0.82f, 0.02f}, {W * 0.22f, H * 0.18f, 0.008f}, {0, 0, 0}, 1);  // neck
      if (i == 0) for (int k = 0; k < 10; k++) t.ball("#f5e6c8", {(R() - 0.5f) * W * 0.9f, (R() - 0.5f) * H * 1.2f, 0.03f}, {0.012f, 0.012f, 0.004f}, {0, 0, 0}, 0);  // fawn spots
      if (i == 1) for (int k = 0; k < 9; k++) t.box("#2a221c", {(R() - 0.5f) * 0.04f, H * (0.8f - k * 0.17f), 0.032f}, {0.012f, 0.05f, 0.012f}, {0, 0, (R() - 0.5f) * 0.5f});  // bristle ridge
      if (i == 4) t.ball("#e7e5e4", {0, H * 0.45f, 0.034f}, {W * 0.4f, 0.04f, 0.006f}, {0, 0, 0}, 1).ball("#e7e5e4", {0, H * 0.32f, 0.034f}, {W * 0.18f, 0.06f, 0.006f}, {0, 0, 0}, 1);  // moon-bear V
      if (i == 3) for (int k = 0; k < 7; k++) t.box("#5f5f5b", {(R() - 0.5f) * W * 0.5f, H * (0.6f - k * 0.17f), 0.028f}, {0.05f + R() * 0.05f, 0.012f, 0.004f}, {0, 0, (R() - 0.5f) * 0.6f});  // grizzled guard hairs
      // lacing cords from the hide edge to the frame
      for (int k = 0; k < 12; k++) {
        float a = k * TAU / 12;
        Vec3 e{std::cos(a) * W * 0.85f, std::sin(a) * H * 0.9f, 0.01f};
        Vec3 f{std::cos(a) > 0.3f ? W + 0.03f : (std::cos(a) < -0.3f ? -W - 0.03f : e.x), std::sin(a) > 0.3f ? H + 0.04f : (std::sin(a) < -0.3f ? -H - 0.04f : e.y), 0};
        t.rod("#d6c7a1", e, f, 0.003f, 0.003f, 3);
      }
    }
    g_tro[i] = uploadMesh(t.build(true));
  }
  {
    // notice board: stone footings, ringed deodar posts, plank backing with a carved frame and corner brackets,
    // slate roof with carved bargeboards, a hanging lantern, prayer flags, a ledge with ink pot, quill and a
    // rolled map. The task notes are separate meshes (one per posted task, see draw()).
    Shape s;
    carvedPost(s, -1.0f, 0, 2.05f, 0.15f);
    carvedPost(s, 1.0f, 0, 2.05f, 0.15f);
    for (int k = 0; k < 9; k++) {
      float x = -0.82f + k * 0.205f;
      s.box(shade("#8b5a2b", 0.78f + R() * 0.35f).c_str(), {x, 1.4f, -0.02f}, {0.2f, 1.15f, 0.05f});
      s.box("#2a1a0e", {x + 0.1f, 1.4f, 0.0f}, {0.01f, 1.15f, 0.01f});
      for (int g = 0; g < 2; g++) s.box(shade("#5b3a1f", 0.9f + R() * 0.2f).c_str(), {x + (R() - 0.5f) * 0.1f, 1.0f + R() * 0.8f, 0.004f}, {0.003f, 0.06f + R() * 0.1f, 0.003f});  // grain
    }
    s.box("#6b4423", {0, 2.02f, 0.03f}, {2.0f, 0.1f, 0.12f}).box("#6b4423", {0, 0.78f, 0.03f}, {2.0f, 0.1f, 0.12f});
    s.box("#3b2414", {0, 2.08f, 0.07f}, {2.0f, 0.02f, 0.06f}).box("#3b2414", {0, 0.72f, 0.07f}, {2.0f, 0.02f, 0.06f});
    for (int k = 0; k < 4; k++) {  // carved corner brackets
      float x = (k & 1) ? 0.82f : -0.82f, y = (k & 2) ? 1.93f : 0.87f, sx = (k & 1) ? -1.0f : 1.0f, sy = (k & 2) ? -1.0f : 1.0f;
      s.box("#7c4f2a", {x + sx * 0.06f, y + sy * 0.06f, 0.05f}, {0.16f, 0.04f, 0.04f}, {0, 0, sx * sy * 0.785f});
      s.ball("#a16207", {x, y, 0.07f}, {0.02f, 0.02f, 0.012f}, {0, 0, 0}, 0);
    }
    for (int k = 0; k < 10; k++) s.box(k & 1 ? "#4a2f19" : "#8b5a2b", {-0.9f + k * 0.2f, 2.13f, 0.07f}, {0.1f, 0.06f, 0.05f});  // dentils
    slateRoof(s, R, 2.5f, 2.62f, 0.62f, 0.55f);
    for (int side = -1; side <= 1; side += 2) {  // carved bargeboards on the gable ends
      for (int e = -1; e <= 1; e += 2)
        s.box("#5b3a1f", {side * 1.26f, 2.62f - 0.17f, e * 0.27f}, {0.04f, 0.07f, 0.62f}, {e * 0.55f, 0, 0});
      s.cone("#4a2f19", {side * 1.27f, 2.67f, 0}, 0.04f, 0.18f, 5);  // finial
    }
    // hanging lantern under the eave
    s.rod("#292524", {0.95f, 2.3f, 0.32f}, {0.95f, 2.1f, 0.32f}, 0.006f, 0.006f, 3);
    s.box("#292524", {0.95f, 2.08f, 0.32f}, {0.1f, 0.02f, 0.1f}).box("#fcd34d", {0.95f, 1.99f, 0.32f}, {0.07f, 0.14f, 0.07f}).box("#292524", {0.95f, 1.91f, 0.32f}, {0.11f, 0.02f, 0.11f});
    for (int c = 0; c < 4; c++) s.box("#292524", {0.95f + ((c & 1) ? 0.045f : -0.045f), 1.99f, 0.32f + ((c & 2) ? 0.045f : -0.045f)}, {0.012f, 0.15f, 0.012f});
    // prayer flags along the roof edge
    const char* fl[5] = {"#1d4ed8", "#f8fafc", "#dc2626", "#16a34a", "#eab308"};
    for (int k = 0; k < 11; k++) {
      float u = k / 10.0f, x = -1.0f + u * 2.0f, y = 2.36f - 0.12f * std::sin(u * PI);
      s.box(fl[k % 5], {x, y - 0.08f, 0.36f}, {0.13f, 0.13f, 0.008f}, {0, 0, (R() - 0.5f) * 0.15f});
    }
    // ledge with an ink pot, quill and a rolled map
    s.box("#7c4f2a", {0, 0.86f, 0.13f}, {1.5f, 0.05f, 0.18f});
    s.tube("#1f2937", {-0.5f, 0.92f, 0.14f}, 0.035f, 0.045f, 0.07f, 8).rod("#f5f5f4", {-0.5f, 0.95f, 0.14f}, {-0.45f, 1.12f, 0.1f}, 0.004f, 0.012f, 4);
    s.tube("#e7dcc0", {0.4f, 0.92f, 0.14f}, 0.03f, 0.03f, 0.42f, 8, {0, 0, PI / 2}).tube("#b91c1c", {0.4f, 0.92f, 0.14f}, 0.033f, 0.033f, 0.03f, 8, {0, 0, PI / 2});
    // a weathered old poster and the village map, always there
    s.box("#d9cba5", {-0.62f, 1.75f, 0.035f}, {0.3f, 0.26f, 0.006f}, {0, 0, 0.06f});
    for (int l = 0; l < 4; l++) s.box("#57534e", {-0.62f, 1.82f - l * 0.045f, 0.04f}, {0.22f - (l & 1) * 0.05f, 0.008f, 0.002f}, {0, 0, 0.06f});
    s.box("#e9dcc0", {0.62f, 1.72f, 0.035f}, {0.34f, 0.3f, 0.006f}, {0, 0, -0.04f});
    s.box("#7c9a6a", {0.58f, 1.74f, 0.04f}, {0.12f, 0.1f, 0.002f}).box("#5b7fa6", {0.68f, 1.68f, 0.04f}, {0.03f, 0.2f, 0.002f}, {0, 0, 0.5f}).box("#b91c1c", {0.55f, 1.66f, 0.042f}, {0.025f, 0.025f, 0.002f});
    g_board = uploadMesh(s.build(true));
  }
  {
    // one task note: aged paper with handwritten lines, a red wax seal and an iron pin
    Shape n;
    n.box("#f3ead2", {0, 0, 0}, {0.26f, 0.34f, 0.006f});
    n.box("#e6d8b5", {0, -0.15f, 0.001f}, {0.26f, 0.04f, 0.006f}, {0.25f, 0, 0});  // curled bottom edge
    n.box("#3f3a36", {0, 0.12f, 0.005f}, {0.16f, 0.014f, 0.002f});
    for (int l = 0; l < 6; l++) n.box("#57534e", {-0.01f, 0.07f - l * 0.035f, 0.005f}, {0.2f - (l % 3) * 0.04f, 0.007f, 0.002f});
    n.ball("#991b1b", {0.08f, -0.13f, 0.007f}, {0.025f, 0.025f, 0.006f}, {0, 0, 0}, 1);
    n.ball("#374151", {0, 0.15f, 0.012f}, {0.012f, 0.012f, 0.008f}, {0, 0, 0}, 0);
    g_note = uploadMesh(n.build(true));
    Shape c;  // accepted mark: a red brush tick
    c.box("#b91c1c", {-0.03f, -0.02f, 0.008f}, {0.03f, 0.09f, 0.002f}, {0, 0, 0.6f}).box("#b91c1c", {0.04f, 0.02f, 0.008f}, {0.03f, 0.16f, 0.002f}, {0, 0, -0.55f});
    g_tick = uploadMesh(c.build(true));
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
// bandits on the MakeHuman body: dark clothes, hood, blood-red face cloth, quiver; the bow rides the left hand
void drawBanditsMh(Shader& s) {
  struct Item { Mat4 base; mh::Pose pose; float yaw, pull; bool alert, drawing, dead = false; Vec3 pos; };
  std::vector<Item> items;
  for (auto& b : g_bandits) {
    if (b.hp <= 0 && b.deadT > 32) continue;
    float y = world::getTerrainHeight(b.x, b.z);
    if (!render::sphereVisible({b.x, y + 1, b.z}, 1.6f)) continue;
    Item it;
    it.base = Mat4::compose({b.x, y, b.z}, Quat::axisAngle({0, 1, 0}, b.yaw), {1, 1, 1});
    if (b.hp <= 0) {
      // death: a throe (head snaps back, arms fly out), the knees go and the body falls back under gravity with a
      // small bounce, then lies spread-eagled; after ~25 s it sinks away
      float t = b.deadT;
      float throe = std::min(1.0f, t / 0.3f);
      float u = clampf((t - 0.25f) / 0.75f, 0, 1), fall = u * u;   // accelerates like a real fall
      float bounce = t > 1.0f ? std::sin(std::min(1.0f, (t - 1.0f) / 0.3f) * PI) * 0.06f : 0.0f;
      float ang = (PI * 0.5f - 0.04f) * fall - bounce;
      float sink = clampf((t - 25) / 6, 0, 1) * 0.45f;
      it.base = Mat4::compose({b.x, y + 0.13f * fall - sink, b.z}, Quat::axisAngle({0, 1, 0}, b.yaw) * Quat::axisAngle({1, 0, 0}, -ang), {1, 1, 1});
      it.dead = true; it.pull = 0; it.alert = false; it.drawing = false; it.yaw = b.yaw; it.pos = {b.x, y, b.z};
      mh::Pose& P = it.pose;
      P.headPitch = -0.55f * throe * (1 - 0.7f * fall);
      P.armOutL = P.armOutR = 0.5f * throe + 0.6f * fall;
      P.armL = P.armR = 0.9f * throe * (1 - fall) + 0.3f * fall;
      P.elbowL = P.elbowR = 0.3f;
      P.lean = -0.15f * throe * (1 - fall);
      items.push_back(it);
      continue;
    }
    it.pull = b.drawT > 0 ? std::min(1.0f, b.drawT / (DRAW_TIME * 0.7f)) : 0.0f;
    it.alert = b.alert; it.drawing = b.drawT > 0; it.yaw = b.yaw; it.pos = {b.x, y, b.z};
    mh::Pose& P = it.pose;
    P.walkPhase = b.phase;
    P.walk = std::min(1.0f, b.speed * 0.4f);
    P.speed = b.speed;
    if (b.alert) {
      P.armL = 1.5f; P.armOutL = 0.25f; P.elbowL = -0.55f;                 // bow arm straight at the target
      P.armR = 1.5f; P.armOutR = 0.15f; P.elbowR = -0.5f; P.foldR = 1.0f + 1.2f * it.pull;  // string hand drawn back to the cheek
      P.headPitch = 0.05f;
    } else {
      P.armL = 0.15f; P.elbowL = 0.4f;  // bow held low while walking the camp
    }
    items.push_back(it);
  }
  if (items.empty()) return;
  float cx = render::cam.pos.x, cz = render::cam.pos.z;
  mh::begin(false);
  for (auto& it : items) {
    mh::Look L;
    L.kind = mh::BANDIT;
    L.skin = 0.82f;
    L.tunic = {0.024f, 0.019f, 0.016f}; L.trousers = {0.04f, 0.032f, 0.026f}; L.shoes = {0.01f, 0.008f, 0.006f};
    L.sash = {0.2f, 0.02f, 0.015f}; L.cap = {0.02f, 0.02f, 0.02f};
    mh::draw(it.base, L, it.pose, hypot2(it.pos.x - cx, it.pos.z - cz), false);
  }
  mh::end();
  s.use();
  const Mesh& bow = people::banditBow();
  for (auto& it : items) {
    if (it.dead) continue;  // the bow fell with him
    Vec3 hand = mh::jointWorld(it.base, it.pose, "hand_l");
    Vec3 fwd{std::sin(it.yaw), 0, std::cos(it.yaw)}, side{std::cos(it.yaw), 0, -std::sin(it.yaw)}, up{0, 1, 0};
    if (!it.alert) { fwd = {0, -1, 0}; up = {std::sin(it.yaw), 0, std::cos(it.yaw)}; }  // lowered: bow points along the walk
    // the bow mesh is built in the old arm space: x side, -y along the arm (forward), z along the limbs (up)
    Mat4 m;
    m.m[0] = side.x; m.m[1] = side.y; m.m[2] = side.z;
    m.m[4] = -fwd.x; m.m[5] = -fwd.y; m.m[6] = -fwd.z;
    m.m[8] = up.x; m.m[9] = up.y; m.m[10] = up.z;
    Vec3 grip{0, -0.672f, 0.078f};
    Vec3 rg{side.x * grip.x - fwd.x * grip.y + up.x * grip.z, side.y * grip.x - fwd.y * grip.y + up.y * grip.z, side.z * grip.x - fwd.z * grip.y + up.z * grip.z};
    m.m[12] = hand.x - rg.x; m.m[13] = hand.y - rg.y; m.m[14] = hand.z - rg.z;
    render::setModel(s, m);
    bow.draw();
    if (it.drawing) {  // nocked arrow from the string hand to the bow
      Vec3 hr = mh::jointWorld(it.base, it.pose, "hand_r");
      Vec3 mid = (hand + hr) * 0.5f;
      Vec3 d = hand - hr; d.normalize();
      render::setModel(s, Mat4::compose(mid, Quat::fromUnitVectors({0, 0, 1}, d), {1, 1, 1}));
      g_arrowMesh.draw();
    }
  }
}

void drawBandits(Shader& s) {
  if (!g_arrowMesh.valid()) {
    Shape a;
    a.rod("#8b6a45", {0, 0, -0.38f}, {0, 0, 0.36f}, 0.008f, 0.008f, 4)
        .cone("#4b5563", {0, 0, 0.4f}, 0.016f, 0.06f, 4, {PI / 2, 0, 0})
        .box("#78716c", {0, 0.012f, -0.33f}, {0.003f, 0.022f, 0.1f})
        .box("#78716c", {0.012f, 0, -0.33f}, {0.022f, 0.003f, 0.1f});
    g_arrowMesh = uploadMesh(a.build(true));
  }
  if (mh::ready()) { drawBanditsMh(s); return; }
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
        for (auto& h : hp->arr) { g_bandits[k].hp = (float)h.n; if (h.n <= 0) g_bandits[k].deadT = 99; k++; }  // no replayed deaths after a load
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
void debugTrophies() { for (auto* id : TROPHY_IDS) g_trophy[id] = id[0] == 'd' || id[0] == 'w' ? 2 : 1; }
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
    bd.hp -= 0.5f * dmg * (head ? 2.0f : 1.0f);  // bandits take half damage
    bd.alert = true; bd.dodgeT = 0.45f; bd.dodgeDir = randf() < 0.5f ? -1.0f : 1.0f; bd.dodgeCd = 1.2f;  // flinch away from the next shot
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
  if (!g_banditOn) for (auto& b : g_bandits) if (b.hp <= 0) b.deadT += dt;  // camp cleared: bodies still settle and fade
  if (g_banditOn) {
    // bandit archers: wander the camp; when you come near they spread out, keep their distance, strafe, plant their
    // feet to draw and loose real arrows at you (aimed, with spread that tightens as the camps get tougher)
    const Vec3& p = E.player.pos;
    CampTier T = tier(g_campLevel);
    float pCamp = hypot2(p.x - g_campX, p.z - g_campZ);
    // player velocity (for leading shots) and where the player is aiming
    static Vec3 lastP = p; Vec3 pv = dt > 0 ? (p - lastP) * (1.0f / dt) : Vec3{}; lastP = p;
    if (pv.length() > 12) pv = Vec3{};
    float aimX = std::sin(E.player.yaw) * -1.0f, aimZ = std::cos(E.player.yaw) * -1.0f;
    { Vec3 f = render::cam.forward(); float fl = hypot2(f.x, f.z); if (fl > 0.01f) { aimX = f.x / fl; aimZ = f.z / fl; } }
    // the band fights together: one sees you or is hit, all of them turn on you
    bool camp = false;
    for (auto& b : g_bandits) if (b.hp > 0 && (b.alert || hypot2(p.x - b.x, p.z - b.z) < T.range + 14)) camp = true;
    int alive = 0; for (auto& b : g_bandits) if (b.hp > 0) alive++;
    int slot = 0;
    for (size_t i = 0; i < g_bandits.size(); i++) {
      Bandit& b = g_bandits[i];
      if (b.hp <= 0) { b.deadT += dt; continue; }
      float dx = p.x - b.x, dz = p.z - b.z, d = hypot2(dx, dz);
      float fromCamp = hypot2(b.x - g_campX, b.z - g_campZ);
      b.alert = camp && pCamp < 110 && E.playing;
      float mx = 0, mz = 0, spd = 0;
      if (b.alert) {
        float keep = T.range * 0.6f;
        float ux = dx / std::max(d, 0.01f), uz = dz / std::max(d, 0.01f);
        b.strafeT -= dt; b.dodgeCd -= dt;
        if (b.strafeT <= 0) { b.strafe = randf() < 0.5f ? -1.0f : 1.0f; b.strafeT = 0.8f + randf() * 1.4f; }
        // flank: each bandit holds its own bearing around the player so they spread out and surround
        float spreadA = (slot - (alive - 1) * 0.5f) * 0.55f;
        float ca = std::cos(spreadA), sa = std::sin(spreadA);
        float hx = -ux * ca + uz * sa, hz = -uz * ca - ux * sa;   // from the player toward this bandit's post
        float postX = p.x + hx * keep - b.x, postZ = p.z + hz * keep - b.z, postD = hypot2(postX, postZ);
        if (postD > 4) { mx = postX / postD; mz = postZ / postD; spd = postD > 14 ? 4.6f : 3.6f; }
        else { mx = -uz * b.strafe; mz = ux * b.strafe; spd = 3.0f; }
        if (d < keep * 0.5f) { mx = -ux; mz = -uz; spd = 4.2f; }      // too close: back off fast
        // evade: when the player draws a bow on this bandit, sidestep hard (not mid-draw, with a cooldown)
        float onTarget = -(aimX * ux + aimZ * uz);
        if (E.bow.drawing && onTarget > 0.985f && b.dodgeCd <= 0 && b.drawT <= 0) {
          b.dodgeT = 0.5f; b.dodgeCd = 1.6f + randf() * 1.2f; b.dodgeDir = randf() < 0.5f ? -1.0f : 1.0f;
        }
        if (b.dodgeT > 0) { b.dodgeT -= dt; mx = -uz * b.dodgeDir; mz = ux * b.dodgeDir; spd = 6.0f; b.drawT = 0; }
        if (fromCamp > 75) { mx = (g_campX - b.x) / fromCamp; mz = (g_campZ - b.z) / fromCamp; spd = 3.6f; }
        b.yaw = std::atan2(dx, dz);
        b.cd -= dt;
        if (b.drawT > 0) {
          spd = 0;  // feet planted while drawing
          b.drawT += dt;
          if (b.drawT >= DRAW_TIME) {
            b.drawT = 0;
            b.cd = T.cd + randf() * 0.5f;
            // loose: lead the moving player, aim at the chest with a spread that tightens with the camp level
            Vec3 from{b.x + std::sin(b.yaw) * 0.55f + std::cos(b.yaw) * 0.2f, world::getTerrainHeight(b.x, b.z) + 1.35f, b.z + std::cos(b.yaw) * 0.55f - std::sin(b.yaw) * 0.2f};
            float sp = 40.0f, lead = std::min(1.0f, 0.55f + 0.08f * g_campLevel);
            Vec3 tgt = p + pv * ((p - from).length() / sp * lead);
            float spread = (1.5f - 0.17f * g_campLevel) * (0.35f + d / 45.0f);
            float ea = randf() * TAU, er = std::sqrt(randf()) * spread;
            Vec3 to{tgt.x + std::cos(ea) * er, p.y - 0.55f + std::sin(ea) * er * 0.6f, tgt.z + std::sin(ea) * er};
            Vec3 dir = to - from;
            float L = dir.length(), tt = L / sp;
            Vec3 v = dir * (sp / std::max(L, 0.01f));
            v.y += 0.5f * 9.8f * tt;  // lob to cancel the drop
            g_barrows.push_back({from, v, 3.0f, false});
            audio::tone(Wave::Triangle, 700, 200, 0.18f, 0.04f);  // bowstring twang
          }
        } else if (b.cd <= 0 && d < T.range && b.dodgeT <= 0) b.drawT = 0.001f;
        slot++;
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
      b.phase += dt * TAU * (b.speed > 0.05f ? 0.85f + 0.3f * b.speed : 0.0f);  // stride cadence matched to speed (no foot sliding)
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
      Mat4 bm = Mat4::compose({bx, by, bz}, Quat::axisAngle({0, 1, 0}, std::atan2(-bx, -bz)), {1, 1, 1});  // faces the hearth
      render::setModel(s, bm);
      g_board.draw();
      for (size_t i = 0; i < g_bounties.size() && i < 3; i++) {  // one pinned note per posted task, ticked once accepted
        Mat4 nm = bm * Mat4::compose({-0.25f + (float)i * 0.28f, 1.38f + (i & 1) * 0.06f, 0.04f}, Quat::axisAngle({0, 0, 1}, ((float)i - 1) * 0.06f), {1, 1, 1});
        render::setModel(s, nm);
        g_note.draw();
        if (g_bounties[i].accepted) g_tick.draw();
      }
    }
  }
  // trophy rack beside the hearth, filled with what you've mounted
  int total = 0;
  for (auto& kv : g_trophy) total += kv.second;
  {
    float rx = -6.5f, rz = 3.0f, ry = world::getTerrainHeight(rx, rz);
    if (render::sphereVisible({rx, ry + 1.8f, rz}, 3.5f)) {
      Mat4 base = Mat4::compose({rx, ry, rz}, Quat::axisAngle({0, 1, 0}, 1.2f), {1, 1, 1});
      render::setModel(s, base);
      g_rack.draw();
      int slot = 0;
      for (int i = 0; i < 5 && slot < 8; i++)
        for (int k = 0; k < mounted(TROPHY_IDS[i]) && slot < 8; k++, slot++) {
          float lx = -1.15f + (slot % 4) * 0.77f, ly = slot < 4 ? 2.05f : 1.05f;
          render::setModel(s, base * Mat4::compose({lx, ly, 0.06f}, Quat::axisAngle({0, 0, 1}, 0.03f * (slot % 3 - 1)), {1, 1, 1}));
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
  } else if (!g_bandits.empty() && mh::ready()) {
    s.use();
    drawBanditsMh(s);  // the last of the band still falling / lying after the camp is cleared
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
