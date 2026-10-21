#pragma once
#include "../core/math.h"
#include "engine.h"
#include <string>
#include <vector>

enum Species { SP_DEER, SP_BOAR, SP_GORAL, SP_WOLF, SP_BEAR, SP_LEOPARD, SP_COUNT };

struct Herd;
struct Animal {
  int id;
  Species sp;
  Herd* herd;
  float x, y, z, yaw, speed;
  int state;
  float t, hp, tx, tz, phase, pitch, aware, fall, downT, steerYaw, steerT;
  bool charged;
  float slow, cd;
  Animal* prey;
  float leap, rear, hunger, lunge;
  bool raid;
  float gaitT, gaitV;
};

struct ArrowHit { Animal* animal; bool headshot; };
struct CarcassYield { std::string name; Cost yields; };

namespace wildlife {
void spawn(uint32_t seed = 7);
void update(float dt);
bool arrowHitTest(const Vec3& p0, const Vec3& p1, ArrowHit& out);
void damage(Animal* a, float dmg, bool headshot);
Animal* findCarcass(float x, float z, float r);
CarcassYield harvestCarcass(Animal* a);
bool nearestHerd(Species sp, float x, float z, float& ox, float& oz);
const char* speciesName(Species sp);
bool isPredator(Species sp);
void initRender();
void draw();
void drawShadow();
void drawBlobs();
int animalCount();
}
