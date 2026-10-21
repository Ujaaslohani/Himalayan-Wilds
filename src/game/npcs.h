#pragma once
#include <string>
#include <vector>
#include <functional>

struct Camp { const char* id; float campX, campZ, homeX, homeZ; const char* area; };

struct NpcRt {
  std::string id, name;
  float x = 0, y = 0, z = 0, yaw = 0, headYaw = 0, phase = 0, speed = 0;
  int mode = 0;
  float tx = 0, tz = 0, t = 0;
  bool recruited = false;
  float travelT = 0;
};

namespace npcs {
extern const Camp CAMPS[4];
const Camp* camp(const std::string& id);
void sync();
void reset();
NpcRt* find(float x, float z, float r);
bool position(const std::string& id, float& x, float& z);
void update(float dt);
void initRender();
void draw();
void drawShadow();
void drawBlobs();
void forEach(const std::function<void(const NpcRt&)>& fn);
}
