#include "tutorial.h"
#include "engine.h"
#include <cmath>

namespace {
struct Step { const char* text; };
const Step STEPS[] = {
    {"Move with W A S D and look around with the mouse. Hold Shift to run."},
    {"Follow the gold compass marker to the deer. Hold left mouse to draw, release to shoot; right mouse aims."},
    {"Walk up to the downed deer and press E to harvest it."},
    {"Gather timber: walk to a tree marked on the compass and press E."},
    {"Press B to open the Build menu and place a Cottage (Homes) near the fire."},
    {"Settlers move into free beds. Open Build > Villagers to give them jobs."},
};
const int N = sizeof(STEPS) / sizeof(STEPS[0]);
int g_step = N;
float g_t = 0, g_shown = 0;
Vec3 g_start;
int g_wood0 = 0;
}  // namespace

namespace tutorial {
void start() { g_step = 0; g_t = 0; g_shown = 0; g_start = E.player.pos; g_wood0 = invCount("wood"); }
void restore(int s) { g_step = s < 0 ? N : s; g_t = 0; g_shown = 0; g_start = E.player.pos; g_wood0 = invCount("wood"); }
int step() { return g_step; }

void update(float dt) {
  if (g_step >= N || A.status != Status::Playing) return;
  g_t += dt;
  g_shown += dt;
  if (g_t > 900) { g_step = N; return; }  // after 15 minutes hints stop by themselves
  bool done = false;
  switch (g_step) {
    case 0: done = hypot2(E.player.pos.x - g_start.x, E.player.pos.z - g_start.z) > 6; break;
    case 1: done = E.bow.lastShotPower > 0 || invCount("deer_pelt") > 0; break;
    case 2: done = invCount("deer_pelt") > 0 || invCount("food") > 20; break;
    case 3: done = invCount("wood") > g_wood0; break;
    case 4: done = !A.structures.empty(); break;
    case 5: {
      int jobs = 0;
      for (int j = 0; j < 4; j++) jobs += A.jobs[j];
      done = jobs > 0 || (A.population > 0 && g_shown > 40);
      break;
    }
  }
  if (g_step == 3) g_wood0 = std::min(g_wood0, invCount("wood"));  // spending wood doesn't count as gathering
  if (done) { g_step++; g_shown = 0; }
}

const char* hint() {
  if (g_step >= N || A.status != Status::Playing) return nullptr;
  if (g_step == 5 && A.population == 0) return nullptr;  // wait until someone has moved in
  return STEPS[g_step].text;
}
}  // namespace tutorial
