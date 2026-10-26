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
void invalidateStaticShadow();
void onStaticRebuild();
Color horizon();
void shutdown();
}
