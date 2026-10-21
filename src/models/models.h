#pragma once
#include "shape.h"
#include <string>
#include <map>

struct HitSphere { float y, r, zOff; };
struct HeadHit { float y, z, r; };

struct SpeciesModel {
  Mesh body, head, leg;
  Mesh upperF, lowerF, upperH, lowerH, footF, footH;
  float lenF1 = 0.3f, lenF2 = 0.3f, lenH1 = 0.3f, lenH2 = 0.3f, footLen = 0.05f;
  bool paw = false;
  Vec3 headPivot;
  Vec3 legPivots[4];
  HitSphere hitBody;
  HeadHit hitHead;
  float shadowRadius;
};

constexpr float BRIDGE_LENGTH = 22.0f;
constexpr float BRIDGE_DECK_Y = 1.1f;
float footprint(const std::string& type);

namespace models {
const SpeciesModel& species(int sp, bool lod = false);
const Mesh& building(const std::string& type);
const Mesh& log();
const Mesh& cairn();
const Mesh& arrow();
const Mesh& bow();
const Mesh& bowHand();
const Mesh& drawHand();
const Mesh& tent();
const Mesh& unitBox();
const Mesh& cone6();
const Mesh& cone5();
const Mesh& unitCylinder();
extern const Vec3 BOW_TIPS[2];
void shutdown();
}
