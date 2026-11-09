#pragma once
#include "../core/math.h"

namespace sky {
void init();
void update(float dt);
void drawSky();
void drawStarsAndMist();
void drawWeather(float dt);
Vec3 lightDir();
Vec3 sunDir();
Mat4 shadowMatrix();
bool shadowNeedsUpdate();
bool shadowStaticDirty();
void clearStaticShadow();
bool shadowScroll(int& dx, int& dy, float& dz);  // static cache must shift by (dx, dy) texels and dz depth
void clearShadowScroll();
void invalidateStaticShadow();
void onStaticRebuild();
Color horizon();
void shutdown();
void scareBirds(const Vec3& from);
void warmPrograms();
float rainAmount();
float blizzard();  // 0..1 winter blizzard strength
float fogBank();   // 0..1 morning valley fog  // birds burst out of nearby trees
}
