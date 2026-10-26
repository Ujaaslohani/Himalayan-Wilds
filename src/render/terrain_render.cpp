#include <cstring>
#include "terrain_render.h"
#include "../world/terrain.h"
#include "../platform/platform.h"
#include <vector>
#include <cmath>
#include "../game/season.h"
#include "render.h"

namespace {

const char* GROUND_COMMON = R"(
vec3 groundShade(vec3 sc, vec3 wp, float ny, float road){
  float sG = clamp((sc.g - max(sc.r, sc.b)) * 14.0, 0.0, 1.0);
  float sN1 = 0.5 + 0.5 * sin(wp.x * 2.7 + sin(wp.z * 2.1)) * sin(wp.z * 3.1 + sin(wp.x * 1.9));
  float sN2 = 0.5 + 0.5 * sin(wp.x * 0.21 + sin(wp.z * 0.17) * 2.0) * sin(wp.z * 0.19 + 1.3);
  vec3 sAu = mix(vec3(0.42, 0.26, 0.07), vec3(0.5, 0.18, 0.05), sN2);
  sc = mix(sc, sAu * (0.9 + 0.2 * sN1), u_autumn * sG * 0.45);
  sc = mix(sc, sc * vec3(0.88, 1.18, 0.82), u_spring * sG);
  sc = mix(sc, sc * vec3(1.25, 1.05, 0.6), u_dry * sG * (0.35 + 0.45 * sN2));
#ifndef GROUND_BAKE
  if (road > 0.01) {
    float sCam = 1.0 - smoothstep(20.0, 70.0, length(wp - u_camPos));
    float sG1 = vnoise(wp.xz * 7.0);
    float sG2 = vnoise(wp.xz * 2.3 + 17.0);
    float sPeb = smoothstep(0.74, 0.84, vnoise(wp.xz * 6.5 + 3.0));
    float sDk = smoothstep(0.74, 0.84, vnoise(wp.xz * 5.3 - 7.0));
    vec3 sGv = sc * (0.84 + 0.22 * sG1 + 0.12 * sG2);
    sGv = mix(sGv, sc * 1.25, sPeb * 0.45);
    sGv = mix(sGv, sc * 0.52, sDk * 0.55);
    sc = mix(sc, sGv, road * sCam);
  }
#endif
#ifdef GROUND_BAKE
  float sDist = 0.0;
#else
  float sDist = length(wp - u_camPos);
#endif
  bool sNear = sDist < 90.0;
  float sSteep = 1.0 - smoothstep(0.42, 0.8, ny);
  if (sSteep > 0.01) {
    if (sNear) {
      float sSt = vnoise(wp.xz * 0.35 + vec2(wp.y * 0.21, -wp.y * 0.17));
      float sCr = vnoise(wp.xz * 0.9 + wp.y * 0.3);
      sc *= mix(1.0, (0.86 + 0.2 * sSt) * (0.86 + 0.2 * smoothstep(0.2, 0.65, sCr)), sSteep * smoothstep(60.0, 120.0, wp.y));
    } else {
      float sSt = vnoise(wp.xz * 0.02 + vec2(wp.y * 0.013, -wp.y * 0.017));
      sc *= mix(1.0, (0.88 + 0.18 * sSt) * 0.96, sSteep * smoothstep(70.0, 130.0, wp.y));
    }
  }
  float sFlat = smoothstep(0.68, 0.92, ny);
  float sDry = step(-0.55, wp.y);
  float sPatch = u_snow > 0.01 ? smoothstep(0.3, 0.7, vnoise(wp.xz * 0.045) * 0.7 + (sNear ? vnoise(wp.xz * 0.31) : 0.5) * 0.3) : 0.0;
  float sAmt = u_snow * sFlat * sDry * mix(0.55, 1.0, sPatch) * mix(0.8, 1.0, sG);
  vec3 sCol = mix(vec3(0.72, 0.77, 0.85), vec3(0.88, 0.91, 0.96), sPatch) * (0.92 + 0.08 * sN1);
  sc = mix(sc, sCol, sAmt);
  return sc;
}
float mountainDetail(vec3 wp, float ny){
  float steep = 1.0 - smoothstep(0.45, 0.85, ny);
#ifndef GROUND_BAKE
  vec3 big = texture(u_ptexA, vec2(wp.x + wp.z * 0.6, wp.y * 1.3) * 0.012).rgb * 2.0;
  float bl = dot(big, vec3(0.333));
#else
  float bl = 1.0;
#endif
  vec2 sp = vec2(wp.x + wp.z * 0.7, wp.y) + vec2(vnoise(wp.xz * 0.006) * 90.0, vnoise(wp.zx * 0.008 + 3.0) * 40.0);
#ifndef GROUND_BAKE
  sp *= mix(1.0, 0.2, smoothstep(150.0, 700.0, length(wp - u_camPos)));
#endif
  float rb = vnoise(sp * vec2(0.03, 0.05)) * 0.5 + vnoise(wp.xz * 0.011 + 4.0) * 0.3 + vnoise(sp * 0.13 + 2.0) * 0.2;
  float gul = smoothstep(0.5, 0.85, vnoise(vec2(sp.x * 0.14, sp.y * 0.012)));
  float rid = smoothstep(0.62, 0.92, vnoise(vec2(sp.x * 0.09, sp.y * 0.02) + 7.0));
  float hi = smoothstep(80.0, 150.0, wp.y);
  return mix(1.0, mix(0.6, 1.3, rb) * (1.0 - 0.4 * gul) * (1.0 + 0.28 * rid) * mix(1.0, bl, 0.7), (0.3 + 0.7 * steep) * mix(0.3, 1.0, hi));
}
vec3 groundBump(vec3 n, vec3 wp, float road){
  float sK = (0.42 + 0.35 * road) * (1.0 - smoothstep(25.0, 70.0, length(wp - u_camPos)));
  if (sK > 0.001) {
    vec2 q = wp.xz;
    vec2 g = vec2(cos(q.x * 1.9 + sin(q.y * 1.3) * 1.7), cos(q.y * 2.1 + sin(q.x * 1.1) * 1.5)) * 0.6
      + vec2(cos(q.x * 5.3 + q.y * 0.8), cos(q.y * 4.9 - q.x * 1.2)) * 0.4;
    n = normalize(n + vec3(-g.x * sK, 0.0, -g.y * sK));
  }
  return n;
}
)";

const char* BAKE_VS = R"(
out vec2 v_uv;
void main(){
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
  v_uv = p * 0.5 + 0.5;
  gl_Position = vec4(p, 0.0, 1.0);
}
)";
const char* BAKE_FS = R"(
in vec2 v_uv;
uniform sampler2D u_height;
uniform sampler2D u_color;
uniform vec4 u_grid;
uniform vec4 u_tile;
out vec4 fragColor;
float hAt(ivec2 c){ c = clamp(c, ivec2(0), ivec2(int(u_grid.z) - 1)); return texelFetch(u_height, c, 0).r; }
float heightAt(vec2 g){
  vec2 i = floor(g); vec2 f = g - i; ivec2 c = ivec2(i);
  float a = hAt(c), b = hAt(c + ivec2(1, 0)), cc = hAt(c + ivec2(0, 1)), d = hAt(c + ivec2(1, 1));
  if (f.x + f.y <= 1.0) return a + (b - a) * f.x + (cc - a) * f.y;
  return d + (cc - d) * (1.0 - f.x) + (b - d) * (1.0 - f.y);
}
void main(){
  vec2 uv = u_tile.xy + v_uv * u_tile.zw;
  vec2 g = uv * (u_grid.z - 1.0);
  vec2 w = u_grid.x + g * u_grid.y;
  float h = heightAt(g);
  ivec2 ci = ivec2(floor(g + 0.5));
  float hl = hAt(ci - ivec2(1, 0)), hr = hAt(ci + ivec2(1, 0)), hd = hAt(ci - ivec2(0, 1)), hu = hAt(ci + ivec2(0, 1));
  vec3 n = normalize(vec3(hl - hr, 2.0 * u_grid.y, hd - hu));
  vec4 t = texture(u_color, (g + 0.5) / u_grid.z);
  vec3 sc = groundShade(srgbToLinear(t.rgb), vec3(w.x, h, w.y), n.y, t.a);
  // baked horizon ambient occlusion (valleys, gullies, cliff bases)
  float occ = 0.0;
  for (int a = 0; a < 8; a++) {
    float ang = float(a) * 0.785398 + 0.39;
    vec2 dir = vec2(cos(ang), sin(ang));
    float hz = 0.0;
    for (int k = 0; k < 4; k++) {
      float dm = 3.0 * pow(2.6, float(k));
      float hs = heightAt(g + dir * (dm / u_grid.y));
      hz = max(hz, (hs - h) / dm);
    }
    occ += clamp(hz, 0.0, 1.5);
  }
  float ao = 1.0 - clamp(occ / 8.0 * 0.85, 0.0, 0.6);
  // cavity: concave creases darker, convex ridges lighter
  float lap = (hl + hr + hd + hu - 4.0 * hAt(ci)) / u_grid.y;
  float cav = clamp(1.0 - lap * 0.35, 0.72, 1.18);
  float steep = 1.0 - smoothstep(0.45, 0.85, n.y);
  // rock macro detail on steep ground: blotches, cracks, weathering streaks
  vec3 wp3 = vec3(w.x, h, w.y);
  float rb = fbm4(vec2(wp3.x + wp3.z, wp3.y * 1.7) * 0.09) * 0.6 + fbm3(wp3.xz * 0.23) * 0.4;
  float rc = 1.0 - abs(vnoise(vec2((wp3.x - wp3.z) * 0.3, wp3.y * 0.45)) * 2.0 - 1.0);
  float streak = vnoise(vec2((wp3.x + wp3.z) * 0.8, wp3.y * 0.06));
  vec3 rockT = mix(vec3(0.72, 0.69, 0.64), vec3(1.22, 1.15, 1.03), smoothstep(0.25, 0.75, rb)) * (1.0 - 0.45 * smoothstep(0.86, 0.97, rc)) * mix(0.9, 1.08, streak);
  sc *= mix(vec3(1.0), rockT, steep * 0.85 * max(smoothstep(70.0, 125.0, h), 1.0 - smoothstep(0.42, 0.66, n.y)));
  sc *= ao * mix(1.0, cav, 0.8);
  sc = mix(sc, sc * vec3(0.9, 1.12, 0.82), (1.0 - smoothstep(70.0, 130.0, h)) * 0.6 * smoothstep(0.5, 0.75, n.y));
  fragColor = vec4(sqrt(max(sc, vec3(0.0))), t.a);
}
)";

const char* TERRAIN_VS = R"(
in vec4 a_grid;
in vec4 a_i0;
uniform sampler2D u_height;
uniform sampler2D u_color;
uniform sampler2D u_bake;
out vec3 v_farCol;
uniform vec4 u_grid;
uniform float u_range[8];
out vec3 v_wpos;
out vec2 v_cell;
out float v_depth;
out vec3 v_nrm;
float hAt(ivec2 c){ c = clamp(c, ivec2(0), ivec2(int(u_grid.z) - 1)); return texelFetch(u_height, c, 0).r; }
float heightAt(vec2 g){
  vec2 i = floor(g); vec2 f = g - i; ivec2 c = ivec2(i);
  float a = hAt(c), b = hAt(c + ivec2(1, 0)), cc = hAt(c + ivec2(0, 1)), d = hAt(c + ivec2(1, 1));
  if (f.x + f.y <= 1.0) return a + (b - a) * f.x + (cc - a) * f.y;
  return d + (cc - d) * (1.0 - f.x) + (b - d) * (1.0 - f.y);
}
void main(){
  float st = a_i0.z;
  int lvl = int(a_i0.w);
  vec2 gp = a_grid.xy;
  vec2 cell = a_i0.xy + gp * st;
  vec2 w = u_grid.x + cell * u_grid.y;
  float d = distance(vec3(w.x, 0.0, w.y), vec3(u_camPos.x, 0.0, u_camPos.z));
  float r = u_range[lvl];
  float k = clamp((d - r * 0.62) / (r * 0.3), 0.0, 1.0);
  vec2 fr = fract(gp * 0.5) * 2.0;
  cell = a_i0.xy + (gp - fr * k) * st;
  w = u_grid.x + cell * u_grid.y;
  float h = heightAt(cell);
  if (a_grid.z > 0.5) h -= 1.5 + st * 1.2;
  ivec2 ci = ivec2(floor(cell + 0.5));
  float hl = hAt(ci - ivec2(1, 0)), hr = hAt(ci + ivec2(1, 0)), hd = hAt(ci - ivec2(0, 1)), hu = hAt(ci + ivec2(0, 1));
  v_nrm = normalize(vec3(hl - hr, 2.0 * u_grid.y, hd - hu));
  v_wpos = vec3(w.x, h, w.y);
  v_cell = cell;
  vec4 vp = u_view * vec4(v_wpos, 1.0);
  v_depth = -vp.z;
  v_farCol = vec3(0.0);
  if (d > 40.0) {
    vec4 t = textureLod(u_bake, cell / (u_grid.z - 1.0), clamp(log2(d / 30.0), 0.0, 6.0));
    vec3 sc = t.rgb * t.rgb * mountainDetail(v_wpos, v_nrm.y);
    float sh = shadowAt(v_wpos, v_nrm);
    vec3 c = outColor(lambert(sc, v_nrm, sh));
    v_farCol = mix(c, fogTint(v_wpos), fogGround(v_wpos, v_depth));
  }
  gl_Position = u_proj * vp;
}
)";

const char* TERRAIN_FS = R"(
in vec3 v_wpos;
in vec2 v_cell;
in float v_depth;
in vec3 v_nrm;
uniform sampler2D u_color;
uniform vec4 u_grid;
in vec3 v_farCol;
uniform sampler2D u_bake;
out vec4 fragColor;
void main(){
  float fd = distance(v_wpos.xz, u_camPos.xz);
  if (fd > 85.0) { fragColor = vec4(v_farCol, 1.0); return; }
  vec4 b = texture(u_bake, v_cell / (u_grid.z - 1.0));
  vec3 sc = b.rgb * b.rgb;
  float road = b.a;
  vec3 n = normalize(v_nrm);
  if (road > 0.01 && fd < 70.0) {
    vec4 t = texture(u_color, (v_cell + 0.5) / u_grid.z);
    sc = groundShade(srgbToLinear(t.rgb), v_wpos, n.y, t.a);
  }
  vec3 nb = groundBump(n, v_wpos, road);
  // snow surface: wind-carved ripples and soft blue shade in hollows
  {
    float lum = dot(sc, vec3(0.3, 0.59, 0.11));
    float snowy = smoothstep(0.42, 0.62, lum) * (1.0 - smoothstep(0.06, 0.16, max(sc.r, max(sc.g, sc.b)) - min(sc.r, min(sc.g, sc.b)))) * (1.0 - smoothstep(55.0, 85.0, fd));
    if (snowy > 0.01) {
      vec2 q = v_wpos.xz;
      float rip = vnoise(vec2(q.x * 0.35 + vnoise(q * 0.08) * 3.0, q.y * 1.4));
      float drift = vnoise(q * 0.05);
      float hol = clamp(rip * 0.6 + (1.0 - drift) * 0.4, 0.0, 1.0);
      sc = mix(sc, sc * mix(vec3(1.03, 1.03, 1.04), vec3(0.84, 0.9, 1.0), hol), snowy);
      vec2 g = vec2(vnoise(vec2((q.x + 0.4) * 0.35, q.y * 1.4)) - vnoise(vec2((q.x - 0.4) * 0.35, q.y * 1.4)), vnoise(vec2(q.x * 0.35, (q.y + 0.3) * 1.4)) - vnoise(vec2(q.x * 0.35, (q.y - 0.3) * 1.4)));
      nb = normalize(nb + vec3(-g.x, 0.0, -g.y) * 0.35 * snowy);
    }
  }
  float steepN = (1.0 - smoothstep(0.4, 0.82, n.y)) * (1.0 - smoothstep(55.0, 85.0, fd)) * mix(0.35, 1.0, max(smoothstep(70.0, 125.0, v_wpos.y), 1.0 - smoothstep(0.4, 0.62, n.y)));
  if (steepN > 0.01) {
    vec3 nr = nb;
    vec3 alb = ptexRock(v_wpos, nr, 0.16, 1.0);
    sc *= mix(vec3(1.0), alb, steepN);
    nb = normalize(mix(nb, nr, steepN));
  }
  float sh = shadowAt(v_wpos, n);
  vec3 c = outColor(lambert(sc, nb, sh));
  c = mix(c, fogTint(v_wpos), fogGround(v_wpos, v_depth));
  if (fd > 70.0) c = mix(c, v_farCol, smoothstep(70.0, 85.0, fd));
  fragColor = vec4(c, 1.0);
}
)";

const char* MESH_VS = R"(
in vec3 a_pos;
in vec4 a_col;
in vec4 a_nrm;
out vec3 v_wpos;
out vec3 v_col;
out vec3 v_nrm;
out float v_depth;
void main(){
  v_wpos = a_pos;
  v_col = srgbToLinear(a_col.rgb);
  v_nrm = a_nrm.xyz;
  vec4 vp = u_view * vec4(a_pos, 1.0);
  v_depth = -vp.z;
  gl_Position = u_proj * vp;
}
)";

const char* MESH_FS = R"(
in vec3 v_wpos;
in vec3 v_col;
in vec3 v_nrm;
in float v_depth;
out vec4 fragColor;
void main(){
  vec3 n = normalize(v_nrm);
  vec3 sc = groundShade(v_col, v_wpos, n.y, 0.0);
  sc *= mountainDetail(v_wpos, n.y);
  vec3 nb = groundBump(n, v_wpos, 0.0);
  if (v_wpos.y > 120.0) {
    // alpine look: snow clings to gentler faces above the snowline, steep faces show streaked rock
    vec2 sp = vec2(v_wpos.x + v_wpos.z * 0.6, v_wpos.y);
    float gul = vnoise(vec2(sp.x * 0.03, sp.y * 0.004));
    float line = 210.0 + vnoise(v_wpos.xz * 0.004) * 70.0;
    float alt = smoothstep(line - 40.0, line + 30.0, v_wpos.y + gul * 30.0);
    float slope = smoothstep(0.42, 0.72, n.y + (gul - 0.5) * 0.45 + vnoise(sp * 0.02) * 0.2);
    float snow = alt * slope;
    vec3 rockc = mix(vec3(0.16, 0.15, 0.14), vec3(0.34, 0.31, 0.28), vnoise(sp * vec2(0.02, 0.05))) * mix(0.75, 1.15, gul);
    vec3 big = texture(u_ptexA, sp * 0.004).rgb * 2.0;
    rockc *= mix(vec3(1.0), big, 0.3);
    float rocky = smoothstep(165.0, 230.0, v_wpos.y);
    sc = mix(sc, rockc, rocky * (1.0 - snow) * 0.85);
    sc = mix(sc, vec3(0.86, 0.89, 0.93) * mix(0.92, 1.04, gul), snow);
  }
  float sh = shadowAt(v_wpos, n);
  vec3 lit = lambert(sc, nb, sh);
  // shaded faces pick up blue sky light, sunlit faces stay warm: gives the peaks depth
  float facing = max(dot(nb, u_sunDir.xyz), 0.0);
  lit += sc * u_hemiSky.rgb * 0.35 * (1.0 - facing) * smoothstep(100.0, 250.0, v_wpos.y);
  vec3 c = outColor(lit);
  float fg = fogGround(v_wpos, v_depth) * mix(1.0, 0.6, smoothstep(160.0, 380.0, v_wpos.y));
  c = mix(c, fogTint(v_wpos), fg);
  fragColor = vec4(c, 1.0);
}
)";

constexpr int PATCH = 16;
constexpr int LEVELS = 6;
Texture g_heightTex, g_colorTex;
GLuint g_vao = 0, g_vbo = 0, g_ibo = 0, g_ivbo = 0;
GLsizei g_indexCount = 0;
size_t g_icap = 0;
Mesh g_backdrop;
std::vector<float> g_minH[LEVELS], g_maxH[LEVELS];
int g_nodes[LEVELS];
float g_range[LEVELS];
std::vector<float> g_inst;
int g_lastCount = 0;

void buildPatchMesh() {
  std::vector<uint8_t> v;
  auto put = [&](int x, int z, int skirt) { v.push_back((uint8_t)x); v.push_back((uint8_t)z); v.push_back((uint8_t)skirt); v.push_back(0); };
  int N = PATCH + 1;
  for (int z = 0; z < N; z++)
    for (int x = 0; x < N; x++) put(x, z, 0);
  std::vector<uint16_t> idx;
  for (int z = 0; z < PATCH; z++)
    for (int x = 0; x < PATCH; x++) {
      uint16_t a = (uint16_t)(z * N + x), b = a + 1, c = (uint16_t)(a + N), d = (uint16_t)(c + 1);
      idx.insert(idx.end(), {a, c, b, b, c, d});
    }
  auto skirtEdge = [&](int x0, int z0, int dx, int dz, bool flip) {
    uint16_t base = (uint16_t)(v.size() / 4);
    for (int i = 0; i <= PATCH; i++) put(x0 + dx * i, z0 + dz * i, 1);
    for (int i = 0; i < PATCH; i++) {
      uint16_t t0 = (uint16_t)((z0 + dz * i) * N + x0 + dx * i), t1 = (uint16_t)((z0 + dz * (i + 1)) * N + x0 + dx * (i + 1));
      uint16_t s0 = base + i, s1 = base + i + 1;
      if (flip) idx.insert(idx.end(), {t0, t1, s0, s0, t1, s1});
      else idx.insert(idx.end(), {t0, s0, t1, s0, s1, t1});
    }
  };
  skirtEdge(0, 0, 1, 0, false);
  skirtEdge(0, PATCH, 1, 0, true);
  skirtEdge(0, 0, 0, 1, true);
  skirtEdge(PATCH, 0, 0, 1, false);
  glGenVertexArrays(1, &g_vao);
  glBindVertexArray(g_vao);
  glGenBuffers(1, &g_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)v.size(), v.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_FALSE, 4, nullptr);
  glGenBuffers(1, &g_ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size() * 2), idx.data(), GL_STATIC_DRAW);
  g_indexCount = (GLsizei)idx.size();
  glGenBuffers(1, &g_ivbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_ivbo);
  glEnableVertexAttribArray(6);
  glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 16, nullptr);
  glVertexAttribDivisor(6, 1);
  glBindVertexArray(0);
  gfx::addBufferBytes((long long)(v.size() + idx.size() * 2));
}

void buildBounds() {
  const auto& H = world::heightGrid();
  for (int L = 0; L < LEVELS; L++) {
    int size = PATCH << L;
    int n = (TERRAIN_SEGMENTS + size - 1) / size;
    g_nodes[L] = n;
    g_minH[L].assign((size_t)n * n, 1e9f);
    g_maxH[L].assign((size_t)n * n, -1e9f);
  }
  int n0 = g_nodes[0];
  for (int nz = 0; nz < n0; nz++)
    for (int nx = 0; nx < n0; nx++) {
      float mn = 1e9f, mx = -1e9f;
      for (int z = nz * PATCH; z <= std::min(GRID_N - 1, (nz + 1) * PATCH); z++)
        for (int x = nx * PATCH; x <= std::min(GRID_N - 1, (nx + 1) * PATCH); x++) {
          float h = H[(size_t)z * GRID_N + x];
          mn = std::min(mn, h);
          mx = std::max(mx, h);
        }
      g_minH[0][(size_t)nz * n0 + nx] = mn;
      g_maxH[0][(size_t)nz * n0 + nx] = mx;
    }
  for (int L = 1; L < LEVELS; L++) {
    int n = g_nodes[L], nc = g_nodes[L - 1];
    for (int nz = 0; nz < n; nz++)
      for (int nx = 0; nx < n; nx++)
        for (int c = 0; c < 4; c++) {
          int cx = nx * 2 + (c & 1), cz = nz * 2 + (c >> 1);
          if (cx >= nc || cz >= nc) continue;
          size_t k = (size_t)nz * n + nx, kc = (size_t)cz * nc + cx;
          g_minH[L][k] = std::min(g_minH[L][k], g_minH[L - 1][kc]);
          g_maxH[L][k] = std::max(g_maxH[L][k], g_maxH[L - 1][kc]);
        }
  }
  float leafM = PATCH * GRID_STEP;
  for (int L = 0; L < LEVELS; L++) g_range[L] = leafM * 2.4f * std::max(1.0f, std::sqrt(g_settings.drawDist())) * (float)(1 << L);
}

float boxDist(const Vec3& p, const Vec3& mn, const Vec3& mx) {
  float dx = std::max(0.0f, std::max(mn.x - p.x, p.x - mx.x));
  float dy = std::max(0.0f, std::max(mn.y - p.y, p.y - mx.y));
  float dz = std::max(0.0f, std::max(mn.z - p.z, p.z - mx.z));
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void select(int L, int nx, int nz) {
  int n = g_nodes[L];
  if (nx < 0 || nz < 0 || nx >= n || nz >= n) return;
  int size = PATCH << L;
  size_t k = (size_t)nz * n + nx;
  Vec3 mn(GRID_MIN + nx * size * GRID_STEP, g_minH[L][k] - 4, GRID_MIN + nz * size * GRID_STEP);
  Vec3 mx(mn.x + size * GRID_STEP, g_maxH[L][k] + 1, mn.z + size * GRID_STEP);
  if (mn.x >= GRID_MIN + TERRAIN_SEGMENTS * GRID_STEP || mn.z >= GRID_MIN + TERRAIN_SEGMENTS * GRID_STEP) return;
  if (!render::cam.box(mn, mx)) return;
  float d = boxDist(render::cam.pos, mn, mx);
  if (L == 0 || d > g_range[L - 1]) {
    g_inst.push_back((float)(nx * size));
    g_inst.push_back((float)(nz * size));
    g_inst.push_back((float)(1 << L));
    g_inst.push_back((float)L);
    return;
  }
  for (int c = 0; c < 4; c++) select(L - 1, nx * 2 + (c & 1), nz * 2 + (c >> 1));
}

}  // namespace

namespace terrain_render {

void init() {
  const auto& H = world::heightGrid();
  g_heightTex = createFloatTexture(H.data(), GRID_N, GRID_N);
  const auto& C = world::colorGrid();
  g_colorTex = createRGBA8Texture((const uint8_t*)C.data(), GRID_N, GRID_N, true);
  buildPatchMesh();
  buildBounds();
  g_backdrop = uploadMesh(world::buildBackdrop());
}

GLuint g_bakeTex = 0, g_bakeFbo = 0;
GLuint heightTexture() { return g_heightTex.id; }
GLuint bakeTexture() { return g_bakeTex; }
GLuint colorTexture() { return g_colorTex.id; }
const char* groundCommon() { return GROUND_COMMON; }
int patchCount() { return g_lastCount; }

const int BAKE_N = 3072, BAKE_T = 512;
int g_bakeNext = -1;
float g_bakeSig = -1, g_bakePending = 0;

float bakeSig() {
  return std::round(season.autumn * 20) + std::round(season.snow * 20) * 31 + std::round(season.spring * 20) * 961 + std::round(season.dry * 20) * 29791;
}

void bakeTile(int i) {
  int tiles = BAKE_N / BAKE_T;
  int tx = i % tiles, tz = i / tiles;
  static const std::string fsB = std::string(GROUND_COMMON) + BAKE_FS;
  Shader& b = render::program("terrainbake", BAKE_VS, fsB.c_str(), "#define GROUND_BAKE\n");
  b.set1i("u_height", 1);
  b.set1i("u_color", 2);
  b.set4f("u_grid", GRID_MIN, GRID_STEP, (float)GRID_N, 0);
  float f = 1.0f / tiles;
  b.set4f("u_tile", tx * f, tz * f, f, f);
  glViewport(tx * BAKE_T, tz * BAKE_T, BAKE_T, BAKE_T);
  render::drawFullscreen();
}

void updateBake() {
  if (!g_bakeTex) {
    glGenTextures(1, &g_bakeTex);
    glBindTexture(GL_TEXTURE_2D, g_bakeTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, BAKE_N, BAKE_N, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 6);
    glGenerateMipmap(GL_TEXTURE_2D);
    gfx::addBufferBytes((long long)BAKE_N * BAKE_N * 4 * 4 / 3);
    glGenFramebuffers(1, &g_bakeFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_bakeFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_bakeTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  int tiles = BAKE_N / BAKE_T, total = tiles * tiles;
  bool first = g_bakeSig < 0;
  if (g_bakeNext < 0) {
    float sg = bakeSig();
    if (sg == g_bakeSig) return;
    g_bakePending = sg;
    g_bakeNext = 0;
  }
  GLint vp[4], prev = 0;
  glGetIntegerv(GL_VIEWPORT, vp);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
  glBindFramebuffer(GL_FRAMEBUFFER, g_bakeFbo);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_BLEND);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, g_heightTex.id);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, g_colorTex.id);
  glActiveTexture(GL_TEXTURE0);
  int n = first ? total : 1;
  for (int k = 0; k < n && g_bakeNext < total; k++) bakeTile(g_bakeNext++);
  if (g_bakeNext >= total) {
    glBindTexture(GL_TEXTURE_2D, g_bakeTex);
    glGenerateMipmap(GL_TEXTURE_2D);
    g_bakeSig = g_bakePending;
    g_bakeNext = -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, prev);
  glViewport(vp[0], vp[1], vp[2], vp[3]);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
}

void draw() {
  updateBake();
  g_inst.clear();
  int top = LEVELS - 1;
  for (int z = 0; z < g_nodes[top]; z++)
    for (int x = 0; x < g_nodes[top]; x++) select(top, x, z);
  g_lastCount = (int)(g_inst.size() / 4);
  if (g_lastCount) {
    static const std::string fsT = std::string(GROUND_COMMON) + TERRAIN_FS;
    static const std::string vsT = std::string(GROUND_COMMON) + TERRAIN_VS;
    Shader& s = render::program("terrain", vsT.c_str(), fsT.c_str());
    s.set1i("u_height", 1);
    s.set1i("u_color", 2);
    s.set1i("u_bake", 4);
    s.set4f("u_grid", GRID_MIN, GRID_STEP, (float)GRID_N, 0);
    glUniform1fv(s.loc("u_range"), LEVELS, g_range);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, g_bakeTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_heightTex.id);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_colorTex.id);
    glActiveTexture(GL_TEXTURE0);
    glBindBuffer(GL_ARRAY_BUFFER, g_ivbo);
    size_t bytes = g_inst.size() * 4;
    static std::vector<float> prev;
    if (bytes > g_icap) {
      g_icap = bytes * 2;
      glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)g_icap, nullptr, GL_DYNAMIC_DRAW);
      prev.clear();
    }
    if (prev.size() != g_inst.size() || std::memcmp(prev.data(), g_inst.data(), bytes) != 0) {
      glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)g_icap, nullptr, GL_DYNAMIC_DRAW);
      glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, g_inst.data());
      prev.assign(g_inst.begin(), g_inst.end());
    }
    glDisable(GL_CULL_FACE);
    glBindVertexArray(g_vao);
    glDrawElementsInstanced(GL_TRIANGLES, g_indexCount, GL_UNSIGNED_SHORT, nullptr, g_lastCount);
    gfx::drawCalls++;
    gfx::triangles += (long long)(g_indexCount / 3) * g_lastCount;
  }
  drawGroundMesh(g_backdrop);
}

void drawGroundMesh(const Mesh& m) {
  if (!m.valid()) return;
  static const std::string fsM = std::string(GROUND_COMMON) + MESH_FS;
  render::program("groundmesh", MESH_VS, fsM.c_str());
  m.draw();
}

void shutdown() {
  g_heightTex.destroy();
  g_colorTex.destroy();
  g_backdrop.destroy();
}

}  // namespace terrain_render
