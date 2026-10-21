#include <cstdint>
#include <vector>
#include <cmath>
#include "render.h"
#include "glsl.h"
#include "../platform/platform.h"
#include <unordered_map>
#include <memory>
#include <cstring>
#include <cstdio>

Camera render::cam;
FrameUBO render::frame;
bool render::shadowPass = false;
int render::frameNo = 0;

namespace {
GLuint g_ubo = 0;
GLuint g_shadowFbo = 0, g_shadowTex = 0, g_staticFbo = 0, g_staticTex = 0;
int g_shadowSize = 0;
GLuint g_emptyVao = 0, g_quadVao = 0, g_quadVbo = 0;
std::unordered_map<uint32_t, std::unique_ptr<Shader>> g_objects;
std::unordered_map<std::string, std::unique_ptr<Shader>> g_programs;
int g_vp[4];
}

void Camera::update() {
  viewProj = proj * view;
  const float* m = viewProj.m;
  auto row = [&](int r) { return Vec4(m[r], m[4 + r], m[8 + r], m[12 + r]); };
  Vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
  planes[0] = Vec4(r3.x + r0.x, r3.y + r0.y, r3.z + r0.z, r3.w + r0.w);
  planes[1] = Vec4(r3.x - r0.x, r3.y - r0.y, r3.z - r0.z, r3.w - r0.w);
  planes[2] = Vec4(r3.x + r1.x, r3.y + r1.y, r3.z + r1.z, r3.w + r1.w);
  planes[3] = Vec4(r3.x - r1.x, r3.y - r1.y, r3.z - r1.z, r3.w - r1.w);
  planes[4] = Vec4(r3.x + r2.x, r3.y + r2.y, r3.z + r2.z, r3.w + r2.w);
  planes[5] = Vec4(r3.x - r2.x, r3.y - r2.y, r3.z - r2.z, r3.w - r2.w);
  for (auto& p : planes) {
    float l = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    p.x /= l; p.y /= l; p.z /= l; p.w /= l;
  }
}

bool Camera::sphere(const Vec3& c, float r) const {
  for (int i = 0; i < 5; i++) {
    const Vec4& p = planes[i];
    if (p.x * c.x + p.y * c.y + p.z * c.z + p.w < -r) return false;
  }
  return true;
}

bool Camera::box(const Vec3& mn, const Vec3& mx) const {
  for (int i = 0; i < 5; i++) {
    const Vec4& p = planes[i];
    Vec3 v(p.x >= 0 ? mx.x : mn.x, p.y >= 0 ? mx.y : mn.y, p.z >= 0 ? mx.z : mn.z);
    if (p.x * v.x + p.y * v.y + p.z * v.z + p.w < 0) return false;
  }
  return true;
}

namespace render {

std::string commonHeader() { return std::string(glsl::FRAME) + glsl::NOISE + glsl::SHADE; }

GLuint g_noiseTex = 0;
void init() {
  {
    std::vector<uint8_t> n(256 * 256);
    for (int y = 0; y < 256; y++)
      for (int x = 0; x < 256; x++) {
        float px = std::fmod(x * 123.34f, 1.0f), py = std::fmod(y * 456.21f, 1.0f);
        float d = px * (px + 45.32f) + py * (py + 45.32f);
        px += d; py += d;
        float v = px * py;
        v -= std::floor(v);
        n[y * 256 + x] = (uint8_t)(v * 255.0f + 0.5f);
      }
    glGenTextures(1, &g_noiseTex);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, g_noiseTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 256, 256, 0, GL_RED, GL_UNSIGNED_BYTE, n.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glActiveTexture(GL_TEXTURE0);
  }
  glGenBuffers(1, &g_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, g_ubo);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(FrameUBO), nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_ubo);
  glGenVertexArrays(1, &g_emptyVao);
  float quad[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
  glGenVertexArrays(1, &g_quadVao);
  glBindVertexArray(g_quadVao);
  glGenBuffers(1, &g_quadVbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_quadVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
  glBindVertexArray(0);
  std::memset(&frame, 0, sizeof frame);
}

void shutdown() {
  for (auto& s : g_objects) glDeleteProgram(s.second->prog);
  for (auto& s : g_programs) glDeleteProgram(s.second->prog);
  g_objects.clear();
  g_programs.clear();
  if (g_shadowTex) glDeleteTextures(1, &g_shadowTex);
  if (g_shadowFbo) glDeleteFramebuffers(1, &g_shadowFbo);
  if (g_ubo) glDeleteBuffers(1, &g_ubo);
}

void uploadFrame() {
  glBindBuffer(GL_UNIFORM_BUFFER, g_ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FrameUBO), &frame);
}

GLuint quadVao() { return g_quadVao; }
GLuint quadVaoBuffer() { return g_quadVbo; }

void drawFullscreen() {
  glBindVertexArray(g_emptyVao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  gfx::drawCalls++;
}

static const char* kindDefine(SeasonKind k) {
  switch (k) {
    case SeasonKind::Leaf: return "#define K_LEAF\n";
    case SeasonKind::Pine: return "#define K_PINE\n";
    case SeasonKind::Plant: return "#define K_PLANT\n";
    case SeasonKind::Grass: return "#define K_GRASS\n";
    case SeasonKind::Flower: return "#define K_FLOWER\n";
    case SeasonKind::Bark: return "#define K_BARK\n";
    case SeasonKind::Rock: return "#define K_ROCK\n";
    case SeasonKind::Build: return "#define K_BUILD\n";
    case SeasonKind::Mush: return "#define K_MUSH\n";
    default: return "";
  }
}

Shader& object(VFmt fmt, InstMode inst, SeasonKind kind, int flags) {
  bool sp = shadowPass;
  uint32_t key = (uint32_t)fmt | ((uint32_t)inst << 4) | ((uint32_t)kind << 8) | ((uint32_t)flags << 16) | (sp ? 1u << 24 : 0);
  auto it = g_objects.find(key);
  Shader* s;
  if (it == g_objects.end()) {
    std::string def;
    def += fmt == VFmt::P3C4 ? "#define VF_FLAT\n" : fmt == VFmt::P3C4N4 ? "#define VF_SMOOTH\n" : "#define VF_GLB\n";
    if (inst == IM_LITE) def += "#define INST_LITE\n";
    if (inst == IM_FULL) def += "#define INST_FULL\n";
    def += kindDefine(kind);
    if (flags & OF_CUTOUT) def += "#define CUTOUT\n";
    if (flags & OF_DOUBLE) def += "#define DOUBLE_SIDED\n";
    if (flags & OF_WIND) def += "#define WIND\n";
    if (flags & OF_TINTWHITE) def += "#define TINT_WHITE\n";
    if (flags & OF_SOFT) def += "#define SOFT\n";
    if (flags & OF_CAPTURE) def += "#define CAPTURE\n";
    if (flags & OF_DEPTH) def += "#define DEPTH_ONLY\n";
    if (sp) def += "#define SHADOW_PASS\n";
    auto sh = std::make_unique<Shader>();
    std::string head = commonHeader();
    if (!compileShader(*sh, head + glsl::OBJECT_VS, head + glsl::SEASON_FRAG + glsl::OBJECT_FS, def)) std::fprintf(stderr, "object shader failed: %s\n", def.c_str());
    sh->use();
    sh->set1i("u_shadowMap", 7);
    sh->set1i("u_noiseTex", 6);
    sh->set1i("u_tex", 0);
    s = sh.get();
    g_objects[key] = std::move(sh);
  } else s = it->second.get();
  s->use();
  static const Mat4 I;
  s->setMat4("u_model", I);
  s->set4f("u_tint", 1, 1, 1, 1);
  s->set4f("u_emissive", 0, 0, 0, 0);
  s->set1f("u_alpha", 1);
  s->set1f("u_recv", 1);
  s->set2f("u_fade", 0, 0);
  s->set2f("u_fadeIn", 0, 0);
  s->set1f("u_packed", 0);
  return *s;
}

Shader& objectFor(const AssetMaterial& m, InstMode inst, int extraFlags) {
  int flags = OF_WIND | OF_DOUBLE | extraFlags;
  if (m.cutout) flags |= OF_CUTOUT;
  Shader& s = object(VFmt::GLB, inst, m.kind, flags);
  bindMaterial(s, m);
  return s;
}

void bindMaterial(Shader& s, const AssetMaterial& m) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m.tex ? m.tex->id : 0);
  s.set1i("u_hasTex", m.tex ? 1 : 0);
  s.set4f("u_tint", m.base.r, m.base.g, m.base.b, 1);
}

void setModel(Shader& s, const Mat4& m) { s.setMat4("u_model", m); }
void setTint(Shader& s, const Color& c, float a) { s.set4f("u_tint", c.r, c.g, c.b, a); }
void setEmissive(Shader& s, const Color& c, float glow) { s.set4f("u_emissive", c.r, c.g, c.b, glow); }

Shader& program(const std::string& name, const char* vs, const char* fs, const std::string& defines) {
  auto it = g_programs.find(name);
  if (it != g_programs.end()) {
    it->second->use();
    return *it->second;
  }
  auto sh = std::make_unique<Shader>();
  std::string head = commonHeader();
  if (!compileShader(*sh, head + vs, head + fs, defines)) std::fprintf(stderr, "program %s failed\n", name.c_str());
  sh->use();
  sh->set1i("u_shadowMap", 7);
  Shader& r = *sh;
  g_programs[name] = std::move(sh);
  return r;
}

GLuint shadowTexture() { return g_shadowTex; }

static void makeDepth(GLuint& tex, GLuint& fbo, int size, bool compare) {
  if (tex) glDeleteTextures(1, &tex);
  if (!fbo) glGenFramebuffers(1, &fbo);
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, compare ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, compare ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (compare) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void beginShadowPass(const Mat4& lightVP, int size, int target) {
  if (size != g_shadowSize) {
    makeDepth(g_shadowTex, g_shadowFbo, size, true);
    makeDepth(g_staticTex, g_staticFbo, size, false);
    g_shadowSize = size;
  }
  (void)lightVP;
  glGetIntegerv(GL_VIEWPORT, g_vp);
  if (target == 2) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_staticFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_shadowFbo);
    glBlitFramebuffer(0, 0, size, size, 0, 0, size, size, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, target == 1 ? g_staticFbo : g_shadowFbo);
  glViewport(0, 0, size, size);
  if (target != 2) glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.5f, 2.0f);
  shadowPass = true;
}

void endShadowPass() {
  glDisable(GL_POLYGON_OFFSET_FILL);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(g_vp[0], g_vp[1], g_vp[2], g_vp[3]);
  shadowPass = false;
}

void bindShadow() {
  glActiveTexture(GL_TEXTURE7);
  glBindTexture(GL_TEXTURE_2D, g_shadowTex);
  glActiveTexture(GL_TEXTURE0);
}

}  // namespace render

void CulledBatch::cull() {
  const Camera& c = render::cam;
  nearB.items.clear(); farB.items.clear(); shadowB.items.clear();
  uint64_t h = 1469598103934665603ull ^ gen;
  bool lod = mid < 1e8f;
  for (size_t i = 0; i < all.size(); i++) {
    const InstFull& it = all[i];
    float x = it.r0[3], y = it.r1[3], z = it.r2[3], r = rad[i];
    float d = std::sqrt((x - c.pos.x) * (x - c.pos.x) + (z - c.pos.z) * (z - c.pos.z));
    if (d - r > outer) continue;
    bool sh = d - r < shadowR;
    if (sh) shadowB.items.push_back(it);
    bool vis = c.sphere({x, y + r * 0.5f, z}, r * 1.2f);
    bool nr = vis && (!lod || d < mid + band + 2), fr = vis && lod && d > mid - 2;
    if (nr) nearB.items.push_back(it);
    if (fr) farB.items.push_back(it);
    h = (h ^ (i * 8 + nr * 4 + fr * 2 + sh + 1)) * 1099511628211ull;
  }
  if (h == sig) return;
  sig = h;
  nearB.upload();
  farB.upload();
  shadowB.upload();
}

void CulledBatch::draw(Shader& s) {
  bool lod = mid < 1e8f;
  if (lod) s.set2f("u_fade", mid, mid + band);
  nearB.draw();
  if (lod) {
    s.set2f("u_fade", 0, 0);
    s.set2f("u_fadeIn", mid, mid + band);
    farB.draw();
    s.set2f("u_fadeIn", 0, 0);
  }
  s.set2f("u_fade", 0, 0);
}
