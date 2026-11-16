#pragma once
// Skill tree: three branches of four or five skills (removed skills left in old saves are ignored and their points come back). Points come from renown levels (and the caravan's teachings); owned
// skills are stored in the inventory as "sk_<id>" so saves carry them without a format change.
namespace skills {
struct Def { const char *id, *name, *desc; int branch, tier, cost; };
constexpr int COUNT = 13, BRANCHES = 3, TIERS = 5;
extern const Def DEFS[COUNT];
extern const char* const BRANCH_NAMES[BRANCHES];
extern const char* const BRANCH_BLURB[BRANCHES];
bool has(const char* id);
bool owned(int i);
int level();                 // renown level
int levelFloor(int L);       // renown needed to reach level L
int spent();
int points();                // unspent
bool canBuy(int i);          // prerequisite met and affordable
bool locked(int i);          // prerequisite missing
bool buy(int i);
void grantPoint(int n);      // bonus points (caravan teachings)

// effects, read by the systems they change
float drawTime();            // seconds to full draw, multiplier
float arrowMult();
float sneakMult();           // damage on a target that has not noticed you
float headMult();            // headshot multiplier
float damageTaken();
float regenDelay();          // seconds without damage before health returns
float regenMult();
float noiseMult();           // how far animals hear you
int gatherExtra();
float banditSight();
int peltExtra();
float spotTime();            // seconds in a bandit's sight before he raises the alarm
}
