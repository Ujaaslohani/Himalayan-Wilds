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
#include <algorithm>
#include <cmath>

namespace {
enum Ev { EV_TRADER, EV_HARVEST, EV_SICK, EV_FESTIVAL, EV_ROCKFALL, EV_STORM, EV_WANDERER, EV_LOSAR, EV_HARVESTFEST, EV_COUNT };
const char* NAMES[EV_COUNT] = {"Travelling trader", "Good harvest", "Sick livestock", "Festival day", "Rockfall", "Storm day", "Wanderer", "Losar", "Harvest festival"};
int g_active = -1, g_days = 0;
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
static Mesh g_trader, g_mule;
bool traderPos(float& x, float& y, float& z, float& yaw) {
  if (g_active != EV_TRADER) return false;
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
static void buildCaravan() {
  Shape t;  // trader: long maroon chuba robe, sash, fur hat, staff
  t.cone("#7f1d1d", {0, 0.0f, 0}, 0.36f, 1.25f, 10);
  t.tube("#7f1d1d", {0, 0.85f, 0}, 0.2f, 0.27f, 0.55f, 10);
  t.tube("#f59e0b", {0, 0.95f, 0}, 0.25f, 0.25f, 0.08f, 10);
  t.ball("#c68b59", {0, 1.55f, 0}, {0.13f, 0.15f, 0.13f});
  t.tube("#3f2a1c", {0, 1.63f, 0}, 0.16f, 0.15f, 0.1f, 10);
  t.cone("#fbbf24", {0, 1.72f, 0}, 0.13f, 0.18f, 8);
  t.rod("#78350f", {0.35f, 0.0f, 0.15f}, {0.35f, 1.75f, 0.15f}, 0.025f, 0.02f);
  g_trader = uploadMesh(t.build(true));
  Shape m;  // mule with saddle packs and a blanket
  m.ball("#6b5a48", {0, 1.0f, 0}, {0.32f, 0.32f, 0.62f});
  m.ball("#6b5a48", {0, 1.3f, 0.65f}, {0.14f, 0.16f, 0.3f}, {0.6f, 0, 0});
  m.ball("#57493b", {0, 1.42f, 0.88f}, {0.11f, 0.12f, 0.2f});
  m.cone("#3f342a", {0.07f, 1.55f, 0.82f}, 0.04f, 0.18f, 5);
  m.cone("#3f342a", {-0.07f, 1.55f, 0.82f}, 0.04f, 0.18f, 5);
  for (int i = 0; i < 4; i++) m.tube("#4b3f33", {(i & 1) ? 0.18f : -0.18f, 0, (i & 2) ? 0.38f : -0.38f}, 0.06f, 0.07f, 0.85f, 6);
  m.box("#1e3a8a", {0, 1.25f, 0}, {0.7f, 0.06f, 0.75f});
  m.box("#a16207", {0.42f, 1.05f, 0}, {0.22f, 0.38f, 0.55f});
  m.box("#92400e", {-0.42f, 1.05f, 0}, {0.22f, 0.38f, 0.55f});
  m.box("#d6cfc2", {0, 1.42f, -0.1f}, {0.45f, 0.25f, 0.4f});
  g_mule = uploadMesh(m.build(true));
}
void drawCaravan() {
  float x, y, z, yaw;
  if (!traderPos(x, y, z, yaw)) return;
  if (!g_trader.valid()) buildCaravan();
  Shader& s = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::None, OF_SOFT);
  float look = std::atan2(E.player.pos.x - x, E.player.pos.z - z);
  render::setModel(s, Mat4::compose({x, y, z}, Quat::axisAngle({0, 1, 0}, look), {1, 1, 1}));
  g_trader.draw();
  float c = std::cos(yaw), sn = std::sin(yaw);
  float mx = x + c * 1.6f, mz = z - sn * 1.6f;
  render::setModel(s, Mat4::compose({mx, world::getTerrainHeight(mx, mz), mz}, Quat::axisAngle({0, 1, 0}, yaw + 1.2f + 0.05f * std::sin(E.clock * 0.7f)), {1, 1, 1}));
  g_mule.draw();
}

void dawn() {
  // yesterday's event ends at dawn
  g_active = -1;
  g_work = 1.0f;
  if (A.population <= 0 && A.structures.empty()) return;
  g_days++;
  // seasonal festivals on fixed days: Losar (new year) opens winter, the harvest festival falls mid-autumn
  if ((season.index == 3 && season.dayOfSeason == 1) || (season.index == 2 && season.dayOfSeason == 4)) {
    bool losar = season.index == 3;
    g_active = losar ? EV_LOSAR : EV_HARVESTFEST;
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
  float chance = 0.12f + 0.1f * g_days + (A.stage >= 3 ? 0.08f : 0.0f);
  if (g_days < 7 && randf() > chance) return;  // a week never passes without something happening
  // pick an event that makes sense right now
  int pick = -1;
  for (int tries = 0; tries < 12 && pick < 0; tries++) {
    int e = (int)(randf() * EV_COUNT) % EV_COUNT;
    if (A.stage >= 3 && randf() < 0.25f) e = EV_TRADER;  // market towns draw more traders
    if (e == EV_TRADER && !hasKind(SK_MARKET_STALL)) continue;
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
