#pragma once
#include "engine.h"
#include <string>
#include <vector>

struct SpotCheck { bool ok; std::string reason; float x, y, z, rot; };
struct StageInfo { int level; std::string name; std::vector<std::string> next; };
extern const char* STAGE_NAMES[3];

float structureRadius(StructureKind kind);
SpotCheck checkStructureSpot(StructureKind kind, float x, float z, float rot, int ignoreUid);
void resolveStructureCollision(float& x, float& z, float radius = 0.35f);
StageInfo computeStage();

namespace village {
void syncResidents();
void resetResidents();
void assignJobs();
int residentCount();
void updateResidents(float dt);
void structuresChanged();
void initRender();
void drawStructures();
void drawResidents();
void drawShadow();
void drawShadowStatic();
void addLights();
void drawFestival();
}
