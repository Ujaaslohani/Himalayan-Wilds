#pragma once
#include "../gfx/gfx.h"
#include <functional>
#include <vector>

// Baked ambient occlusion: voxelise one or more meshes, then cast a fixed set of rays from every vertex.
// GLB meshes store the occlusion in the spare normal byte (n[3], read by the object shader as a_nrm.w);
// P3C4 meshes (village buildings) have it multiplied into the vertex colour.
struct AOGrid {
  std::vector<const MeshData*> meshes;
  float cell = 0.1f, maxDist = 1.0f;
  float groundY = -1e9f;                         // everything below is solid (the ground the asset stands on)
  std::function<float(float, float)> groundAt;  // optional terrain height instead of a flat ground
  void add(const MeshData& m) { meshes.push_back(&m); }
  void build(int maxRes = 64, float minCell = 0.0f);
  float occlusion(Vec3 p, Vec3 n, bool sphere) const;
  void applyGLB(MeshData& m, float strength, bool sphere) const;
  void applyColour(MeshData& m, float strength) const;
 private:
  Vec3 o{0, 0, 0};
  int nx = 0, ny = 0, nz = 0;
  std::vector<uint8_t> v;
  bool solid(Vec3 p) const;
};
