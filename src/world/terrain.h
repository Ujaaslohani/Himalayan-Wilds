#pragma once
#include "../core/math.h"
#include "../gfx/gfx.h"
#include <vector>
#include <string>
#include <array>

constexpr float MAP_HALF = 600.0f;
constexpr float SETTLEMENT_RADIUS = 18.0f;
constexpr float PLATEAU = 19.0f;
constexpr int TERRAIN_SEGMENTS = 512;
constexpr float GRID_SIZE = MAP_HALF * 2 + 40;
constexpr float GRID_STEP = GRID_SIZE / TERRAIN_SEGMENTS;
constexpr float GRID_MIN = -GRID_SIZE / 2;
constexpr int GRID_N = TERRAIN_SEGMENTS + 1;

using P2 = std::array<float, 2>;

struct Summit { std::string name; float x, z, h, r; };

struct RiverHit {
  float d = 1e9f;
  float s = 0;
  float wy = -1e9f;
  float hw = 7;
  float depth = 1;
  float tx = 0, tz = 1;
  float cx = 0, cz = 0;
  float flow = 0;
};

struct River {
  std::vector<P2> pts;
  std::vector<float> cum;
  std::vector<float> wy, hw, depth, rapid;
  float len = 0;
  P2 head{0, 0};
};

struct Lake { std::string name; float x, z, r, level, depth; };

enum class FeatKind { Stairs, Gap, Pillars };
struct FeatSpec { FeatKind kind; float at; int n = 5; float w = 4; float pr = 0; };
struct Feat { FeatKind kind; float s, len, rise; int n; float w, pr; };

struct Route {
  std::string name;
  int peak;
  std::vector<P2> pts;
  std::vector<float> cum;
  float len, h0, h1, g, top;
  std::vector<Feat> feats;
  float minX, maxX, minZ, maxZ;
};

struct Road {
  std::vector<P2> pts;
  std::vector<float> cum, hs;
  float minX, maxX, minZ, maxZ;
};

struct Step { float x, z, dx, dz, hl, hw, top, bottom; };
struct Gap { float x, z, dx, dz, hl, base, rx, rz, yaw; bool pillars; };
struct Shrine { std::string name; float x, z, y, yaw; };

struct PathHit { float rd = 99, rh = 0, td = 99, th = 0, td2 = 99, th2 = 0, cap = 1e9f; };

namespace world {
extern std::vector<Summit> SUMMITS;
extern std::vector<Road> ROADS;
extern std::vector<Route> ROUTES;
extern std::vector<Step> STEPS;
extern std::vector<Gap> GAPS;
extern std::vector<Shrine> SHRINES;
extern std::vector<Lake> LAKES;
extern River RIVER;
extern P2 BRIDGE_SITE;
extern float BRIDGE_ROT;
extern P2 FORD_SITE;

void generate();
const std::vector<float>& heightGrid();
const std::vector<uint32_t>& colorGrid();

float getTerrainHeight(float x, float z);
float slopeAt(float x, float z);
float localSlope(float x, float z);
float valleyRadius(float x, float z);
float mountainFactor(float x, float z);
float forestDensity(float x, float z);
RiverHit riverQuery(float x, float z);
float riverMask(float x, float z);
float waterHeightAt(float x, float z);
int lakeAt(float x, float z, float margin = 0);
PathHit pathQuery(float x, float z);
float pathDistance(float x, float z);
float nearRoute(float x, float z);
float pathMask(float x, float z);
float routeHeightAt(const Route& r, float s);
void routePoint(const Route& r, float s, float& x, float& z, float& dx, float& dz);
float stepHeightAt(float x, float z, float h);
const Gap* gapAt(float x, float z, float feet);
Color terrainColor(float x, float z, float h, float normalY);
std::vector<std::vector<Vec3>> fenceLines();
MeshData buildBackdrop();
float terrainHeightRaw(float x, float z);
float naturalHeightAt(float x, float z);
}  // namespace world
