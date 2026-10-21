#pragma once
#include "../core/math.h"

namespace archery {
void fire(const Vec3& camPos, const Vec3& camDir, float power, bool aimed);
void update(float dt);
void initRender();
void draw();
void drawViewModel(float dt, const Mat4& camWorld);
void reset();
}
