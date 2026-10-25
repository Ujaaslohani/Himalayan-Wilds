#include "sky.h"
#include "render.h"
#include "../game/engine.h"
#include "../game/season.h"
#include "../world/terrain.h"
#include "../world/forest.h"
#include "../platform/platform.h"
#include <vector>

namespace {

const char* SKY_VS = R"(
out vec3 v_dir;
uniform mat4 u_invVP;
void main(){
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
  vec4 w = u_invVP * vec4(p, 1.0, 1.0);
  v_dir = w.xyz / w.w - u_camPos;
  gl_Position = vec4(p, 1.0, 1.0);
}
)";

const char* SKY_FS = R"(
in vec3 v_dir;
uniform vec3 u_top;
uniform vec3 u_bottom;
uniform vec3 u_sun;
uniform vec3 u_moon;
uniform vec3 u_sunTint;
uniform float u_cover;
out vec4 fragColor;
void main(){
  vec3 d = normalize(v_dir);
  float h = max(d.y, 0.0);
  vec3 col = mix(u_bottom, u_top, pow(h, 0.55));
  float sd = max(dot(d, u_sun), 0.0);
  float hz = smoothstep(-0.02, 0.03, d.y);
  float day = u_daylight;
  col += u_sunTint * (pow(sd, 1400.0) * 0.6 + pow(sd, 90.0) * 0.35 + pow(sd, 10.0) * 0.2) * day;
  col = mix(col, vec3(1.0, 0.97, 0.88) * 1.6 + u_sunTint * 0.2, smoothstep(0.99955, 0.99972, sd) * hz);
  float md = dot(d, u_moon);
  float night = 1.0 - day;
  if (md > 0.995) {
    vec3 mt = normalize(cross(u_moon, vec3(0.0, 1.0, 0.0)) + vec3(0.0, 0.0, 0.001));
    vec3 mb = cross(mt, u_moon);
    vec2 mp = vec2(dot(d, mt), dot(d, mb)) * 60.0;
    float mare = vnoise(mp * 2.2 + 3.0) * 0.6 + vnoise(mp * 5.0) * 0.4;
    float disc = smoothstep(0.99962, 0.99976, md) * hz;
    float lit = smoothstep(-0.35, 0.25, mp.x + 0.25);
    vec3 mc = mix(vec3(0.62, 0.64, 0.68), vec3(0.95, 0.95, 0.9), smoothstep(0.35, 0.65, mare)) * mix(0.18, 1.0, lit);
    col = mix(col, mc * mix(0.75, 1.15, night), disc * mix(0.35, 1.0, night));
  }
  col += vec3(0.55, 0.65, 0.85) * (pow(max(md, 0.0), 400.0) * 0.22 + pow(max(md, 0.0), 40.0) * 0.05) * night * hz;
  if (d.y > 0.0) {
    vec2 uv = d.xz / (d.y + 0.15) * 1.35 + vec2(u_clock * 0.004, u_clock * 0.0017);
    float n = fbm4(uv);
    float c = smoothstep(0.62 - u_cover * 0.3, 0.9 - u_cover * 0.15, n);
    float n2 = fbm3(uv + u_sun.xz * 0.12);
    float lit = clamp(0.7 + (n - n2) * 3.0, 0.15, 1.25);
    float mglow = pow(max(md, 0.0), 6.0);
    vec3 dark = mix(vec3(0.025, 0.03, 0.05), vec3(0.52, 0.56, 0.64), day);
    vec3 bright = mix(vec3(0.07, 0.085, 0.12) + vec3(0.16, 0.18, 0.22) * mglow, mix(u_sunTint, vec3(1.0), 0.55), day);
    vec3 cc = mix(dark, bright, clamp(lit, 0.0, 1.0)) + u_sunTint * pow(sd, 6.0) * (1.0 - c) * 0.6 * day + vec3(0.35, 0.42, 0.55) * pow(max(md, 0.0), 160.0) * night * 0.35;
    float fade = smoothstep(0.0, 0.2, d.y);
    col = mix(col, cc, c * fade * 0.94);
  }
  fragColor = vec4(col, 1.0);
}
)";

const char* STAR_VS = R"(
in vec3 a_pos;
out float v_tw;
void main(){
  vec4 vp = u_view * vec4(u_camPos + a_pos * 110.0, 1.0);
  gl_Position = u_proj * vp;
  gl_Position.z = gl_Position.w;
  v_tw = 0.7 + 0.3 * sin(u_clock * 2.0 + a_pos.x * 50.0);
  gl_PointSize = 2.0 * (u_viewH / 900.0) + 0.5;
}
)";
const char* STAR_FS = R"(
in float v_tw;
uniform float u_op;
out vec4 fragColor;
void main(){ fragColor = vec4(0.973, 0.98, 0.988, u_op * v_tw); }
)";

const char* MIST_VS = R"(
in vec2 a_pos;
in vec4 a_i0;
out vec2 v_uv;
out float v_seed;
out float v_fade;
void main(){
  v_uv = a_pos;
  v_seed = fract(a_i0.x * 0.013 + a_i0.z * 0.007);
  vec3 c = a_i0.xyz + vec3(sin(u_clock * 0.03 + v_seed * 6.0) * 8.0, 0.0, cos(u_clock * 0.025 + v_seed * 5.0) * 8.0);
  vec4 mv = u_view * vec4(c, 1.0);
  mv.xy += a_pos * vec2(a_i0.w, a_i0.w * 0.42);
  float dist = -mv.z;
  v_fade = smoothstep(30.0, 120.0, dist) * (1.0 - smoothstep(1400.0, 2200.0, dist));
  gl_Position = u_proj * mv;
}
)";
const char* MIST_FS = R"(
in vec2 v_uv;
in float v_seed;
in float v_fade;
uniform float u_light;
uniform vec3 u_tint;
out vec4 fragColor;
void main(){
  float r = length(v_uv * vec2(1.0, 1.3));
  float n = fbm3(v_uv * 1.6 + vec2(v_seed * 17.0 + u_clock * 0.01, v_seed * 9.0));
  float day = clamp((u_light - 0.22) / 0.78, 0.0, 1.0);
  float a = smoothstep(1.0, 0.3, r) * smoothstep(0.5, 0.82, n) * v_fade * (0.06 + 0.44 * day);
  if (a < 0.01) discard;
  vec3 col = mix(u_tint, mix(u_tint * 0.8, vec3(1.0), 0.55 + 0.45 * v_uv.y), day) * mix(0.6, 1.0, day);
  fragColor = vec4(col, a);
}
)";

const char* SNOW_VS = R"(
in vec3 a_pos;
uniform float u_fallAmt;
out float v_a;
void main(){
  vec3 box = vec3(40.0, 20.0, 40.0);
  vec3 sd = a_pos;
  vec3 p = sd * box;
  p.y -= u_clock * (1.0 + sd.x * 0.7);
  p.x += sin(u_clock * 0.6 + sd.z * 25.0) * 0.8;
  p.z += cos(u_clock * 0.5 + sd.y * 19.0) * 0.6;
  vec3 rel = mod(p - u_camPos + box * 0.5, box) - box * 0.5;
  vec4 mv = u_view * vec4(u_camPos + rel, 1.0);
  gl_Position = u_proj * mv;
  float on = step(sd.y, u_fallAmt);
  v_a = on * (1.0 - smoothstep(14.0, 20.0, length(rel)));
  gl_PointSize = on * clamp((0.05 + sd.z * 0.05) * u_viewH / max(-mv.z, 0.1), 1.0, 14.0);
}
)";
const char* SNOW_FS = R"(
in float v_a;
out vec4 fragColor;
void main(){
  float d = length(gl_PointCoord - 0.5) * 2.0;
  if (d > 1.0) discard;
  fragColor = vec4(vec3(0.95, 0.97, 1.0) * (0.35 + 0.65 * u_daylight), v_a * (1.0 - d * d) * 0.9);
}
)";

const char* LEAF_VS = R"(
in vec3 a_pos;
in vec4 a_col;
uniform float u_leafAmt;
out float v_a;
out float v_rot;
out vec3 v_seed;
void main(){
  vec3 sd = a_pos;
  vec4 em = a_col;
  float t = fract(sd.x + u_clock * (0.045 + sd.y * 0.04));
  float ang = sd.z * 6.2832;
  float rad = 0.6 + sd.y * 4.6;
  float fall = min(1.0, t / 0.78);
  vec3 p = vec3(em.x + cos(ang) * rad, mix(em.y, em.w + 0.06, fall), em.z + sin(ang) * rad);
  float ft = min(t, 0.78) * 30.0;
  p.x += sin(ft * 1.3 + sd.z * 30.0) * 0.6 * fall;
  p.z += cos(ft * 1.1 + sd.x * 20.0) * 0.6 * fall;
  vec4 mv = u_view * vec4(p, 1.0);
  gl_Position = u_proj * mv;
  float on = step(sd.y, u_leafAmt) * step(0.5, em.y - em.w);
  v_a = on * smoothstep(0.0, 0.06, t) * (1.0 - smoothstep(0.88, 1.0, t));
  v_rot = ft * (1.5 + sd.y * 3.0) + sd.z * 10.0;
  v_seed = sd;
  gl_PointSize = on * clamp((0.09 + sd.x * 0.05) * u_viewH / max(-mv.z, 0.1), 1.5, 16.0);
}
)";
const char* LEAF_FS = R"(
in float v_a;
in float v_rot;
in vec3 v_seed;
uniform float u_petal;
out vec4 fragColor;
void main(){
  vec2 q = gl_PointCoord - 0.5;
  float c = cos(v_rot), s = sin(v_rot);
  q = vec2(c * q.x - s * q.y, s * q.x + c * q.y);
  float flip = 0.35 + 0.65 * abs(sin(v_rot * 0.7));
  q.x /= flip;
  float d = length(q * vec2(1.0, 1.9)) * 2.0;
  if (d > 1.0 || v_a < 0.01) discard;
  vec3 au = mix(vec3(0.95, 0.38, 0.05), vec3(0.7, 0.1, 0.04), smoothstep(0.3, 0.7, v_seed.z));
  au = mix(au, vec3(0.98, 0.7, 0.1), step(0.8, v_seed.x));
  vec3 pe = mix(vec3(1.0, 0.75, 0.85), vec3(1.0, 0.95, 0.97), v_seed.z);
  vec3 col = mix(au, pe, u_petal) * (0.75 + 0.25 * flip) * (0.3 + 0.7 * u_daylight);
  fragColor = vec4(col, v_a);
}
)";

struct SkyCols { Color zen, hor; };
const Color NIGHT = Color::hex("0b1430"), NIGHTH = Color::hex("1c2846"), DAWN = Color::hex("f08a4b"), DAWNH = Color::hex("fbc99a");
const Color DAY = Color::hex("78bdf0"), DAYH = Color::hex("d3e8ee"), GOLD = Color::hex("f6a35c"), GOLDH = Color::hex("fde3a7");
const Color SUN = Color::hex("fff1d6"), SUNLOW = Color::hex("ffc58a"), MOON = Color::hex("9db4e0");
const Color WINTER_H = Color::hex("e4eaf0"), WINTER_Z = Color::hex("9fb6cc"), AUTUMN_H = Color::hex("f3d9b5");
const Color G_SUMMER = Color::hex("7a9a45"), G_AUTUMN = Color::hex("9a7038"), G_WINTER = Color::hex("c9d3dc"), HEMI_SKY = Color::hex("d6ecff");
const float SHADOW_HALF = 32;

Color g_zen, g_hor, g_sunTint;
Vec3 g_sunDir, g_moonDir, g_lightDir;
Mat4 g_shadowMat;
Vec3 g_shadowC, g_lastShadowC{1e9f, 0, 0};
int g_shadowTick = 0;
bool g_shadowUpdate = true, g_staticDirty = true;
float g_cover = 0.4f;
GLuint g_starVao = 0, g_starVbo = 0, g_mistVao = 0, g_mistIbo = 0, g_snowVao = 0, g_snowVbo = 0, g_leafVao = 0, g_leafSeedVbo = 0, g_leafEmitVbo = 0;
int g_mistCount = 0;
const int LEAF_EMIT = 40, LEAF_PER = 32;
std::vector<float> g_leafEmit;
float g_leafT = 99;
std::vector<float> g_near;

SkyCols skyColors(float h) {
  struct Key { float t; const char *zen, *hor; };
  static const Key K[] = {
      {0.0f, "#010103", "#030407"},   {3.6f, "#010103", "#030407"},   {4.6f, "#040820", "#081028"},
      {5.5f, "#0b1a48", "#1a2c5c"},   {6.4f, "#21499a", "#5a7fbc"},   {7.6f, "#78bdf0", "#d3e8ee"},
      {16.6f, "#78bdf0", "#d3e8ee"},  {17.6f, "#6a80b0", "#f6b465"},  {18.4f, "#5a4a80", "#f08a3c"},
      {19.2f, "#3a2462", "#8e4a86"},  {20.1f, "#101a4a", "#24305e"},  {21.2f, "#03040e", "#070a16"},
      {24.0f, "#010103", "#030407"},
  };
  const int N = sizeof(K) / sizeof(K[0]);
  h = std::fmod(h + 24.0f, 24.0f);
  int i = 0;
  while (i < N - 2 && h >= K[i + 1].t) i++;
  float u = (h - K[i].t) / std::max(1e-3f, K[i + 1].t - K[i].t);
  u = u * u * (3 - 2 * u);
  return {Color::hex(K[i].zen).lerp(Color::hex(K[i + 1].zen), u), Color::hex(K[i].hor).lerp(Color::hex(K[i + 1].hor), u)};
}

void setv(float* d, const Color& c, float w) { d[0] = c.r; d[1] = c.g; d[2] = c.b; d[3] = w; }
void setv(float* d, const Vec3& c, float w) { d[0] = c.x; d[1] = c.y; d[2] = c.z; d[3] = w; }

}  // namespace

namespace sky {

void init() {
  Rng rng(777);
  std::vector<float> stars;
  for (int i = 0; i < 260; i++) {
    float th = rng.next() * TAU, ph = std::acos(rng.next() * 0.85f + 0.15f);
    stars.insert(stars.end(), {std::sin(ph) * std::cos(th), std::cos(ph), std::sin(ph) * std::sin(th)});
  }
  glGenVertexArrays(1, &g_starVao);
  glBindVertexArray(g_starVao);
  glGenBuffers(1, &g_starVbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_starVbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(stars.size() * 4), stars.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);

  std::vector<float> spots;
  auto add = [&](float x, float y, float z, float s) { spots.insert(spots.end(), {x, y, z, s}); };
  for (size_t i = 0; i < world::SUMMITS.size(); i++) {
    const Summit& sm = world::SUMMITS[i];
    for (int k = 0; k < 5; k++) {
      float a = k * 1.3f + i * 2.1f;
      float r = 72.0f + (k % 3) * 20;
      add(sm.x + std::cos(a) * r, sm.h * (0.36f + 0.06f * (k % 4)), sm.z + std::sin(a) * r, 64.0f + (k % 2) * 28);
    }
  }
  for (int k = 0; k < 24; k++) {
    float a = (k / 24.0f) * TAU + 0.2f;
    float r = MAP_HALF - 10 + (k % 3) * 40;
    add(std::cos(a) * r, 140.0f + (k % 4) * 26, std::sin(a) * r, 140.0f + (k % 3) * 50);
  }
  g_mistCount = (int)(spots.size() / 4);
  glGenVertexArrays(1, &g_mistVao);
  glBindVertexArray(g_mistVao);
  glBindBuffer(GL_ARRAY_BUFFER, render::quadVaoBuffer());
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
  glGenBuffers(1, &g_mistIbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_mistIbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(spots.size() * 4), spots.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(6);
  glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 16, nullptr);
  glVertexAttribDivisor(6, 1);

  std::vector<float> seeds(1600 * 3);
  for (auto& s : seeds) s = rng.next();
  glGenVertexArrays(1, &g_snowVao);
  glBindVertexArray(g_snowVao);
  glGenBuffers(1, &g_snowVbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_snowVbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(seeds.size() * 4), seeds.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);

  int N = LEAF_EMIT * LEAF_PER;
  std::vector<float> lseed(N * 3);
  for (auto& s : lseed) s = rng.next();
  g_leafEmit.assign(N * 4, 0);
  glGenVertexArrays(1, &g_leafVao);
  glBindVertexArray(g_leafVao);
  glGenBuffers(1, &g_leafSeedVbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_leafSeedVbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(lseed.size() * 4), lseed.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
  glGenBuffers(1, &g_leafEmitVbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_leafEmitVbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(g_leafEmit.size() * 4), g_leafEmit.data(), GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 16, nullptr);
  glBindVertexArray(0);
}

void update(float dt) {
  (void)dt;
  float h = E.time;
  float ang = ((h - 6) / 24) * TAU;
  float elev = std::sin(ang);
  float daylight = sstep(elev, -0.08f, 0.3f);
  E.isNight = daylight < 0.15f;
  E.daylight = daylight;
  SkyCols sc = skyColors(h);
  updateSeason(h);
  float snow = season.snow, au = season.autumn;
  g_hor = sc.hor.lerp(WINTER_H, snow * 0.45f * daylight).lerp(AUTUMN_H, au * 0.18f * daylight);
  g_zen = sc.zen.lerp(WINTER_Z, snow * 0.35f * daylight);
  g_sunDir = Vec3(std::cos(ang), elev, -0.32f).normalized();
  g_moonDir = Vec3(-std::cos(ang) * 0.92f, -elev, 0.42f).normalized();
  float moonUp = sstep(g_moonDir.y, -0.04f, 0.22f);
  float sunI = 2.1f * std::pow(daylight, 1.15f) * (1 - 0.18f * snow);
  float moonI = 0.32f * moonUp * (1 - daylight);
  bool useSun = sunI >= moonI;
  g_lightDir = useSun ? g_sunDir : g_moonDir;
  g_lightDir.y = std::max(g_lightDir.y, 0.1f);
  g_lightDir.normalize();
  g_sunTint = daylight > 0.55f ? SUN : SUNLOW;
  Color lightCol = useSun ? g_sunTint : MOON;
  float lightI = std::max(std::max(sunI, moonI), 0.05f);
  float hemiI = lerpf(0.32f, 1.05f, daylight);
  Color ground = G_SUMMER.lerp(G_AUTUMN, au * 0.8f).lerp(G_WINTER, snow);
  g_cover = 0.5f + 0.3f * season.fall + 0.1f * season.autumn;

  const Vec3& p = E.player.pos;
  float texel = (SHADOW_HALF * 2) / (float)g_settings.shadowSize;
  float step = texel * std::max(1.0f, std::round(6.0f / texel));
  Vec3 c(std::round(p.x / step) * step, std::round(p.y / step) * step, std::round(p.z / step) * step);
  static Vec3 s_light{0, 1, 0};
  static int s_size = 0;
  bool moved = c.distanceSq(g_shadowC) > 0.01f || s_size != g_settings.shadowSize;
  bool turned = g_lightDir.dot(s_light) < 0.99999f;
  if (moved || turned || g_staticDirty) {
    g_shadowC = c;
    s_light = g_lightDir;
    s_size = g_settings.shadowSize;
    Vec3 lp = g_shadowC + s_light * 150;
    Mat4 lv = Mat4::lookAt(lp, g_shadowC, std::fabs(s_light.y) > 0.99f ? Vec3(0, 0, 1) : Vec3(0, 1, 0));
    Mat4 lpj = Mat4::ortho(-SHADOW_HALF, SHADOW_HALF, -SHADOW_HALF, SHADOW_HALF, 1, 320);
    g_shadowMat = lpj * lv;
    g_staticDirty = true;
  }
  g_shadowUpdate = g_staticDirty || (g_shadowTick++ % 2) == 0;

  FrameUBO& F = render::frame;
  std::memcpy(F.view, render::cam.view.m, 64);
  std::memcpy(F.proj, render::cam.proj.m, 64);
  std::memcpy(F.viewProj, render::cam.viewProj.m, 64);
  std::memcpy(F.shadowMat, g_shadowMat.m, 64);
  setv(F.cam, render::cam.pos, E.clock);
  float strength = useSun ? std::min(1.0f, sunI / 1.3f) : 0.45f * moonUp;
  setv(F.sunDir, g_lightDir, strength);
  setv(F.sunCol, lightCol * lightI, daylight);
  setv(F.hemiSky, HEMI_SKY * hemiI, 1);
  setv(F.hemiGround, ground * hemiI, season.fall);
  setv(F.fogCol, g_hor, (60 - 25 * season.fall) * g_settings.drawDist());
  F.fogParam[0] = (260 - 100 * season.fall) * g_settings.drawDist();
  F.fogParam[1] = (float)platform::height();
  F.fogParam[2] = clampf((0.45f - daylight) / 0.35f, 0, 1);
  F.fogParam[3] = 0;
  F.season[0] = season.snow; F.season[1] = season.autumn; F.season[2] = season.bare; F.season[3] = season.spring;
  F.season2[0] = season.dry; F.season2[1] = season.ice; F.season2[2] = wind.t; F.season2[3] = wind.strength;
  F.wind[0] = wind.dirX; F.wind[1] = wind.dirZ; F.wind[2] = 1.0f / (float)g_settings.shadowSize; F.wind[3] = SHADOW_HALF;
  setv(F.misc, g_zen, g_cover);
}

Vec3 lightDir() { return g_lightDir; }
Mat4 shadowMatrix() { return g_shadowMat; }
Vec3 sunDir() { return g_sunDir; }
bool shadowNeedsUpdate() { return g_shadowUpdate; }
bool shadowStaticDirty() { return g_staticDirty; }
void clearStaticShadow() { g_staticDirty = false; }
void invalidateStaticShadow() { g_staticDirty = true; }
Color horizon() { return g_hor; }

void drawSky() {
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);
  Shader& s = render::program("sky", SKY_VS, SKY_FS);
  Mat4 inv = render::cam.viewProj.inverse();
  s.setMat4("u_invVP", inv);
  s.set3c("u_top", g_zen);
  s.set3c("u_bottom", g_hor);
  s.set3("u_sun", g_sunDir);
  s.set3("u_moon", g_moonDir);
  s.set3c("u_sunTint", g_sunTint);
  s.set1f("u_cover", g_cover);
  render::drawFullscreen();
  float op = 1 - E.daylight;
  if (op > 0.01f) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);
    Shader& st = render::program("stars", STAR_VS, STAR_FS);
    st.set1f("u_op", op);
    glBindVertexArray(g_starVao);
    glDrawArrays(GL_POINTS, 0, 260);
    gfx::drawCalls++;
    glDisable(GL_BLEND);
  }
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
}

void drawStarsAndMist() {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  Shader& m = render::program("mist", MIST_VS, MIST_FS);
  m.set1f("u_light", 0.22f + 0.78f * E.daylight);
  m.set3c("u_tint", g_hor);
  glBindVertexArray(g_mistVao);
  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, g_mistCount);
  gfx::drawCalls++;
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void drawWeather(float dt) {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  if (season.fall > 0.01f) {
    Shader& s = render::program("snow", SNOW_VS, SNOW_FS);
    s.set1f("u_fallAmt", season.fall);
    glBindVertexArray(g_snowVao);
    glDrawArrays(GL_POINTS, 0, 1600);
    gfx::drawCalls++;
  }
  if (season.leaf > 0.02f) {
    g_leafT += dt;
    if (g_leafT >= 1) {
      g_leafT = 0;
      forest::nearTrees(render::cam.pos.x, render::cam.pos.z, 30, g_near, LEAF_EMIT);
      int n = (int)(g_near.size() / 4);
      bool changed = false;
      for (int e = 0; e < LEAF_EMIT; e++) {
        bool has = e < n;
        float x = has ? g_near[e * 4] : 0, z = has ? g_near[e * 4 + 2] : 0, sc = has ? g_near[e * 4 + 3] : 0, gy = has ? g_near[e * 4 + 1] : 0;
        for (int k = 0; k < LEAF_PER; k++) {
          size_t o = (size_t)(e * LEAF_PER + k) * 4;
          if (g_leafEmit[o] == x && g_leafEmit[o + 2] == z) continue;
          changed = true;
          g_leafEmit[o] = x;
          g_leafEmit[o + 1] = has ? gy + (3 + std::fmod(k * 0.37f, 1.0f) * 4) * sc : 0;
          g_leafEmit[o + 2] = z;
          g_leafEmit[o + 3] = has ? world::getTerrainHeight(x, z) : 0;
        }
      }
      if (changed) {
        glBindBuffer(GL_ARRAY_BUFFER, g_leafEmitVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(g_leafEmit.size() * 4), g_leafEmit.data());
      }
    }
    Shader& s = render::program("leaves", LEAF_VS, LEAF_FS);
    s.set1f("u_leafAmt", std::min(1.0f, season.leaf));
    s.set1f("u_petal", season.spring > season.autumn ? 1.0f : 0.0f);
    glBindVertexArray(g_leafVao);
    glDrawArrays(GL_POINTS, 0, LEAF_EMIT * LEAF_PER);
    gfx::drawCalls++;
  }
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void shutdown() {
  GLuint bufs[] = {g_starVbo, g_mistIbo, g_snowVbo, g_leafSeedVbo, g_leafEmitVbo};
  glDeleteBuffers(5, bufs);
  GLuint vaos[] = {g_starVao, g_mistVao, g_snowVao, g_leafVao};
  glDeleteVertexArrays(4, vaos);
}

}  // namespace sky
