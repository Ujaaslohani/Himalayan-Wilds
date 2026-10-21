#include "water.h"
#include "render.h"
#include "../world/terrain.h"
#include "../game/season.h"
#include <vector>

namespace {

struct WVert { float p[3]; float flow[4]; float t[2]; };

const char* WATER_VS = R"(
in vec3 a_pos;
in vec4 a_col;
in vec4 a_nrm;
out vec3 v_wpos;
out vec4 v_flow;
out vec2 v_tan;
out float v_depth;
void main(){
  v_wpos = a_pos;
  v_flow = a_col;
  v_tan = a_nrm.xy;
  vec4 vp = u_view * vec4(a_pos, 1.0);
  v_depth = -vp.z;
  gl_Position = u_proj * vp;
}
)";

const char* WATER_FS = R"(
in vec3 v_wpos;
in vec4 v_flow;
in vec2 v_tan;
in float v_depth;
uniform float u_lake;
out vec4 fragColor;
float ripples(vec2 p, float t){
  return vnoise(p * 1.7) * 0.55 + vnoise(p * 4.1 + vec2(4.3, -t * 0.7)) * 0.45;
}
void main(){
  float t = u_clock * (1.0 - 0.75 * u_ice);
  float depth = clamp(v_flow.z, 0.0, 1.6);
  float rapid = v_flow.w;
  vec2 tg = normalize(v_tan + vec2(1e-5));
  vec2 nm = vec2(-tg.y, tg.x);
  vec2 fp = u_lake > 0.5 ? v_wpos.xz * vec2(0.35, 0.35) + vec2(t * 0.05, t * 0.03) : vec2(v_flow.x * 0.16 - t * (1.4 + rapid * 2.4), v_flow.y * 0.45);
  float n1 = vnoise(fp * 1.7);
  float n2 = vnoise(fp * 4.1 + vec2(4.3, -t * 0.7));
  float n3 = vnoise(vec2(fp.x * 2.6 - t * 1.2, fp.y * 2.9));
  float e = 0.06;
  float h0 = ripples(fp, t), hx = ripples(fp + vec2(e, 0.0), t), hy = ripples(fp + vec2(0.0, e), t);
  vec2 g = vec2(hx - h0, hy - h0) / e * (0.05 + rapid * 0.08 + u_lake * 0.02);
  vec3 wn = u_lake > 0.5 ? normalize(vec3(-g.x, 1.0, -g.y)) : normalize(vec3(-(g.x * tg.x + g.y * nm.x), 1.0, -(g.x * tg.y + g.y * nm.y)));
  vec3 V = normalize(u_camPos - v_wpos);
  float fres = 0.02 + 0.98 * pow(1.0 - max(dot(wn, V), 0.0), 5.0);
  vec3 shallow = vec3(0.30, 0.62, 0.62), deepc = vec3(0.05, 0.25, 0.36);
  vec3 col = mix(shallow, deepc, smoothstep(0.05, 0.9, depth));
  col = mix(col, vec3(0.22, 0.36, 0.30), smoothstep(0.0, 0.25, depth) * (1.0 - smoothstep(0.25, 0.6, depth)) * 0.35);
  vec3 R = reflect(-V, wn);
  vec3 sky = mix(u_fogCol.rgb, u_misc.rgb, pow(clamp(R.y, 0.0, 1.0), 0.55));
  col = mix(col, sky, clamp(fres, 0.0, 0.85));
  col += vec3(0.85, 0.95, 1.0) * smoothstep(0.62, 0.9, n1 * 0.55 + n2 * 0.45) * (0.16 + rapid * 0.2) * (1.0 - u_lake * 0.6);
  float bank = 1.0 - smoothstep(0.02, 0.18, depth);
  float foam = bank * smoothstep(0.35, 0.75, n3);
  foam = max(foam, rapid * smoothstep(0.45 - rapid * 0.25, 0.85, n1 * 0.5 + n3 * 0.5));
  col = mix(col, vec3(0.94, 0.97, 0.98), foam * 0.85);
  vec3 H = normalize(u_sunDir.xyz + V);
  float spec = pow(max(dot(wn, H), 0.0), 220.0) * 3.0 + pow(max(n2, 0.0), 9.0) * 0.6;
  col += u_sunCol.rgb * spec * 0.35 * u_daylight;
  float ice = u_ice * (0.55 + 0.45 * smoothstep(0.35, 0.0, depth)) * (0.85 + 0.15 * n1) * (1.0 - rapid * 0.7);
  col = mix(col, vec3(0.8, 0.88, 0.93), ice);
  float lightK = clamp(0.16 + 0.84 * u_daylight, 0.0, 1.0);
  col *= lightK;
  float alpha = mix(smoothstep(0.0, 0.05, depth) * 0.9, 0.97, max(ice, foam * 0.6));
  alpha = max(alpha, fres * 0.6 * smoothstep(0.0, 0.03, depth));
  col = mix(col, u_fogCol.rgb, fogObject(v_wpos, v_depth));
  fragColor = vec4(col, alpha);
}
)";

const char* FALL_VS = R"(
in vec3 a_pos;
in vec4 a_col;
out vec2 v_uv;
out vec3 v_wpos;
out float v_depth;
void main(){
  v_uv = a_col.xy;
  v_wpos = a_pos;
  vec4 vp = u_view * vec4(a_pos, 1.0);
  v_depth = -vp.z;
  gl_Position = u_proj * vp;
}
)";

const char* FALL_FS = R"(
in vec2 v_uv;
in vec3 v_wpos;
in float v_depth;
out vec4 fragColor;
void main(){
  float s = vnoise(vec2(v_uv.x * 18.0, v_uv.y * 3.0 + u_clock * 3.2));
  float s2 = vnoise(vec2(v_uv.x * 41.0 + 3.0, v_uv.y * 6.0 + u_clock * 5.1));
  vec3 col = mix(vec3(0.42, 0.74, 0.82), vec3(0.95, 0.98, 1.0), smoothstep(0.35, 0.8, s * 0.6 + s2 * 0.4));
  col = mix(col, vec3(1.0), smoothstep(0.75, 1.0, 1.0 - v_uv.y) * 0.8);
  float edge = smoothstep(0.0, 0.12, v_uv.x) * smoothstep(1.0, 0.88, v_uv.x);
  col *= clamp(0.18 + 0.82 * u_daylight, 0.0, 1.0);
  col = mix(col, u_fogCol.rgb, fogObject(v_wpos, v_depth));
  fragColor = vec4(col, (0.78 + 0.2 * s2) * edge * (1.0 - 0.4 * u_ice));
}
)";

const char* POOL_FS = R"(
in vec3 v_wpos;
in vec4 v_flow;
in vec2 v_tan;
in float v_depth;
uniform vec3 u_center;
out vec4 fragColor;
void main(){
  vec2 p = v_wpos.xz - u_center.xz;
  float r = length(p);
  float ring = vnoise(vec2(atan(p.y, p.x) * 4.0, r * 1.6 - u_clock * 1.8));
  float foam = smoothstep(0.35, 0.8, ring) * (1.0 - smoothstep(1.5, 5.5, r));
  vec3 col = vec3(0.97) * clamp(0.18 + 0.82 * u_daylight, 0.0, 1.0);
  col = mix(col, u_fogCol.rgb, fogObject(v_wpos, v_depth));
  fragColor = vec4(col, foam * (1.0 - smoothstep(4.5, 6.0, r)));
}
)";

const char* SPRAY_VS = R"(
in vec3 a_pos;
uniform vec3 u_origin;
uniform vec3 u_dir;
out float v_a;
void main(){
  vec3 sd = a_pos;
  float t = fract(sd.x + u_clock * (0.18 + sd.y * 0.22));
  float big = step(0.7, sd.z);
  vec3 side = normalize(cross(vec3(0.0, 1.0, 0.0), u_dir));
  vec3 p = u_origin + side * (sd.y - 0.5) * 11.0 * (0.4 + t) + vec3(0.0, t * mix(3.0, 8.0, big), 0.0) + u_dir * (2.0 + (sd.z - 0.5) * 6.0 + t * 2.0);
  v_a = (1.0 - t) * mix(0.55, 0.28, big) * clamp(0.25 + u_daylight, 0.0, 1.0);
  vec4 vp = u_view * vec4(p, 1.0);
  gl_Position = u_proj * vp;
  gl_PointSize = min(mix(120.0, 900.0, big) * (0.5 + t) / max(-vp.z, 0.5) * (u_viewH / 900.0), 256.0);
}
)";

const char* SPRAY_FS = R"(
in float v_a;
out vec4 fragColor;
void main(){
  float d = length(gl_PointCoord - 0.5) * 2.0;
  if (d > 1.0) discard;
  float a = (1.0 - d) * 0.75;
  fragColor = vec4(vec3(1.0), a * v_a);
}
)";

Mesh g_river, g_lakes, g_fall, g_pool;
GLuint g_sprayVao = 0, g_sprayVbo = 0;
Vec3 g_poolCenter, g_fallDir;

Mesh uploadW(const std::vector<WVert>& v, const std::vector<uint32_t>& idx) {
  MeshData d;
  d.fmt = VFmt::Pos3;
  d.vertexCount = (int)v.size();
  d.verts.resize(v.size() * sizeof(WVert));
  std::memcpy(d.verts.data(), v.data(), d.verts.size());
  d.idx = idx;
  Mesh m;
  glGenVertexArrays(1, &m.vao);
  glBindVertexArray(m.vao);
  glGenBuffers(1, &m.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)d.verts.size(), d.verts.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(WVert), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(WVert), (void*)12);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(WVert), (void*)28);
  glGenBuffers(1, &m.ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size() * 4), idx.data(), GL_STATIC_DRAW);
  m.indexType = GL_UNSIGNED_INT;
  m.count = (GLsizei)idx.size();
  glBindVertexArray(0);
  gfx::addBufferBytes((long long)(d.verts.size() + idx.size() * 4));
  return m;
}

void buildRiver() {
  const River& R = world::RIVER;
  std::vector<WVert> v;
  std::vector<uint32_t> idx;
  const int ACROSS = 12;
  size_t n = R.pts.size();
  int rows = 0;
  for (size_t i = 0; i < n; i++) {
    size_t a = i > 0 ? i - 1 : 0, b = std::min(n - 1, i + 1);
    float tx = R.pts[b][0] - R.pts[a][0], tz = R.pts[b][1] - R.pts[a][1];
    float l = hypot2(tx, tz);
    if (l < 1e-5f) continue;
    tx /= l; tz /= l;
    float nx = -tz, nz = tx;
    float hw = R.hw[i] + 3.0f;
    float wy = R.wy[i];
    for (int k = 0; k <= ACROSS; k++) {
      float u = -1 + 2.0f * k / ACROSS;
      float x = R.pts[i][0] + nx * u * hw, z = R.pts[i][1] + nz * u * hw;
      RiverHit q = world::riverQuery(x, z);
      float y = q.d < 1e8f ? q.wy : wy;
      float depth = y - world::getTerrainHeight(x, z);
      v.push_back({{x, y, z}, {R.cum[i], u * hw, depth, R.rapid[i]}, {tx, tz}});
    }
    if (rows > 0) {
      uint32_t r0 = (uint32_t)((rows - 1) * (ACROSS + 1)), r1 = (uint32_t)(rows * (ACROSS + 1));
      for (int k = 0; k < ACROSS; k++) idx.insert(idx.end(), {r0 + k, r0 + k + 1, r1 + k, r0 + k + 1, r1 + k + 1, r1 + k});
    }
    rows++;
  }
  g_river = uploadW(v, idx);
}

void buildLakes() {
  std::vector<WVert> v;
  std::vector<uint32_t> idx;
  for (const Lake& L : world::LAKES) {
    const int RINGS = 14, SEG = 48;
    uint32_t base = (uint32_t)v.size();
    float rmax = L.r + 6;
    for (int r = 0; r <= RINGS; r++) {
      float rr = rmax * r / RINGS;
      int segs = r == 0 ? 1 : SEG;
      for (int s = 0; s < segs; s++) {
        float a = (float)s / SEG * TAU;
        float x = L.x + std::cos(a) * rr, z = L.z + std::sin(a) * rr;
        float depth = L.level - world::getTerrainHeight(x, z);
        v.push_back({{x, L.level, z}, {0, 0, depth, 0}, {1, 0}});
      }
    }
    for (int s = 0; s < SEG; s++) idx.insert(idx.end(), {base, base + 1 + (uint32_t)((s + 1) % SEG), base + 1 + (uint32_t)s});
    for (int r = 1; r < RINGS; r++) {
      uint32_t r0 = base + 1 + (uint32_t)((r - 1) * SEG), r1 = base + 1 + (uint32_t)(r * SEG);
      for (int s = 0; s < SEG; s++) {
        uint32_t s1 = (uint32_t)((s + 1) % SEG);
        idx.insert(idx.end(), {r0 + s, r0 + s1, r1 + s, r0 + s1, r1 + s1, r1 + s});
      }
    }
  }
  g_lakes = uploadW(v, idx);
}

void buildFall() {
  const River& R = world::RIVER;
  float hx = R.head[0], hz = R.head[1];
  float dirx = 0.08f, dirz = -1.0f;
  float dl = hypot2(dirx, dirz);
  dirx /= dl; dirz /= dl;
  float sx = -dirz, sz = dirx;
  g_fallDir = {-dirx, 0, -dirz};
  float wy = R.wy[0];
  std::vector<WVert> v;
  std::vector<uint32_t> idx;
  const int ROWS = 28, COLS = 10;
  const float W = 4.6f;
  float topBehind = 14;
  for (int r = 0; r <= ROWS; r++) {
    float vv = (float)r / ROWS;
    float behind = -1.5f + vv * (topBehind + 1.5f);
    for (int c = 0; c <= COLS; c++) {
      float u = (float)c / COLS;
      float off = (u * 2 - 1) * W * (1 + (1 - vv) * 0.25f);
      float x = hx + dirx * behind + sx * off, z = hz + dirz * behind + sz * off;
      float h0 = world::getTerrainHeight(x, z);
      float slope = world::getTerrainHeight(x + dirx * 0.6f, z + dirz * 0.6f) - world::getTerrainHeight(x - dirx * 0.6f, z - dirz * 0.6f);
      float push = clampf((slope / 1.2f - 0.6f) * 0.9f, 0, 2.2f);
      for (float k = 0.4f; k <= 3.2f; k += 0.4f)
        if (world::getTerrainHeight(x - dirx * k, z - dirz * k) > h0 - 0.1f) push = std::max(push, k + 0.5f);
      x -= dirx * push;
      z -= dirz * push;
      float y = std::max(h0 + 0.35f + std::sin(off * 1.3f) * 0.1f, wy + 0.05f);
      v.push_back({{x, y, z}, {u, vv, 0, 0}, {0, 0}});
    }
    if (r > 0) {
      uint32_t r0 = (uint32_t)((r - 1) * (COLS + 1)), r1 = (uint32_t)(r * (COLS + 1));
      for (int c = 0; c < COLS; c++) idx.insert(idx.end(), {r0 + c, r0 + c + 1, r1 + c, r0 + c + 1, r1 + c + 1, r1 + c});
    }
  }
  g_fall = uploadW(v, idx);
  g_poolCenter = {hx - dirx * 2.6f, wy + 0.03f, hz - dirz * 2.6f};
  std::vector<WVert> pv;
  std::vector<uint32_t> pi;
  const int SEG = 32;
  pv.push_back({{g_poolCenter.x, g_poolCenter.y, g_poolCenter.z}, {0, 0, 1, 0}, {1, 0}});
  for (int s = 0; s < SEG; s++) {
    float a = (float)s / SEG * TAU;
    pv.push_back({{g_poolCenter.x + std::cos(a) * 6, g_poolCenter.y, g_poolCenter.z + std::sin(a) * 6}, {0, 0, 1, 0}, {1, 0}});
  }
  for (int s = 0; s < SEG; s++) pi.insert(pi.end(), {0u, 1u + (uint32_t)((s + 1) % SEG), 1u + (uint32_t)s});
  g_pool = uploadW(pv, pi);
  std::vector<float> seeds(70 * 3);
  Rng rng(9123);
  for (auto& s : seeds) s = rng.next();
  glGenVertexArrays(1, &g_sprayVao);
  glBindVertexArray(g_sprayVao);
  glGenBuffers(1, &g_sprayVbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_sprayVbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(seeds.size() * 4), seeds.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
  glBindVertexArray(0);
}

}  // namespace

namespace water {

void init() {
  buildRiver();
  buildLakes();
  buildFall();
}

void draw() {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  Shader& s = render::program("water", WATER_VS, WATER_FS);
  s.set1f("u_lake", 0);
  g_river.draw();
  s.set1f("u_lake", 1);
  g_lakes.draw();
  render::program("fall", FALL_VS, FALL_FS);
  g_fall.draw();
  Shader& p = render::program("pool", WATER_VS, POOL_FS);
  p.set3("u_center", g_poolCenter);
  g_pool.draw();
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void drawParticles() {
  if (render::cam.pos.distanceTo(g_poolCenter) > 400) return;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  Shader& s = render::program("spray", SPRAY_VS, SPRAY_FS);
  s.set3("u_origin", g_poolCenter);
  s.set3("u_dir", g_fallDir);
  glBindVertexArray(g_sprayVao);
  glDrawArrays(GL_POINTS, 0, 70);
  gfx::drawCalls++;
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void shutdown() {
  g_river.destroy();
  g_lakes.destroy();
  g_fall.destroy();
  g_pool.destroy();
  if (g_sprayVbo) glDeleteBuffers(1, &g_sprayVbo);
  if (g_sprayVao) glDeleteVertexArrays(1, &g_sprayVao);
}

}  // namespace water
