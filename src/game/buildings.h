#pragma once
#include "engine.h"

constexpr float SETTLEMENT_LIMIT = 85;  // starting build radius
// The settlement grows with what you build: the farthest building/structure from the hearth
// pushes out both the build radius and the zone wild animals keep out of.
float settlementExtent();  // farthest built building or structure (m from hearth), 0 if none
float settlementLimit();   // build radius: 85 m, growing to farthest building + 30 m (max 220)
float settlementSafe();    // animal-free zone: 40 m, growing to farthest building + 20 m (max 170)

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
