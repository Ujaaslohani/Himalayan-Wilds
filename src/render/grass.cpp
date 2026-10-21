#include "grass.h"
#include "render.h"
#include "terrain_render.h"
#include "../world/terrain.h"
#include "../world/forest.h"
#include "../platform/platform.h"
#include "../game/season.h"
#include <vector>
#include <cstdint>
#include <algorithm>
#include <unordered_map>
#include <cmath>
#include <string>

namespace {

const char* GRASS_VS = R"(
layout(location=0) in vec2 a_pos;
layout(location=6) in vec4 a_b0;
layout(location=7) in vec4 a_b1;
uniform sampler2D u_bake;
uniform vec4 u_grid;
uniform vec4 u_ring;
uniform vec4 u_blade;
out vec3 v_col;
void kill(){ gl_Position = vec4(2.0, 2.0, 2.0, 1.0); v_col = vec3(0.0); }
void main(){
  vec4 h = a_b1;
  vec2 p = a_b0.xz;
  float d = distance(p, u_camPos.xz);
  float fade = u_ring.w;
  if (d > u_ring.z || d < u_ring.y - fade) { kill(); return; }
  if (h.z > a_b0.w * u_blade.z) { kill(); return; }
  float grow = (1.0 - smoothstep(u_ring.z - fade, u_ring.z, d)) * (u_ring.y > 0.0 ? smoothstep(u_ring.y - fade, u_ring.y, d) : 1.0);
  grow *= 1.0 - 0.65 * u_snow;
  if (grow < 0.02) { kill(); return; }
  float base = a_b0.y;
  vec4 cc = u_viewProj * vec4(p.x, base + 0.3, p.y, 1.0);
  float m = cc.w * 1.15 + 1.5;
  if (cc.w < -1.0 || abs(cc.x) > m || cc.y > m * 1.2 + 2.0 || cc.y < -m * 1.2 - 2.0) { kill(); return; }
  float ang = h.w * 6.2831853;
  vec2 side = vec2(cos(ang), sin(ang));
  vec2 face = vec2(-side.y, side.x);
  float H = mix(0.32, 0.72, fract(h.x * 7.13 + h.y * 3.7)) * u_blade.y * grow;
  float W = u_blade.x * mix(0.8, 1.25, h.z) * (0.4 + 0.6 * grow);
  float t = a_pos.y;
  float gust = u_windS * (0.6 + 0.4 * sin(u_windT * 1.1 + p.x * 0.05 + p.y * 0.04));
  float sway = (0.35 + 0.65 * sin(u_windT * 2.3 + p.x * 0.35 + p.y * 0.27 + h.w * 6.0)) * gust;
  vec2 lean = face * (0.15 + 0.25 * fract(h.y * 9.1)) + u_windDir * sway * 0.55;
  vec2 xz = p + side * a_pos.x * W * (1.0 - t * 0.85) + lean * H * t * t;
  float y = base + H * t * (1.0 - 0.18 * t * length(lean));
  vec3 wpos = vec3(xz.x, y, xz.y);
  vec4 vp = u_view * vec4(wpos, 1.0);
  vec2 g = (p - u_grid.xx) / u_grid.y;
  vec4 bk = textureLod(u_bake, g / (u_grid.z - 1.0), 0.0);
  vec3 gc = bk.rgb * bk.rgb;
  vec3 tip = gc * vec3(1.35, 1.3, 0.95) + vec3(0.02, 0.025, 0.0);
  tip = mix(tip, vec3(0.62, 0.5, 0.22), 0.35 * u_dry + 0.25 * u_autumn);
  tip = mix(tip, vec3(0.85, 0.88, 0.92), u_snow * 0.7);
  vec3 alb = mix(gc * 0.62, tip, smoothstep(0.0, 1.0, t)) * (0.85 + 0.3 * h.z);
  vec3 n = normalize(vec3(0.0, 1.0, 0.0) + (u_sunDir.xyz * vec3(1.0, 0.0, 1.0)) * 0.35);
  float sh = shadowAt(t > 0.5 ? wpos : vec3(p.x, base + 0.3, p.y), vec3(0.0, 1.0, 0.0));
  vec3 c = outColor(lambert(alb, n, sh) * (0.7 + 0.3 * t));
  v_col = mix(c, u_fogCol.rgb, fogGround(wpos, -vp.z));
  gl_Position = u_proj * vp;
}
)";

const char* GRASS_FS = R"(
in vec3 v_col;
out vec4 fragColor;
void main(){ fragColor = vec4(v_col, 1.0); }
)";

const char* LEAF_VS = R"(
layout(location=0) in vec2 a_pos;
uniform sampler2D u_height;
uniform sampler2D u_density;
uniform vec4 u_grid;
uniform vec4 u_leaf;
uniform int u_n;
out vec3 v_wpos;
out float v_depth;
out vec2 v_uv;
out vec4 v_h;
float hAt(ivec2 c){ c = clamp(c, ivec2(0), ivec2(int(u_grid.z) - 1)); return texelFetch(u_height, c, 0).r; }
float heightAt(vec2 g){
  vec2 i = floor(g); vec2 f = g - i; ivec2 c = ivec2(i);
  float a = hAt(c), b = hAt(c + ivec2(1, 0)), cc = hAt(c + ivec2(0, 1)), d = hAt(c + ivec2(1, 1));
  if (f.x + f.y <= 1.0) return a + (b - a) * f.x + (cc - a) * f.y;
  return d + (cc - d) * (1.0 - f.x) + (b - d) * (1.0 - f.y);
}
vec4 hash4(vec2 p){
  vec4 q = fract(vec4(p.xyxy) * vec4(0.1031, 0.1030, 0.0973, 0.1099));
  q += dot(q, q.wzxy + 33.33);
  return fract((q.xxyz + q.yzzw) * q.zywx);
}
void kill(){ gl_Position = vec4(2.0, 2.0, 2.0, 1.0); v_wpos = vec3(0.0); v_depth = 0.0; v_uv = vec2(0.0); v_h = vec4(0.0); }
void main(){
  float s = u_leaf.x;
  int ix = gl_InstanceID % u_n, iz = gl_InstanceID / u_n;
  vec2 origin = floor(u_camPos.xz / s) * s - float(u_n / 2) * s;
  vec2 cellId = floor(origin / s) + vec2(ix, iz);
  vec4 h = hash4(cellId + 91.7);
  vec2 p = (cellId + h.xy) * s;
  float d = distance(p, u_camPos.xz);
  if (d > u_leaf.y) { kill(); return; }
  vec2 g = (p - u_grid.xx) / u_grid.y;
  float dens = texture(u_density, (g + 0.5) / u_grid.z).r;
  if (h.z > dens * u_leaf.z + 0.15 * u_leaf.z) { kill(); return; }
  float fadeK = 1.0 - smoothstep(u_leaf.y * 0.7, u_leaf.y, d);
  if (fract(h.w * 13.7) > fadeK) { kill(); return; }
  float ang = h.w * 6.2831853;
  vec2 ax = vec2(cos(ang), sin(ang)), az = vec2(-ax.y, ax.x);
  float L = mix(0.07, 0.13, fract(h.x * 5.3)) * (1.0 + d * 0.012);
  vec2 xz = p + ax * a_pos.x * L * 0.55 + az * a_pos.y * L;
  vec2 gg = (xz - u_grid.xx) / u_grid.y;
  float y = heightAt(gg) + 0.012 + 0.01 * fract(h.y * 7.1);
  vec4 cc = u_viewProj * vec4(xz.x, y, xz.y, 1.0);
  v_wpos = vec3(xz.x, y, xz.y);
  vec4 vp = u_view * vec4(v_wpos, 1.0);
  v_depth = -vp.z;
  v_uv = a_pos;
  v_h = h;
  gl_Position = u_proj * vp;
}
)";

const char* LEAF_FS = R"(
in vec3 v_wpos;
in float v_depth;
in vec2 v_uv;
in vec4 v_h;
out vec4 fragColor;
void main(){
  float y = v_uv.y;
  float w = 0.95 * pow(max(0.0, 1.0 - y * y), 0.75);
  float lobe = 1.0 + 0.12 * sin(y * 9.0 + v_h.x * 6.0);
  if (abs(v_uv.x) > w * lobe) discard;
  float k = fract(v_h.z * 17.3 + v_h.y * 3.1);
  vec3 c0 = vec3(0.62, 0.16, 0.03), c1 = vec3(0.78, 0.36, 0.04), c2 = vec3(0.72, 0.55, 0.08), c3 = vec3(0.36, 0.17, 0.06);
  vec3 alb = k < 0.3 ? c0 : k < 0.6 ? c1 : k < 0.85 ? c2 : c3;
  alb *= 0.8 + 0.35 * fract(v_h.w * 29.0);
  alb *= mix(0.65, 1.0, smoothstep(0.0, 0.09, abs(v_uv.x)));
  float vein = smoothstep(0.06, 0.0, abs(abs(v_uv.x) - (y + 1.0) * 0.18 * w) ) * step(0.15, abs(v_uv.x));
  alb *= 1.0 - 0.15 * vein;
  alb = mix(alb, vec3(0.85, 0.88, 0.92), u_snow * 0.8);
  vec3 n = vec3(0.0, 1.0, 0.0);
  float sh = shadowAt(v_wpos, n);
  vec3 c = outColor(lambert(alb, n, sh));
  c = mix(c, u_fogCol.rgb, fogGround(v_wpos, v_depth));
  fragColor = vec4(c, 1.0);
}
)";

GLuint g_vao = 0, g_vbo = 0, g_leafVao = 0, g_leafVbo = 0;
Texture g_density;
int g_lastBlades = 0;

struct Ring { float spacing, rIn, rOut, fade, width, height, density; };

std::vector<uint8_t> g_densCpu;
const int GT = 48;
struct GTile { GLuint vbo = 0; int n = 0; uint64_t used = 0; size_t bytes = 0; };
std::unordered_map<uint64_t, GTile> g_tiles;
size_t g_tileBytes = 0;
uint64_t g_frame = 0;
GLuint g_bladeVao = 0;

float densAt(float x, float z) {
  float gx = (x - GRID_MIN) / GRID_STEP, gz = (z - GRID_MIN) / GRID_STEP;
  int ix = (int)std::floor(gx), iz = (int)std::floor(gz);
  float fx = gx - ix, fz = gz - iz;
  auto at = [](int i, int j) { i = std::max(0, std::min(GRID_N - 1, i)); j = std::max(0, std::min(GRID_N - 1, j)); return g_densCpu[(size_t)j * GRID_N + i] / 255.0f; };
  return (at(ix, iz) * (1 - fx) + at(ix + 1, iz) * fx) * (1 - fz) + (at(ix, iz + 1) * (1 - fx) + at(ix + 1, iz + 1) * fx) * fz;
}

void hash4(float px, float py, float o[4]) {
  float q[4] = {px * 0.1031f, py * 0.1030f, px * 0.0973f, py * 0.1099f};
  for (float& v : q) v -= std::floor(v);
  float dd = q[0] * (q[3] + 33.33f) + q[1] * (q[2] + 33.33f) + q[2] * (q[0] + 33.33f) + q[3] * (q[1] + 33.33f);
  for (float& v : q) v += dd;
  float a[4] = {q[0] + q[1], q[0] + q[2], q[1] + q[2], q[2] + q[3]}, b[4] = {q[2], q[1], q[3], q[0]};
  for (int i = 0; i < 4; i++) { o[i] = a[i] * b[i]; o[i] -= std::floor(o[i]); }
}

int g_newTiles = 0;
GTile* tileFor(int ring, float s, int tx, int tz, bool prewarm = false) {
  uint64_t key = ((uint64_t)ring << 56) ^ ((uint64_t)(uint32_t)(tx + 100000) << 28) ^ (uint64_t)(uint32_t)(tz + 100000);
  auto it = g_tiles.find(key);
  if (it != g_tiles.end()) { it->second.used = g_frame; return &it->second; }
  if (prewarm && g_newTiles >= 3) return nullptr;
  g_newTiles++;
  GTile& t = g_tiles[key];
  t.used = g_frame;
  std::vector<float> v;
  v.reserve(GT * GT * 8);
  for (int j = 0; j < GT; j++)
    for (int i = 0; i < GT; i++) {
      float cx = (float)(tx * GT + i), cz = (float)(tz * GT + j);
      float h[4];
      hash4(cx, cz, h);
      float x = (cx + h[0]) * s, z = (cz + h[1]) * s;
      if (std::fabs(x) > MAP_HALF || std::fabs(z) > MAP_HALF) continue;
      float dn = densAt(x, z);
      if (h[2] > dn) continue;
      float y = world::getTerrainHeight(x, z);
      float b[8] = {x, y, z, dn, h[0], h[1], h[2], h[3]};
      v.insert(v.end(), b, b + 8);
    }
  t.n = (int)(v.size() / 8);
  t.bytes = v.size() * 4;
  glGenBuffers(1, &t.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, t.vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)std::max<size_t>(t.bytes, 4), v.empty() ? nullptr : v.data(), GL_STATIC_DRAW);
  g_tileBytes += t.bytes;
  gfx::addBufferBytes((long long)t.bytes);
  return &t;
}

void evictTiles() {
  const size_t CAP = 192u << 20;
  if (g_tileBytes <= CAP) return;
  std::vector<std::pair<uint64_t, uint64_t>> order;
  for (auto& kv : g_tiles) if (kv.second.used + 2 < g_frame) order.push_back({kv.second.used, kv.first});
  std::sort(order.begin(), order.end());
  for (auto& o : order) {
    if (g_tileBytes <= CAP * 3 / 4) break;
    GTile& t = g_tiles[o.second];
    glDeleteBuffers(1, &t.vbo);
    g_tileBytes -= t.bytes;
    gfx::addBufferBytes(-(long long)t.bytes);
    g_tiles.erase(o.second);
  }
}

}  // namespace

namespace grass {

void init() {
  const float V[] = {-0.5f, 0, 0.5f, 0, -0.42f, 0.3f, 0.42f, 0.3f, -0.28f, 0.62f, 0.28f, 0.62f, 0, 1};
  glGenVertexArrays(1, &g_vao);
  glBindVertexArray(g_vao);
  glGenBuffers(1, &g_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof V, V, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
  glBindVertexArray(0);
  std::vector<uint8_t> d((size_t)GRID_N * GRID_N);
  for (int j = 0; j < GRID_N; j++)
    for (int i = 0; i < GRID_N; i++) {
      float x = GRID_MIN + i * GRID_STEP, z = GRID_MIN + j * GRID_STEP;
      float v = std::fabs(x) > MAP_HALF || std::fabs(z) > MAP_HALF ? 0 : forest::grassDensity(x, z);
      d[(size_t)j * GRID_N + i] = (uint8_t)std::lround(clampf(v, 0, 1) * 255);
    }
  g_density = uploadTextureR8(d.data(), GRID_N, GRID_N, true);
  g_densCpu = d;
  glGenVertexArrays(1, &g_bladeVao);
  glBindVertexArray(g_bladeVao);
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
  glEnableVertexAttribArray(6);
  glVertexAttribDivisor(6, 1);
  glEnableVertexAttribArray(7);
  glVertexAttribDivisor(7, 1);
  glBindVertexArray(0);
  const float Q[] = {-1, -1, 1, -1, -1, 1, 1, 1};
  glGenVertexArrays(1, &g_leafVao);
  glBindVertexArray(g_leafVao);
  glGenBuffers(1, &g_leafVbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_leafVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof Q, Q, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
  glBindVertexArray(0);
}

void draw() {
  float R = g_settings.fullMap ? 220.0f : std::max(40.0f, g_settings.grassRadius * 3.3f);
  R *= (0.5f + 0.5f * forest::detailLevel()) * (1.0f - 0.3f * season.snow);
  Ring rings[3] = {
      {0.17f, 0, std::min(26.0f, R * 0.25f), 6, 0.055f, 1.0f, 1.0f},
      {0.36f, std::min(26.0f, R * 0.25f), R * 0.55f, 6, 0.1f, 1.05f, 0.95f},
      {0.85f, R * 0.55f, R, 14, 0.24f, 1.1f, 0.9f},
  };
  static const std::string vs = std::string(terrain_render::groundCommon()) + GRASS_VS;
  Shader& s = render::program("grassgpu", vs.c_str(), GRASS_FS);
  s.set1i("u_bake", 4);
  s.set4f("u_grid", GRID_MIN, GRID_STEP, (float)GRID_N, 0);
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, terrain_render::bakeTexture());
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(g_bladeVao);
  g_lastBlades = 0;
  g_frame++;
  const Camera& cam = render::cam;
  float snowK = 1.0f - 0.75f * season.snow;
  for (int ri = 0; ri < 3; ri++) {
    const Ring& r = rings[ri];
    s.set4f("u_ring", r.spacing, r.rIn, r.rOut, r.fade);
    s.set4f("u_blade", r.width, r.height, r.density * snowK, 0);
    float ts = GT * r.spacing;
    int tx0 = (int)std::floor((cam.pos.x - r.rOut) / ts), tx1 = (int)std::floor((cam.pos.x + r.rOut) / ts);
    int tz0 = (int)std::floor((cam.pos.z - r.rOut) / ts), tz1 = (int)std::floor((cam.pos.z + r.rOut) / ts);
    for (int tz = tz0; tz <= tz1; tz++)
      for (int tx = tx0; tx <= tx1; tx++) {
        float x0 = tx * ts, z0 = tz * ts, x1 = x0 + ts + r.spacing, z1 = z0 + ts + r.spacing;
        float nx = clampf(cam.pos.x, x0, x1) - cam.pos.x, nz = clampf(cam.pos.z, z0, z1) - cam.pos.z;
        if (nx * nx + nz * nz > r.rOut * r.rOut) continue;
        float fx = std::max(std::fabs(x0 - cam.pos.x), std::fabs(x1 - cam.pos.x)), fz = std::max(std::fabs(z0 - cam.pos.z), std::fabs(z1 - cam.pos.z));
        float inner = std::max(0.0f, r.rIn - r.fade);
        if (fx * fx + fz * fz < inner * inner) continue;
        float h0 = 1e9f, h1 = -1e9f;
        for (float u : {0.0f, 0.5f, 1.0f})
          for (float v : {0.0f, 0.5f, 1.0f}) {
            float h = world::getTerrainHeight(x0 + (x1 - x0) * u, z0 + (z1 - z0) * v);
            h0 = std::min(h0, h), h1 = std::max(h1, h);
          }
        if (!cam.box({x0, h0 - 4, z0}, {x1, h1 + 6, z1})) continue;
        GTile& t = *tileFor(ri, r.spacing, tx, tz);
        if (t.n <= 0) continue;
        glBindBuffer(GL_ARRAY_BUFFER, t.vbo);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 32, (void*)0);
        glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, 32, (void*)16);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 7, t.n);
        gfx::drawCalls++;
        g_lastBlades += t.n;
      }
  }
  for (int ri = 0; ri < 3; ri++) {
    const Ring& r = rings[ri];
    float ts = GT * r.spacing, R2 = r.rOut + ts;
    int tx0 = (int)std::floor((cam.pos.x - R2) / ts), tx1 = (int)std::floor((cam.pos.x + R2) / ts);
    int tz0 = (int)std::floor((cam.pos.z - R2) / ts), tz1 = (int)std::floor((cam.pos.z + R2) / ts);
    for (int tz = tz0; tz <= tz1 && g_newTiles < 3; tz++)
      for (int tx = tx0; tx <= tx1 && g_newTiles < 3; tx++) {
        float x0 = tx * ts, z0 = tz * ts;
        float nx = clampf(cam.pos.x, x0, x0 + ts) - cam.pos.x, nz = clampf(cam.pos.z, z0, z0 + ts) - cam.pos.z;
        if (nx * nx + nz * nz > R2 * R2) continue;
        tileFor(ri, r.spacing, tx, tz, true);
      }
  }
  g_newTiles = 0;
  evictTiles();
  glBindVertexArray(0);
  float amt = clampf(season.autumn * 1.1f + season.fall * 0.5f - season.spring * 0.6f - season.dry * 0.4f, 0, 1);
  if (amt > 0.03f) {
    Shader& l = render::program("leaflitter", LEAF_VS, LEAF_FS);
    l.set1i("u_height", 1);
    l.set1i("u_density", 3);
    l.set4f("u_grid", GRID_MIN, GRID_STEP, (float)GRID_N, 0);
    const float sp = 0.24f, RL = 26;
    int n = (int)std::ceil(2 * RL / sp) + 2;
    l.set4f("u_leaf", sp, RL, amt * 0.75f, 0);
    l.set1i("u_n", n);
    glBindVertexArray(g_leafVao);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, n * n);
    gfx::drawCalls++;
    glBindVertexArray(0);
  }
}

int bladeCount() { return g_lastBlades; }

void shutdown() {
  if (g_vbo) glDeleteBuffers(1, &g_vbo);
  if (g_vao) glDeleteVertexArrays(1, &g_vao);
  g_density.destroy();
  if (g_leafVbo) glDeleteBuffers(1, &g_leafVbo);
  if (g_leafVao) glDeleteVertexArrays(1, &g_leafVao);
}

}  // namespace grass
