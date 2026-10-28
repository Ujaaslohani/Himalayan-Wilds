#include "../assets/assets.h"
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
GLuint g_shadowFbo = 0, g_shadowTex = 0, g_staticFbo = 0, g_staticTex = 0, g_prevFbo = 0, g_prevTex = 0;
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

bool sphereVisible(const Vec3& p, float r) {
  Vec4 c = cam.viewProj.transform4(p, 1.0f);
  if (c.w < -r) return false;
  float m = r * 2.0f + 0.5f;
  return std::fabs(c.x) <= c.w + m && std::fabs(c.y) <= c.w + m;
}

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

int dbgMode = std::getenv("HW_DBG") ? std::atoi(std::getenv("HW_DBG")) : 0;
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
    if (flags & OF_FUR) def += "#define FUR\n";
    if (sp) def += "#define SHADOW_PASS\n";
    auto sh = std::make_unique<Shader>();
    std::string head = commonHeader();
    if (!compileShader(*sh, head + glsl::OBJECT_VS, head + glsl::SEASON_FRAG + glsl::OBJECT_FS, def)) std::fprintf(stderr, "object shader failed: %s\n", def.c_str());
    sh->use();
    sh->set1i("u_shadowMap", 7);
    sh->set1i("u_noiseTex", 6);
    sh->set1i("u_tex", 0);
    sh->set1i("u_ptexA", 8);
    sh->set1i("u_ptexN", 9);
    sh->set1i("u_ptexW", 10);
    sh->set1i("u_shadowPrev", 11);
  sh->set1i("u_shadowPrev", 11);
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
  s->set1i("u_dbg", dbgMode);
  s->set1i("u_tier", 0);
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
  sh->set1i("u_noiseTex", 6);
  sh->set1i("u_ptexA", 8);
  sh->set1i("u_ptexN", 9);
  sh->set1i("u_ptexW", 10);
  sh->set1i("u_shadowPrev", 11);
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
    makeDepth(g_prevTex, g_prevFbo, size, true);
    g_shadowSize = size;
  }
  (void)lightVP;
  glGetIntegerv(GL_VIEWPORT, g_vp);
  if (target == 1 && g_prevFbo) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_shadowFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_prevFbo);
    glBlitFramebuffer(0, 0, size, size, 0, 0, size, size, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
  }
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

static GLuint g_ptexA = 0, g_ptexN = 0, g_ptexW = 0;
static const char* PT_VS = "out vec2 v_uv;\nvoid main(){ vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); v_uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }\n";
// Expensive, tileable rock generator: fractured plates (periodic Voronoi), domain-warped fbm, ridged erosion, strata, lichen.
static const char* PT_GEN = R"(in vec2 v_uv;
out vec4 o;
float hp(vec2 i, float P){ i = mod(i, P); return fract(sin(dot(i, vec2(127.1, 311.7))) * 43758.5453); }
vec2 hp2(vec2 i, float P){ i = mod(i, P); return fract(sin(vec2(dot(i, vec2(127.1, 311.7)), dot(i, vec2(269.5, 183.3)))) * 43758.5453); }
float vn(vec2 p, float P){ vec2 i = floor(p), f = fract(p); vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  return mix(mix(hp(i, P), hp(i + vec2(1, 0), P), u.x), mix(hp(i + vec2(0, 1), P), hp(i + vec2(1, 1), P), u.x), u.y); }
float fbm(vec2 p, float P, int oct){ float s = 0.0, a = 0.5; for (int i = 0; i < 8; i++) { if (i >= oct) break; s += a * vn(p, P); p *= 2.0; P *= 2.0; a *= 0.5; } return s; }
float ridged(vec2 p, float P, int oct){ float s = 0.0, a = 0.5; for (int i = 0; i < 8; i++) { if (i >= oct) break; s += a * (1.0 - abs(vn(p, P) * 2.0 - 1.0)); p *= 2.0; P *= 2.0; a *= 0.5; } return s; }
vec3 vor(vec2 p, float P){ vec2 i = floor(p), f = fract(p); float d1 = 9.0, d2 = 9.0; float id = 0.0;
  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) { vec2 g = vec2(x, y); vec2 c = g + hp2(i + g, P) * 0.85 + 0.075; float d = length(c - f);
    if (d < d1) { d2 = d1; d1 = d; id = hp(i + g + 17.0, P); } else if (d < d2) d2 = d; }
  return vec3(d1, d2, id); }
void main(){
  vec2 p = v_uv;
  vec2 warp = vec2(fbm(p * 4.0, 4.0, 5), fbm(p * 4.0 + 5.2, 4.0, 5)) - 0.5;
  vec3 v1 = vor(p * 6.0 + warp * 1.4, 6.0);
  vec3 v2 = vor(p * 17.0 + warp * 2.5, 17.0);
  float crack1 = smoothstep(0.0, 0.07, v1.y - v1.x);
  float crack2 = smoothstep(0.0, 0.045, v2.y - v2.x);
  float plates = v1.z;
  float grain = fbm(p * 32.0, 32.0, 6);
  float er = ridged(p * 8.0 + warp, 8.0, 5);
  float strata = 0.5 + 0.5 * sin((p.y + warp.y * 0.25 + fbm(p * 2.0, 2.0, 3) * 0.3) * 6.2831853 * 9.0);
  float h = plates * 0.32 + er * 0.28 + grain * 0.22 + strata * 0.06;
  h *= mix(0.25, 1.0, crack1) * mix(0.6, 1.0, crack2);
  float hue = fbm(p * 3.0 + 9.0, 3.0, 4);
  vec3 a = mix(vec3(0.44, 0.42, 0.40), vec3(0.58, 0.52, 0.45), hue);
  a = mix(a, vec3(0.62, 0.6, 0.57), smoothstep(0.55, 0.85, plates) * 0.5);
  a *= mix(0.78, 1.18, grain) * mix(0.9, 1.06, strata);
  a *= mix(0.45, 1.0, crack1) * mix(0.72, 1.0, crack2);
  a *= mix(0.85, 1.12, smoothstep(0.3, 0.8, h));
  float lich = smoothstep(0.7, 0.76, fbm(p * 24.0 + 3.0, 24.0, 4)) * smoothstep(0.4, 0.7, h);
  a = mix(a, vec3(0.55, 0.55, 0.32), lich * 0.6);
  float spk = step(0.985, hp(floor(p * 512.0), 512.0));
  a = mix(a, vec3(0.78), spk * 0.5);
  o = vec4(clamp(a, 0.0, 1.0), clamp(h, 0.0, 1.0));
}
)";
static const char* PT_NRM = R"(in vec2 v_uv;
uniform sampler2D u_src;
uniform float u_px;
out vec4 o;
float H(vec2 uv){ return texture(u_src, fract(uv)).a; }
void main(){
  float l = H(v_uv - vec2(u_px, 0)), r = H(v_uv + vec2(u_px, 0)), d = H(v_uv - vec2(0, u_px)), u = H(v_uv + vec2(0, u_px));
  float c = H(v_uv);
  vec2 g = vec2(l - r, d - u) * 6.0;
  float blur = 0.0;
  for (int k = 0; k < 8; k++) { float a = float(k) * 0.785398; blur += H(v_uv + vec2(cos(a), sin(a)) * u_px * 6.0); }
  blur /= 8.0;
  float cav = clamp(0.62 + (c - blur) * 4.0, 0.0, 1.0);
  o = vec4(clamp(g * 0.5 + 0.5, 0.0, 1.0), cav, 1.0);
}
)";
// Wood: stretched grain along u, growth rings, knots, plank seams, weathering. rgb = albedo mult*0.5, a = height.
static const char* PT_WOOD = R"(in vec2 v_uv;
out vec4 o;
float hp(vec2 i, float P){ i = mod(i, P); return fract(sin(dot(i, vec2(127.1, 311.7))) * 43758.5453); }
float vn(vec2 p, vec2 P){ vec2 i = floor(p), f = fract(p); vec2 u = f * f * (3.0 - 2.0 * f);
  vec2 a = mod(i, P), b = mod(i + vec2(1, 0), P), c = mod(i + vec2(0, 1), P), d = mod(i + 1.0, P);
  return mix(mix(hp(a, 1e4), hp(b, 1e4), u.x), mix(hp(c, 1e4), hp(d, 1e4), u.x), u.y); }
float fbm(vec2 p, vec2 P){ float s = 0.0, a = 0.5; for (int i = 0; i < 6; i++) { s += a * vn(p, P); p *= 2.0; P *= 2.0; a *= 0.5; } return s; }
void main(){
  vec2 p = v_uv;
  float plank = floor(p.y * 6.0);
  float py = fract(p.y * 6.0);
  float seam = smoothstep(0.0, 0.035, py) * smoothstep(1.0, 0.965, py);
  float off = hp(vec2(plank, 3.0), 64.0);
  vec2 q = vec2(p.x + off, p.y);
  float w = fbm(q * vec2(4.0, 24.0), vec2(4.0, 24.0));
  float rings = 0.5 + 0.5 * sin((py * 7.0 + w * 3.5 + off * 10.0) * 6.2831853);
  float fibre = fbm(q * vec2(6.0, 160.0), vec2(6.0, 160.0));
  vec2 kc = vec2(hp(vec2(plank, 7.0), 64.0), 0.5);
  float kd = length((vec2(fract(q.x * 2.0), py) - kc) * vec2(2.0, 1.0));
  float knot = smoothstep(0.12, 0.0, kd) * step(0.55, hp(vec2(plank, 11.0), 64.0));
  float tone = mix(0.82, 1.12, hp(vec2(plank, 5.0), 64.0));
  float m = tone * mix(0.8, 1.1, rings) * mix(0.85, 1.12, fibre) * (1.0 - 0.55 * knot) * mix(0.45, 1.0, seam);
  float weather = fbm(p * vec2(3.0, 3.0) + 9.0, vec2(3.0)) ;
  m *= mix(0.92, 1.08, weather);
  float h = clamp(fibre * 0.4 + rings * 0.2 + 0.4, 0.0, 1.0) * mix(0.2, 1.0, seam);
  o = vec4(m * 0.5, h, m * 0.5, 1.0);
}
)";
static void ptexEnsure() {
  if (g_ptexA) return;
  const int N = 1024;
  auto mk = [&](GLuint& t) {
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (gfx::maxAniso() > 1.0f) glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, std::min(4.0f, gfx::maxAniso()));
    gfx::addBufferBytes((long long)N * N * 4 * 4 / 3);
  };
  mk(g_ptexA);
  mk(g_ptexN);
  mk(g_ptexW);
  GLint vp[4], prev = 0;
  glGetIntegerv(GL_VIEWPORT, vp);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glViewport(0, 0, N, N);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_ptexA, 0);
  program("ptexgen", PT_VS, PT_GEN);
  drawFullscreen();
  glBindTexture(GL_TEXTURE_2D, g_ptexA);
  glGenerateMipmap(GL_TEXTURE_2D);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_ptexW, 0);
  program("ptexwood", PT_VS, PT_WOOD);
  drawFullscreen();
  glBindTexture(GL_TEXTURE_2D, g_ptexW);
  glGenerateMipmap(GL_TEXTURE_2D);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_ptexN, 0);
  Shader& n = program("ptexnrm", PT_VS, PT_NRM);
  n.set1i("u_src", 0);
  n.set1f("u_px", 1.0f / N);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g_ptexA);
  drawFullscreen();
  glBindTexture(GL_TEXTURE_2D, g_ptexN);
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindFramebuffer(GL_FRAMEBUFFER, prev);
  glDeleteFramebuffers(1, &fbo);
  // compress colour textures to DXT1 (0.5 byte/px); normal/cavity texture stays RGBA8 for clean bumps
  // photo rock (CC0, ambientCG Rock030) replaces the generated albedo and normals when present
  int rw = 0, rh = 0, nw = 0, nh = 0;
  std::vector<uint8_t> rockCol = assets::loadImageRGBA("textures/rock_a_col.jpg", rw, rh);
  std::vector<uint8_t> rockNrm = assets::loadImageRGBA("textures/rock_a_nrm.jpg", nw, nh);
  if (!rockNrm.empty() && nw == N && nh == N) {
    glBindTexture(GL_TEXTURE_2D, g_ptexN);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, N, N, GL_RGBA, GL_UNSIGNED_BYTE, rockNrm.data());
    glGenerateMipmap(GL_TEXTURE_2D);
  }
  auto squash = [&](GLuint& t) {
    if (!gfx::hasS3TC()) return;
    std::vector<uint8_t> px((size_t)N * N * 4);
    glBindTexture(GL_TEXTURE_2D, t);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    if (t == g_ptexA && !rockCol.empty() && rw == N && rh == N)
      for (size_t i = 0; i < px.size(); i += 4) for (int c = 0; c < 3; c++) px[i + c] = (uint8_t)(rockCol[i + c] / 2);
    for (size_t i = 3; i < px.size(); i += 4) px[i] = 255;
    TexLevels L = prepareTexture(px.data(), N, N, true, true, false);
    if (!L.bc) return;
    Texture nt = uploadPrepared(L, true);
    if (gfx::maxAniso() > 1.0f) glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, std::min(4.0f, gfx::maxAniso()));
    glDeleteTextures(1, &t);
    gfx::addBufferBytes(-(long long)N * N * 4 * 4 / 3);
    t = nt.id;
  };
  squash(g_ptexA);
  squash(g_ptexW);
  glViewport(vp[0], vp[1], vp[2], vp[3]);
  glEnable(GL_DEPTH_TEST);
}

void bindShadow() {
  ptexEnsure();
  glActiveTexture(GL_TEXTURE8);
  glBindTexture(GL_TEXTURE_2D, g_ptexA);
  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D, g_ptexN);
  glActiveTexture(GL_TEXTURE10);
  glBindTexture(GL_TEXTURE_2D, g_ptexW);
  glActiveTexture(GL_TEXTURE11);
  glBindTexture(GL_TEXTURE_2D, g_prevTex);
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
