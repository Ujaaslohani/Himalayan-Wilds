#pragma once

#include "../core/math.h"
// the grotto behind the waterfall (built with the falls; game/secrets.cpp dresses it)
struct CaveFrame {
  Vec3 center, front;  // floor centre (at water level), middle of the front edge
  float yaw;           // facing out of the cave
  float depth, half;   // metres in from the front edge, half width
  float roofY, floorY;
  float roofLen;       // front edge to where the cliff rises above the roof
};
namespace water {
CaveFrame caveFrame();
void init();
void draw();
void setReflection(unsigned tex);   // half-res scene colour (rgb) + linear depth (a) for screen-space reflections; 0 = off
void drawParticles();
// a disturbance on the water surface: expanding ripple rings, plus a burst of droplets when splash > 0
void ripple(float x, float y, float z, float strength, float splash = 0);
void shutdown();
}
