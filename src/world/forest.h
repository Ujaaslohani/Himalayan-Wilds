#pragma once
#include "../assets/assets.h"
#include "../core/math.h"
#include <vector>
#include <string>

struct Blocker { float x, z, r; };

namespace forest {
float detailLevel();
extern int (*profOpen)(const char*);
extern void (*profClose)(int);
void init();
float grassDensity(float x, float z);
void setBlockers(const std::vector<Blocker>& b);
void update(const Vec3& camPos, const Vec3& camFwd);
void markDirty();
void drawOpaque();
void drawShadow();
void drawBlobs();
bool nearestRock(float x, float z, float& ox, float& oz, float& os);
void resolveTreeCollision(float& x, float& z, float radius = 0.35f);
void nearTrees(float x, float z, float r, std::vector<float>& out, int max);
bool treeTrunkAt(float x, float y, float z);
std::vector<AssetRequest> requiredFiles();
void shutdown();
int instanceCount();
}
