#pragma once
#include "engine.h"
#include <string>
#include <vector>

struct SpotCheck { bool ok; std::string reason; float x, y, z, rot; };
struct StageInfo { int level; std::string name; std::vector<std::string> next; };
extern const char* STAGE_NAMES[5];
float tierBonus();
bool roofAt(float x, float z);  // under a village roof (for sound muffling)  // production multiplier from the settlement tier (+8% per tier)

float structureRadius(StructureKind kind);
SpotCheck checkStructureSpot(StructureKind kind, float x, float z, float rot, int ignoreUid);
void resolveStructureCollision(float& x, float& z, float radius = 0.35f);
StageInfo computeStage();

namespace village {
void syncResidents();
void resetResidents();
void assignJobs();
int residentCount();
void snapshotResidents();
void setResidentAlpha(float a);
void refreshAutoJobs();
int autoJobCount(int j);
int nearPositions(float x, float z, float r, float* out, int maxN);
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
