#pragma once
#include "../core/math.h"
// Blood trails, village festivals and the valley's hidden details (waterfall cave, mani stones, white deer,
// Migoi prints in fresh snow, the temple bell, the winter star alignment, the gorge echo, the ruined village,
// the one-day blue poppy). Found-state lives in extras stats, so it saves with the game.
namespace secrets {
void reset();
void update(float dt);
void draw();
void drawShadow();
void roofPerches(std::vector<Vec3>& out);   // v1.1.8: home roof tops (birds perch there)
void drawRacks();                                  // v1.1.3: chilli / maize drying racks beside homes
void drawSmoke();                                  // chimney smoke (transparent pass)
void drawCave();                                   // after the terrain: the tunnel behind the falls
bool bridgeDeck(float x, float z, float feetY, float& y);  // plank bridges over the outer river crossings
bool caveFloor(float x, float z, float feetY, float& y);  // walkable floor inside the cave
void onShot(float x, float z);                      // bow released here (gorge echo)
bool arrowStep(const Vec3& a, const Vec3& b);       // arrow segment: true if it struck the temple bell
void onWhiteDeerKilled();
int festivalToday();                                // 0 none, 1 harvest, 2 Losar
bool where(const char* what, float& x, float& y, float& z);  // selftest positions
}
