#pragma once
#include "../gfx/gfx.h"
#include "../assets/assets.h"

struct FrameUBO {
  float view[16];
  float proj[16];
  float viewProj[16];
  float shadowMat[16];
  float cam[4];
  float sunDir[4];
  float sunCol[4];
  float hemiSky[4];
  float hemiGround[4];
  float fogCol[4];
  float fogParam[4];
  float season[4];
  float season2[4];
  float wind[4];
  float misc[4];
};

struct Camera {
  Vec3 pos;
  Mat4 view, proj, viewProj;
  float fov = 75, aspect = 1.6f, nearZ = 0.2f, farZ = 2600;
  Vec4 planes[6];
  Vec3 forward() const { return Vec3(-view.m[2], -view.m[6], -view.m[10]); }
  void update();
  bool sphere(const Vec3& c, float r) const;
  bool box(const Vec3& mn, const Vec3& mx) const;
};

enum InstMode { IM_NONE = 0, IM_LITE = 1, IM_FULL = 2 };
namespace render { extern int dbgMode; }
enum ObjFlag { OF_CUTOUT = 1, OF_DOUBLE = 2, OF_WIND = 4, OF_TINTWHITE = 8, OF_SOFT = 16, OF_CAPTURE = 32, OF_DEPTH = 64 };

inline InstFull toInst(const Mat4& m, uint32_t tint = 0xffffffffu) {
  InstFull f;
  for (int c = 0; c < 4; c++) { f.r0[c] = m.m[c * 4 + 0]; f.r1[c] = m.m[c * 4 + 1]; f.r2[c] = m.m[c * 4 + 2]; }
  f.tint = tint;
  return f;
}

struct DynBatch {
  InstBatch batch;
  std::vector<InstFull> items;
  const Mesh* mesh = nullptr;
  void init(const Mesh* m) { mesh = m; batch.create(m, InstKind::Full); }
  void clear() { items.clear(); }
  void add(const Mat4& m, uint32_t tint = 0xffffffffu) { items.push_back(toInst(m, tint)); }
  void upload() { batch.upload(items.data(), items.size()); }
  void draw() const { if (batch.count > 0) batch.draw(); }
  void destroy() { batch.destroy(); }
};

struct CulledBatch {
  std::vector<InstFull> all;
  std::vector<float> rad;
  DynBatch nearB, farB, shadowB;
  uint64_t sig = 0;
  unsigned gen = 1;
  float mid = 1e9f, band = 8, outer = 1e9f, shadowR = 46;
  void init(const Mesh* m, const Mesh* lod = nullptr) { nearB.init(m); farB.init(lod ? lod : m); shadowB.init(lod ? lod : m); if (!lod) mid = 1e9f; }
  void clear() { all.clear(); rad.clear(); gen++; }
  void add(const Mat4& m, float r, uint32_t tint = 0xffffffffu) { all.push_back(toInst(m, tint)); rad.push_back(r); gen++; }
  void cull();
  void draw(Shader& s);
  void drawShadow() { shadowB.draw(); }
  void destroy() { nearB.destroy(); farB.destroy(); shadowB.destroy(); }
};

namespace render {
extern Camera cam;
extern FrameUBO frame;
extern bool shadowPass;
extern int frameNo;

void init();
void shutdown();
void uploadFrame();
Shader& object(VFmt fmt, InstMode inst, SeasonKind kind, int flags);
Shader& objectFor(const AssetMaterial& m, InstMode inst, int extraFlags = 0);
void bindMaterial(Shader& s, const AssetMaterial& m);
void setModel(Shader& s, const Mat4& m);
void setTint(Shader& s, const Color& c, float a = 1);
void setEmissive(Shader& s, const Color& c, float glow = 0);
Shader& program(const std::string& name, const char* vs, const char* fs, const std::string& defines = "");
GLuint shadowTexture();
void beginShadowPass(const Mat4& lightVP, int size, int target = 0);
void endShadowPass();
void bindShadow();
void drawFullscreen();
std::string commonHeader();
GLuint quadVao();
GLuint quadVaoBuffer();
}
