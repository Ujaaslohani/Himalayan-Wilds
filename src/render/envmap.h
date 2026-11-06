#pragma once
// Precomputed environment: baked once at load from the heightfield, rivers, lakes and forest density.
//  env (unit 13, RGBA16): r = regional haze density, g = ground-mist strength, b = sky visibility (terrain AO x canopy), a = ground height
//  horizon (units 14, 15, RGBA8): highest terrain angle in 8 compass directions -> mountain shadows at any sun angle
#include <vector>
namespace envmap {
// detail map (unit 16, RGBA8, ~1.2 m texels): r = ground contact shade (1 open, 0 fully shaded) under trees, rocks
// and buildings; g = baked firelight from fixed lamps and campfires; b = light height;
// a = grass bare-earth noise (precomputed for the grass shader; 1 under buildings)
struct Stamp { float x, z, hx, hz, rot, k; bool circle; };
struct FireLight { float x, y, z, power, radius; };
void setDetailSources(const std::vector<Stamp>& occluders, const std::vector<FireLight>& lights);  // rebakes only when changed
void build();
void bind();
float hazeAt(float x, float z);
// baked area data for sound and gameplay (CPU copies of the env map)
float canopyAt(float x, float z);   // forest cover 0..1, smoothed
float openAt(float x, float z);     // sky visibility 0..1 (low in gorges, under canopy)
float wetAt(float x, float z);
float bareAt(float x, float z);
// water flow (baked once): current direction and speed in m/s at a point; zero on land and on still lakes
void flowAt(float x, float z, float& vx, float& vz);
// walking surface (baked once): what the ground under a footstep is made of
enum Surface { SURF_GRASS = 0, SURF_EARTH, SURF_STONE, SURF_WATER };
Surface surfaceAt(float x, float z);     // grass bare-earth noise 0..1 (same as the grass shader reads); thread-safe once ready      // closeness to open water / marsh 0..1  // regional haze 0..1 (CPU side, for culling)
float hazeCullDistance(float px, float pz, float dt);  // call once per frame
inline float g_cullQ = 230;  // last value, quantised to 10 m  // smoothed distance beyond which the haze hides everything
}
