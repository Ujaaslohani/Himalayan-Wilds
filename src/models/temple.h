#pragma once
#include "shape.h"
#include "../world/terrain.h"
#include <vector>

constexpr float PLINTH_HALF = 7.2f;
constexpr float PLINTH_H = 1.3f;
constexpr float COURT = 13.0f;

namespace temple {
const Mesh& model();
const Mesh& glow();
const Mesh& modelLod();
const std::vector<Vec3>& lights();
float ground(float x, float z, float h);
void resolveCollision(float& x, float& z, float radius = 0.35f);
const Shrine* nearShrine(float x, float z, float r = 8);
std::vector<Vec3> lightSpots();
void shutdown();
}
