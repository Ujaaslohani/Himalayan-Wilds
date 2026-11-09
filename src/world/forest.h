#pragma once
#include <functional>
#include <string>
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
void seasonJumped();  // season changed abruptly (save loaded / new game): recapture distant tree pictures at once
void markDirty();
void drawOpaque();
void drawShadow();
void drawBlobs();
bool nearestRock(float x, float z, float& ox, float& oz, float& os);
void resolveTreeCollision(float& x, float& z, float radius = 0.35f, float feetY = -1e9f);  // rocks and logs below feetY are stepped over
float obstacleTop(float x, float z);  // top of a rock / stump / log under the point, or -1e9
void nearTrees(float x, float z, float r, std::vector<float>& out, int max);
bool treeTrunkAt(float x, float y, float z);
std::vector<AssetRequest> requiredFiles();
void shutdown();
int instanceCount();
// every placed item of a layer (0 grass, 1 undergrowth, 2 rocks, 3 trees) over the whole map, ignoring blockers' draw-time culling
bool obstacleAt(float x, float z, float r);
// false when a ridge hides the whole box from the camera (terrain-horizon occlusion, cached per 3 m of movement)
bool horizonVisible(const Vec3& mn, const Vec3& mx);  // would something r wide overlap a trunk, rock, stump or log here?
void precomputeItems();  // whole-map tree/rock/undergrowth tables (call once terrain and water exist)
void forEachItem(int layer, const std::function<void(float x, float y, float z, float scale, const std::string& file)>& f);
}
