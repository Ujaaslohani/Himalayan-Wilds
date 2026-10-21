#include "../render/terrain_render.h"
#include "../world/terrain.h"
#include "fx.h"
#include "engine.h"
#include "../render/render.h"
#include "../models/models.h"
#include "../world/terrain.h"

namespace {

const char* BLOB_VS = R"(
in vec2 a_pos;
in vec4 a_i0;
in vec4 a_i1;
out vec2 v_q;
void main(){
  float c = cos(a_i1.x), s = sin(a_i1.x);
  vec2 q = a_pos * a_i1.yz;
  vec3 w = a_i0.xyz + vec3(c * q.x + s * q.y, 0.0, -s * q.x + c * q.y);
  v_q = a_pos;
  gl_Position = u_viewProj * vec4(w, 1.0);
}
)";
const char* BLOB_FS = R"(
in vec2 v_q;
out vec4 fragColor;
void main(){
  float d = length(v_q);
  float a = d < 0.6 ? mix(0.55, 0.3, d / 0.6) : mix(0.3, 0.0, (d - 0.6) / 0.4);
  if (d >= 1.0) discard;
  fragColor = vec4(0.0, 0.0, 0.0, a);
}
)";

const char* BASIC_VS = R"(
in vec3 a_pos;
in vec4 a_col;
uniform mat4 u_model;
out vec3 v_wpos;
out vec3 v_col;
out float v_depth;
void main(){
  vec4 wp = u_model * vec4(a_pos, 1.0);
  v_wpos = wp.xyz;
  v_col = srgbToLinear(a_col.rgb);
  vec4 vp = u_view * wp;
  v_depth = -vp.z;
  gl_Position = u_proj * vp;
}
)";
const char* BASIC_FS = R"(
in vec3 v_wpos;
in vec3 v_col;
in float v_depth;
uniform vec4 u_color;
uniform float u_fog;
uniform float u_useVC;
out vec4 fragColor;
void main(){
  vec3 c = outColor(mix(vec3(1.0), v_col, u_useVC) * u_color.rgb);
  c = mix(c, u_fogCol.rgb, fogObject(v_wpos, v_depth) * u_fog);
  fragColor = vec4(c, u_color.a);
}
)";

const char* GLOW_VS = R"(
in vec4 a_pos;
out float v_kind;
void main(){
  vec4 mv = u_view * vec4(a_pos.xyz, 1.0);
  float size = abs(a_pos.w);
  v_kind = a_pos.w < 0.0 ? 1.0 : 0.0;
  float ps = v_kind > 0.5 ? clamp(size * u_viewH * 0.55 / -mv.z, 1.0, 400.0) : clamp(size * u_viewH * 0.55 / -mv.z, u_viewH * 0.009, 220.0);
  if (v_kind < 0.5) mv.xyz *= max(0.0, 1.0 - 0.45 / length(mv.xyz));
  gl_Position = u_proj * mv;
  gl_PointSize = ps;
}
)";
const char* GLOW_FS = R"(
in float v_kind;
uniform float u_nightAmt;
out vec4 fragColor;
void main(){
  float d = length(gl_PointCoord - 0.5) * 2.0;
  if (d > 1.0) discard;
  if (v_kind > 0.5) {
    vec3 c0 = vec3(1.0, 0.86, 0.55), c1 = vec3(1.0, 0.59, 0.2), c2 = vec3(1.0, 0.43, 0.08);
    vec3 c = d < 0.35 ? mix(c0, c1, d / 0.35) : mix(c1, c2, (d - 0.35) / 0.65);
    float a = d < 0.35 ? mix(1.0, 0.5, d / 0.35) : mix(0.5, 0.0, (d - 0.35) / 0.65);
    fragColor = vec4(c * a, 1.0);
    return;
  }
  float a = pow(1.0 - d, 2.2);
  fragColor = vec4(vec3(1.0, 0.78, 0.42) * a * u_nightAmt, 1.0);
}
)";

const char* POOL_VS = R"(
in vec2 a_pos;
in vec4 a_i0;
uniform sampler2D u_height;
uniform vec4 u_grid;
out vec2 v_q;
float hAt(ivec2 c){ c = clamp(c, ivec2(0), ivec2(int(u_grid.z) - 1)); return texelFetch(u_height, c, 0).r; }
float heightAt(vec2 g){
  vec2 i = floor(g); vec2 f = g - i; ivec2 c = ivec2(i);
  float a = hAt(c), b = hAt(c + ivec2(1, 0)), cc = hAt(c + ivec2(0, 1)), d = hAt(c + ivec2(1, 1));
  if (f.x + f.y <= 1.0) return a + (b - a) * f.x + (cc - a) * f.y;
  return d + (cc - d) * (1.0 - f.x) + (b - d) * (1.0 - f.y);
}
void main(){
  const int N = 10;
  int tri = gl_VertexID / 6, k = gl_VertexID % 6;
  int cx = tri % N, cz = tri / N;
  ivec2 o[6] = ivec2[6](ivec2(0,0), ivec2(1,0), ivec2(1,1), ivec2(0,0), ivec2(1,1), ivec2(0,1));
  vec2 q = (vec2(cx, cz) + vec2(o[k])) / float(N) * 2.0 - 1.0;
  v_q = q;
  vec2 xz = a_i0.xz + q * a_i0.w;
  float h = heightAt((xz - u_grid.xx) / u_grid.y) + 0.05;
  gl_Position = u_viewProj * vec4(xz.x, h, xz.y, 1.0);
}
)";
const char* POOL_FS = R"(
in vec2 v_q;
uniform float u_nightAmt;
out vec4 fragColor;
void main(){
  float d = length(v_q);
  if (d > 1.0) discard;
  float a = pow(1.0 - d, 1.8) * 0.55;
  fragColor = vec4(vec3(1.0, 0.72, 0.38) * a * u_nightAmt, 1.0);
}
)";

const char* EMBER_VS = R"(
in vec3 a_pos;
uniform vec4 u_place;
out float v_a;
void main(){
  vec3 sd = a_pos;
  float t = fract(sd.y + u_clock * (0.25 + sd.z * 0.3));
  vec3 p = vec3((sd.x - 0.5) * 0.6 + sin(u_clock * 2.0 + sd.x * 9.0) * 0.15 * t, t * 2.6 + 0.3, (sd.z - 0.5) * 0.6);
  v_a = 1.0 - t;
  vec4 mv = u_view * vec4(u_place.xyz + p * u_place.w, 1.0);
  gl_Position = u_proj * mv;
  gl_PointSize = 90.0 * u_place.w / max(-mv.z, 0.2) * (u_viewH / 900.0);
}
)";
const char* EMBER_FS = R"(
in float v_a;
out vec4 fragColor;
void main(){
  float d = length(gl_PointCoord - 0.5);
  if (d > 0.5) discard;
  fragColor = vec4(vec3(1.0, 0.65, 0.2) * v_a * (1.0 - d * 2.0), 1.0);
}
)";

struct BlobI { float x, y, z, pad, yaw, sx, sz, pad2; };
struct Fire { float x, y, z, s; };
struct Ghost { const Mesh* mesh; Mat4 m; bool ok, custom; };

std::vector<BlobI> g_blobs;
std::vector<Fire> g_fires;
std::vector<float> g_glows;
std::vector<float> g_pools;
std::vector<Ghost> g_ghosts;
GLuint g_blobVao = 0, g_blobVbo = 0, g_glowVao = 0, g_glowVbo = 0, g_poolVao = 0, g_poolVbo = 0, g_emberVao = 0, g_emberVbo = 0;
size_t g_blobCap = 0, g_glowCap = 0, g_poolCap = 0;

void instVao(GLuint& vao, GLuint& vbo, int attribs) {
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, render::quadVaoBuffer());
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  for (int a = 0; a < attribs; a++) {
    glEnableVertexAttribArray(6 + a);
    glVertexAttribPointer(6 + a, 4, GL_FLOAT, GL_FALSE, attribs * 16, (void*)(intptr_t)(a * 16));
    glVertexAttribDivisor(6 + a, 1);
  }
  glBindVertexArray(0);
}

void stream(GLuint vbo, size_t& cap, const void* data, size_t bytes) {
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  if (bytes > cap) {
    cap = bytes * 2;
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cap, nullptr, GL_DYNAMIC_DRAW);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, data);
}

}  // namespace

namespace fx {

void init() {
  instVao(g_blobVao, g_blobVbo, 2);
  instVao(g_poolVao, g_poolVbo, 1);
  glBindVertexArray(g_poolVao);
  glDisableVertexAttribArray(0);
  glBindVertexArray(0);
  glGenVertexArrays(1, &g_glowVao);
  glBindVertexArray(g_glowVao);
  glGenBuffers(1, &g_glowVbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_glowVbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 16, nullptr);
  std::vector<float> seeds(24 * 3);
  Rng r(31337);
  for (auto& s : seeds) s = r.next();
  glGenVertexArrays(1, &g_emberVao);
  glBindVertexArray(g_emberVao);
  glGenBuffers(1, &g_emberVbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_emberVbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(seeds.size() * 4), seeds.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
  glBindVertexArray(0);
}

void beginFrame() {
  g_blobs.clear();
  g_fires.clear();
  g_glows.clear();
  g_pools.clear();
  g_ghosts.clear();
}

void blob(float x, float y, float z, float yaw, float sx, float sz) { g_blobs.push_back({x, y, z, 0, yaw, sx, sz, 0}); }
void campfire(float x, float y, float z, float s) { g_fires.push_back({x, y, z, s}); }
void glow(float x, float y, float z, float size) { g_glows.insert(g_glows.end(), {x, y, z, size}); }
void pool(float x, float y, float z, float r) { g_pools.insert(g_pools.end(), {x, y, z, r}); }
void ghost(const Mesh* mesh, const Mat4& m, bool ok, bool custom) { g_ghosts.push_back({mesh, m, ok, custom}); }

void drawBlobs() {
  if (g_blobs.empty()) return;
  stream(g_blobVbo, g_blobCap, g_blobs.data(), g_blobs.size() * sizeof(BlobI));
  render::program("blob", BLOB_VS, BLOB_FS);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-2, -2);
  glBindVertexArray(g_blobVao);
  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)g_blobs.size());
  gfx::drawCalls++;
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void drawTransparent() {
  glEnable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  if (!g_fires.empty()) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    Shader& s = render::program("basic", BASIC_VS, BASIC_FS);
    s.set1f("u_fog", 1);
    s.set1f("u_useVC", 1);
    float t = E.clock;
    Color outer = Color::hex("f97316"), inner = Color::hex("fde047");
    for (auto& f : g_fires) {
      if (render::cam.pos.distanceTo({f.x, f.y, f.z}) > 260) continue;
      Mat4 root = Mat4::compose({f.x, f.y, f.z}, Quat(), {f.s, f.s, f.s});
      Mat4 mo = root * Mat4::compose({0, 0, 0}, Quat::axisAngle({0, 1, 0}, t * 0.8f), {1 + std::sin(t * 9) * 0.06f, 1 + std::sin(t * 7.3f) * 0.12f, 1 + std::cos(t * 8) * 0.06f});
      s.setMat4("u_model", mo);
      s.set4f("u_color", outer.r, outer.g, outer.b, 0.9f);
      models::cone6().draw();
      Mat4 mi = root * Mat4::scale({1, 1 + std::sin(t * 11 + 1) * 0.15f, 1});
      s.setMat4("u_model", mi);
      s.set4f("u_color", inner.r, inner.g, inner.b, 0.95f);
      models::cone5().draw();
    }
    glBlendFunc(GL_ONE, GL_ONE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    Shader& e = render::program("ember", EMBER_VS, EMBER_FS);
    glBindVertexArray(g_emberVao);
    for (auto& f : g_fires) {
      if (render::cam.pos.distanceTo({f.x, f.y, f.z}) > 160) continue;
      e.set4f("u_place", f.x, f.y, f.z, f.s);
      glDrawArrays(GL_POINTS, 0, 24);
      gfx::drawCalls++;
    }
    for (auto& f : g_fires) {
      float night = E.isNight ? 1.5f : 1;
      g_glows.insert(g_glows.end(), {f.x, f.y + 0.8f * f.s, f.z, -(3.0f + std::sin(t * 6) * 0.25f) * night * f.s});
    }
    glDepthMask(GL_TRUE);
  }
  if (!g_ghosts.empty()) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (auto& g : g_ghosts) {
      if (g.custom && g.ok) {
        Shader& s = render::object(VFmt::P3C4, IM_NONE, SeasonKind::None, 0);
        render::setModel(s, g.m);
        s.set1f("u_alpha", 0.82f);
        glDepthMask(GL_TRUE);
        g.mesh->draw();
      } else {
        glDepthMask(GL_FALSE);
        Shader& s = render::program("ghost", BASIC_VS, BASIC_FS);
        s.setMat4("u_model", g.m);
        Color c = Color::hex(g.ok ? "34d399" : "f87171");
        s.set1f("u_fog", 0);
        s.set1f("u_useVC", 0);
        s.set4f("u_color", c.r, c.g, c.b, g.custom ? 0.5f : 0.42f);
        g.mesh->draw();
        glDepthMask(GL_TRUE);
      }
    }
  }
  glDisable(GL_BLEND);
}

void drawGlows() {
  float night = clampf((0.45f - E.daylight) / 0.35f, 0, 1);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  if (!g_pools.empty() && night > 0.01f) {
    stream(g_poolVbo, g_poolCap, g_pools.data(), g_pools.size() * 4);
    Shader& s = render::program("lpool", POOL_VS, POOL_FS);
    s.set1f("u_nightAmt", night);
    s.set1i("u_height", 1);
    s.set4f("u_grid", GRID_MIN, GRID_STEP, (float)GRID_N, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, terrain_render::heightTexture());
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-3, -3);
    glBindVertexArray(g_poolVao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 600, (GLsizei)(g_pools.size() / 4));
    gfx::drawCalls++;
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
  if (!g_glows.empty()) {
    std::vector<float> list;
    list.reserve(g_glows.size());
    for (size_t i = 0; i < g_glows.size(); i += 4)
      if (g_glows[i + 3] < 0 || night > 0.01f) list.insert(list.end(), g_glows.begin() + i, g_glows.begin() + i + 4);
    if (!list.empty()) {
      stream(g_glowVbo, g_glowCap, list.data(), list.size() * 4);
      Shader& s = render::program("glow", GLOW_VS, GLOW_FS);
      s.set1f("u_nightAmt", night);
      glEnable(GL_PROGRAM_POINT_SIZE);
      glBindVertexArray(g_glowVao);
      glDrawArrays(GL_POINTS, 0, (GLsizei)(list.size() / 4));
      gfx::drawCalls++;
    }
  }
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void basicMesh(const Mesh& mesh, const Mat4& model, const Color& c, float alpha, bool useVC, bool fog) {
  Shader& s = render::program("basic", BASIC_VS, BASIC_FS);
  s.setMat4("u_model", model);
  s.set4f("u_color", c.r, c.g, c.b, alpha);
  s.set1f("u_useVC", useVC ? 1.0f : 0.0f);
  s.set1f("u_fog", fog ? 1.0f : 0.0f);
  mesh.draw();
}

void shutdown() {
  GLuint b[] = {g_blobVbo, g_glowVbo, g_poolVbo, g_emberVbo};
  glDeleteBuffers(4, b);
  GLuint v[] = {g_blobVao, g_glowVao, g_poolVao, g_emberVao};
  glDeleteVertexArrays(4, v);
}

}  // namespace fx
