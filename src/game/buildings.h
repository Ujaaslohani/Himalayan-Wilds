#pragma once
#include "engine.h"

constexpr float SETTLEMENT_LIMIT = 85;

struct PlacementCheck { bool ok; std::string reason; float x, y, z, rot; };

PlacementCheck checkPlacement(const Building& b, float x, float z, float rot);
void resolveBuildingCollision(float& x, float& z, float radius = 0.35f);
float walkHeight(float x, float z);

void bridgeRailCollision(float& x, float& z, float y);
namespace buildings {
void draw();
void drawShadow();
void drawPreview();
}
