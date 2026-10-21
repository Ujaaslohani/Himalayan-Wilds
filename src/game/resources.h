#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum ResType { RES_WOOD, RES_STONE, RES_FIBER, RES_HERB };

struct ResInfo { const char* name; const char* item; int amount, harvests; float respawn; int icon; };
extern const ResInfo RES_INFO[4];

struct ResNode {
  int id;
  ResType type;
  float x, y, z, rot, scale;
  int left;
  float regrowAt;
};

struct HarvestResult { std::string item; int amount; std::string name; bool depleted; };

namespace resources {
void spawn(uint32_t seed = 11);
const std::vector<ResNode>& nodes();
ResNode* find(float x, float z, float r);
const ResNode* nearestOfType(ResType t, float x, float z);
HarvestResult harvest(ResNode& n);
void update();
void clearAround(float x, float z, float r);
void initRender();
void draw();
void drawShadow();
void shutdown();
std::vector<std::string> requiredFiles();
}
