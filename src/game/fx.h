#pragma once
#include "../core/math.h"
#include "../gfx/gfx.h"
#include <vector>

namespace fx {
void init();
void beginFrame();
void blob(float x, float y, float z, float yaw, float sx, float sz);
void campfire(float x, float y, float z, float scale);
void glow(float x, float y, float z, float size);
void halo(float x, float y, float z, float size);  // wide light bloom in the haze (night)
void pool(float x, float y, float z, float r);
void breath(const Vec3& p, const Vec3& dir);
void mist(const Vec3& p);   // v1.1.10: rising river mist wisp   // v1.1.3: a puff of breath vapour (cold weather)
void ghost(const Mesh* mesh, const Mat4& m, bool ok, bool custom);
void drawBlobs();
void drawTransparent();
void drawGlows();
void basicMesh(const Mesh& mesh, const Mat4& model, const Color& c, float alpha, bool useVC, bool fog = true);
void shutdown();
}
