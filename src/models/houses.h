#pragma once
#include "shape.h"
#include "../game/engine.h"
#include <vector>
#include <string>

enum KindGroup { KG_HOME, KG_VILLAGE, KG_DECOR };
enum LookOpt { LO_ROOF, LO_WALL, LO_WOOD, LO_DOOR, LO_ACCENT, LO_DECO, LO_COUNT };

struct KindInfo {
  const char* id;
  const char* name;
  KindGroup group;
  const char* blurb;
  Cost cost;
  int capacity;
  int appeal;
  float radius;
  std::vector<LookOpt> options;
};

struct BuiltModel {
  const Mesh* mesh = nullptr;
  std::vector<Vec3> lights;
  Vec3 door;
};

extern const KindInfo KINDS[SK_COUNT];
extern const char* const LOOK_LABELS[LO_COUNT][7];
extern const int LOOK_COUNTS[LO_COUNT];
extern const char* const LOOK_OPT_NAMES[LO_COUNT];

const BuiltModel& structureModel(StructureKind kind, const Look& L);
void pruneStructureModels(const std::vector<std::pair<StructureKind, Look>>& keep);
StructureKind kindFromId(const std::string& id);
void shutdownHouses();
