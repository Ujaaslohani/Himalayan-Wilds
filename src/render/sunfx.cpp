#include "sunfx.h"
#include "render.h"
#include "sky.h"
#include "../game/engine.h"
#include "../game/season.h"
#include <cmath>
#include <algorithm>

namespace {

const char* FS_VS = "out vec2 v_uv;\nvoid main(){ vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); v_uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }\n";

const char* OCC_FS = R"(in vec2 v_uv;
uniform sampler2D u_depth;
uniform vec2 u_sun;
uniform vec2 u_rad;
out vec4 o;
void main(){
  float vis = 0.0;
  for (int i = 0; i < 48; i++) {
    float r = sqrt((float(i) + 0.5) / 48.0);
    float a = float(i) * 2.39996;
    vec2 uv = u_sun + vec2(cos(a), sin(a)) * r * u_rad;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) { vis += 0.5; continue; }
    vis += step(0.99999, texture(u_depth, uv).r);
  }
  o = vec4(vis / 48.0);
}
)";

const char* SHAFT_FS = R"(in vec2 v_uv;
uniform sampler2D u_depth;
uniform vec2 u_sun;
uniform float u_aspect;
out vec4 o;
void main(){
  vec2 d = (u_sun - v_uv) / 28.0;
  vec2 uv = v_uv;
  float acc = 0.0, w = 1.0;
  for (int i = 0; i < 28; i++) {
    acc += step(0.99999, texture(u_depth, uv).r) * w;
    w *= 0.955;
    uv += d;
  }
  vec2 dd = (v_uv - u_sun) * vec2(u_aspect, 1.0);
  float fall = exp(-length(dd) * 2.4);
  o = vec4(acc / 16.0 * fall);
}
)";

const char* COMP_FS = R"(in vec2 v_uv;
uniform sampler2D u_occ;
uniform sampler2D u_shaft;
uniform vec2 u_sun;
uniform float u_aspect;
uniform float u_k;
uniform float u_look;
uniform float u_time;
uniform vec3 u_col;
out vec4 o;
void main(){
  float vis = texture(u_occ, vec2(0.5)).r;
  vec2 dd = (v_uv - u_sun) * vec2(u_aspect, 1.0);
  float r = length(dd);
  float ang = atan(dd.y, dd.x);
  float core = exp(-r * 110.0) * 1.5 + exp(-r * 26.0) * 0.28 + exp(-r * 6.0) * 0.06;
  float rays = pow(abs(cos(ang * 7.0 + u_time * 0.05)), 60.0) + 0.6 * pow(abs(cos(ang * 11.0 - u_time * 0.03 + 1.3)), 90.0);
  rays *= exp(-r * 8.0) * 0.4;
  float glare = (core + rays) * vis;
  float wash = vis * u_look * (exp(-r * 4.5) * 0.24 + 0.03);
  float shaft = texture(u_shaft, v_uv).r * 0.22;
  vec3 c = u_col * (glare + wash + shaft) * u_k;
  o = vec4(c, 1.0);
}
)";

struct RT {
  GLuint fbo = 0, tex = 0;
  int w = 0, h = 0;
  void ensure(int W, int H) {
    if (W == w && H == h && fbo) return;
    release();
    w = W; h = H;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  void release() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (tex) glDeleteTextures(1, &tex);
    fbo = tex = 0;
    w = h = 0;
  }
};

RT g_occ, g_shaft;
float g_sx = 0, g_sy = 0, g_k = 0, g_look = 0, g_aspect = 1;
bool g_on = false;

float sstep(float x, float a, float b) {
  float t = std::min(1.0f, std::max(0.0f, (x - a) / (b - a)));
  return t * t * (3 - 2 * t);
}

void computeState() {
  g_on = false;
  Vec3 sun = sky::sunDir();
  float up = sstep(sun.y, -0.03f, 0.08f);
  float day = sstep(E.daylight, 0.12f, 0.5f);
  if (up <= 0 || day <= 0) return;
  const Camera& cam = render::cam;
  Vec3 p = cam.pos + sun * 1000.0f;
  Vec4 c = cam.viewProj.transform4(p, 1.0f);
  if (c.w <= 0.01f) return;
  float nx = c.x / c.w, ny = c.y / c.w;
  if (std::fabs(nx) > 1.6f || std::fabs(ny) > 1.6f) return;
  g_sx = nx * 0.5f + 0.5f;
  g_sy = ny * 0.5f + 0.5f;
  float cover = 1 - 0.55f * std::min(1.0f, season.fall + 0.3f * season.autumn);
  g_k = up * day * cover;
  float align = std::max(0.0f, cam.forward().dot(sun));
  g_look = std::min(1.0f, std::pow(align, 6.0f) * 1.15f);
  g_on = g_k > 0.01f;
}

}  // namespace

namespace sunfx {

bool wanted() {
  computeState();
  return g_on;
}

void occlusion(GLuint depthTex, int w, int h) {
  if (!g_on || !depthTex) return;
  g_aspect = (float)w / std::max(1, h);
  GLint prevFbo = 0, vp[4];
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
  glGetIntegerv(GL_VIEWPORT, vp);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, depthTex);

  g_occ.ensure(1, 1);
  glBindFramebuffer(GL_FRAMEBUFFER, g_occ.fbo);
  glViewport(0, 0, 1, 1);
  Shader& oc = render::program("sunocc", FS_VS, OCC_FS);
  oc.set1i("u_depth", 0);
  oc.set2f("u_sun", g_sx, g_sy);
  float rad = 0.006f;
  oc.set2f("u_rad", rad, rad * g_aspect);
  glEnable(GL_BLEND);
  glBlendColor(0, 0, 0, 0.35f);
  glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
  render::drawFullscreen();
  glDisable(GL_BLEND);

  g_shaft.ensure(std::max(1, w / 4), std::max(1, h / 4));
  glBindFramebuffer(GL_FRAMEBUFFER, g_shaft.fbo);
  glViewport(0, 0, g_shaft.w, g_shaft.h);
  Shader& sh = render::program("sunshaft", FS_VS, SHAFT_FS);
  sh.set1i("u_depth", 0);
  sh.set2f("u_sun", g_sx, g_sy);
  sh.set1f("u_aspect", g_aspect);
  render::drawFullscreen();

  glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
  glViewport(vp[0], vp[1], vp[2], vp[3]);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
}

void composite(int W, int H) {
  if (!g_on || !g_occ.fbo) return;
  Shader& s = render::program("suncomp", FS_VS, COMP_FS);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, g_shaft.tex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g_occ.tex);
  s.set1i("u_occ", 0);
  s.set1i("u_shaft", 1);
  s.set2f("u_sun", g_sx, g_sy);
  s.set1f("u_aspect", (float)W / std::max(1, H));
  s.set1f("u_k", g_k);
  s.set1f("u_look", g_look);
  s.set1f("u_time", E.clock);
  Color col = E.daylight > 0.55f ? Color::hex("fff3dc") : Color::hex("ffc890");
  s.set3f("u_col", col.r, col.g, col.b);
  render::drawFullscreen();
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}

void shutdown() {
  g_occ.release();
  g_shaft.release();
}

}  // namespace sunfx
