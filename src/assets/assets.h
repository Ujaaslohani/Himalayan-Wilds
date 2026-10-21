#pragma once
#include "../gfx/gfx.h"
#include <string>
#include <vector>

enum class SeasonKind { None, Leaf, Pine, Plant, Grass, Flower, Bark, Rock, Ground, Build, Mush };

struct AssetMaterial {
  std::string name;
  const Texture* tex = nullptr;
  Color base{1, 1, 1};
  bool cutout = false;
  SeasonKind kind = SeasonKind::Plant;
};

struct AssetPart {
  Mesh mesh;
  Mesh lod;
  AssetMaterial mat;
  Mat4 local;
  bool identity = true;
};

struct Asset {
  std::string file;
  std::vector<AssetPart> parts;
  int tris = 0;
  Vec3 bmin{1e9f, 1e9f, 1e9f}, bmax{-1e9f, -1e9f, -1e9f};
  bool ok() const { return !parts.empty(); }
};

struct AssetRequest {
  std::string file;
  std::string layer;
  float colorFloor = -1;
};

namespace assets {
void loadAll(const std::vector<AssetRequest>& reqs);
const Asset* get(const std::string& file);
std::vector<std::string> missing();
SeasonKind kindFor(const std::string& matName, const std::string& layer);
void shutdown();
}
