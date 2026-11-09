#pragma once
#include <string>
#include <vector>
#include "engine.h"
#include "../core/json.h"

// Sleep, bounty board, trophy rack, bandit camps, stats and achievements.
namespace extras {
void reset();
void save(Json& d);
void load(const Json& d);
void update(float dt);
void draw();

// sleep (at your own home's door, or the hearth bedroll before you have one)
bool sleepSpot(float x, float z);
std::string sleepBlocked();     // empty when sleeping is allowed now
void sleep();
float sleepFade();              // 0..1 black overlay

// bounty board
struct Bounty { int kind; std::string item; int species; int need, have; std::string text; int renown; Cost bonus; bool accepted = false; };
const std::vector<Bounty>& bounties();
void deliver(int i);    // turn in (only at the board)
void accept(int i);
bool nearBoard(float x, float z);
void boardPos(float& x, float& z);
void onKill(int species);

// trophy rack
int mounted(const std::string& id);
void mount(const std::string& id);
int trophyAppeal();

// bandits
bool banditsActive();
bool banditCamp(float& x, float& z);
int banditLevel();
int banditHeads(float* xyz, int maxN);  // living bandits: world position just above each head
void debugBandits(float x, float z, int level);
void debugTrophies();  // selftest: one of every trophy mounted  // selftest: put a camp of the given level here  // 1..6 (difficulty of the current / last camp)
bool arrowHit(const Vec3& a, const Vec3& b, float dmg);

// stats + achievements
void stat(const std::string& key, double add = 1);
double getStat(const std::string& key);
struct Achievement { const char* name; const char* desc; bool got; };
std::vector<Achievement> achievements();
}
