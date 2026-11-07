#pragma once
#include "shape.h"
#include <string>

constexpr float PEOPLE_LIFT = 0.24f;
constexpr float BODY_SCALE = 0.88f;
extern const Vec3 PIV_HEAD, PIV_ARM, PIV_LEG;

struct PersonModel {
  Mesh torso, head, arm, leg;
};

struct ResidentKit {
  Mesh torsos[3], heads[3], arm, leg;
};

namespace people {
const PersonModel& person(const std::string& role);
const ResidentKit& residentKit();
const PersonModel& bandit();
// a specialist's clothing colours (for the MakeHuman body): coat, trousers, sash, cap band, shoes
struct OutfitHex { const char *coat, *trousers, *sash, *cap, *shoe; bool female, elder; };
OutfitHex outfitHex(const std::string& role);   // villager body, dark clothes, hood, quiver, blood-red face cloth, grim face
const Mesh& banditBow();       // drawn with the left arm's matrix
void shutdown();
}
