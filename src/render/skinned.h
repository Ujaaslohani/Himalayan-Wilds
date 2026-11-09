#pragma once
// Skinned, animated models (assets/animals/*.anim, converted offline from glTF: mesh, skeleton and clips resampled to
// 30 fps). Bones are posed on the CPU (a few dozen per animal) and the mesh is skinned on the GPU.
#include "../core/math.h"
#include <string>
#include <vector>

namespace skinned {
constexpr int MAX_BONES = 52;

struct Clip {
  std::string name;
  float fps = 30;
  int frames = 0;
  bool loop = true;
  std::vector<float> keys;  // [frame][joint][10]: t xyz, q xyzw, s xyz
  float duration() const { return frames / fps; }
};

struct Model {
  mutable float spots = 0;  // procedural leopard rosettes (cells per model unit), 0 = none
  bool ok = false;
  unsigned vao = 0, vbo = 0, ebo = 0;
  int nIdx = 0, nJ = 0;
  std::vector<int> parent;
  std::vector<Mat4> ibm, pre;
  std::vector<std::string> jointName;
  std::vector<Clip> clips;
  float height = 1;  // tallest point of the first clip's first pose (model units)
  int clip(const std::string& n) const;
  int joint(const std::string& n) const;
};

// pose: blend of two clip samples (b = 0 -> all A); writes skin matrices (model space) and joint world matrices
struct Pose { Mat4 skin[MAX_BONES]; Mat4 world[MAX_BONES]; };
void evaluate(const Model& m, int clipA, float tA, int clipB, float tB, float b, Pose& out);

const Model* load(const std::string& assetPath);  // cached; nullptr if missing

// draw one posed model; `xf` places it in the world (translation * yaw * scale)
void draw(const Model& m, const Pose& p, const Mat4& xf, bool shadow);
void beginDraw(bool shadow);
}
