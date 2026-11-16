#include "skills.h"
#include "engine.h"
#include "app.h"
#include "../audio/audio.h"
#include <string>

namespace skills {

const char* const BRANCH_NAMES[BRANCHES] = {"Archer", "Survivor", "Woodsman"};
const char* const BRANCH_BLURB[BRANCHES] = {"The bow in your hands", "Staying on your feet", "Moving unseen, wasting nothing"};

const Def DEFS[COUNT] = {
    {"steady", "Steady Hand", "Draw the bow 20% faster.", 0, 0, 1},
    {"keen", "Keen Eye", "Arrows hit 15% harder.", 0, 1, 1},
    {"ambush", "Ambusher", "+50% damage to animals and bandits that have not noticed you.", 0, 2, 2},
    {"deadeye", "Deadeye", "Headshots deal 2.5x damage instead of 2x.", 0, 3, 3},

    {"hardy", "Hardy", "+20 maximum health.", 1, 0, 1},
    {"hide", "Thick Hide", "Take 20% less damage from every source.", 1, 1, 2},
    {"recovery", "Quick Recovery", "Health starts returning after 4 s instead of 6, and 50% faster.", 1, 2, 2},
    {"mountain", "Mountain Blood", "+30 maximum health.", 1, 3, 3},

    {"softstep", "Soft Step", "Animals hear you from 25% less far.", 2, 0, 1},
    {"forager", "Forager", "+1 from every timber, slate and herb harvest.", 2, 1, 1},
    {"shadow", "Shadow", "Bandits pick you out from 25% less far.", 2, 2, 2},
    {"skinner", "Skinner", "+1 pelt, hide or fur from every animal.", 2, 3, 2},
    {"ghost", "Ghost", "Bandits need 4.5 s to spot you instead of 3.", 2, 4, 3},
};

static std::string key(const char* id) { return std::string("sk_") + id; }
bool has(const char* id) { return invCount(key(id)) > 0; }
bool owned(int i) { return i >= 0 && i < COUNT && has(DEFS[i].id); }

int levelFloor(int L) { return 250 * L * (L + 1); }   // 500, 1500, 3000, 5000 ... (level 20 = 105,000)
int level() {
  int L = 0;
  while (L < 40 && A.score >= levelFloor(L + 1)) L++;
  return L;
}
int spent() {
  int s = 0;
  for (int i = 0; i < COUNT; i++) if (owned(i)) s += DEFS[i].cost;
  return s;
}
int points() { return level() + invCount("sk_bonus_point") - spent(); }
bool locked(int i) {
  if (DEFS[i].tier == 0) return false;
  for (int k = 0; k < COUNT; k++)
    if (DEFS[k].branch == DEFS[i].branch && DEFS[k].tier == DEFS[i].tier - 1) return !owned(k);
  return true;
}
bool canBuy(int i) { return !owned(i) && !locked(i) && points() >= DEFS[i].cost; }
bool buy(int i) {
  if (!canBuy(i)) { audio::crispClick(); return false; }
  actions::addItems({{key(DEFS[i].id), 1}});
  std::string id = DEFS[i].id;
  float hp = id == "hardy" ? 20.0f : id == "mountain" ? 30.0f : 0.0f;
  if (hp > 0) { E.player.maxHp += hp; E.player.hp += hp; }
  audio::coinPickup();
  actions::notify(std::string("Skill learned: ") + DEFS[i].name + ". " + DEFS[i].desc);
  return true;
}
void grantPoint(int n) { actions::addItems({{"sk_bonus_point", n}}); }

float drawTime() { return has("steady") ? 0.8f : 1.0f; }
float arrowMult() { return has("keen") ? 1.15f : 1.0f; }
float sneakMult() { return has("ambush") ? 1.5f : 1.0f; }
float headMult() { return has("deadeye") ? 2.5f : 2.0f; }
float damageTaken() { return has("hide") ? 0.8f : 1.0f; }
float regenDelay() { return has("recovery") ? 4.0f : 6.0f; }
float regenMult() { return has("recovery") ? 1.5f : 1.0f; }
float noiseMult() { return has("softstep") ? 0.75f : 1.0f; }
int gatherExtra() { return has("forager") ? 1 : 0; }
float banditSight() { return has("shadow") ? 0.75f : 1.0f; }
int peltExtra() { return has("skinner") ? 1 : 0; }
float spotTime() { return has("ghost") ? 4.5f : 3.0f; }

}  // namespace skills
