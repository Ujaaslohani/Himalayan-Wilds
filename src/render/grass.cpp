#include <cstdlib>
#include <cstdio>
#include "../assets/assets.h"
#include "envmap.h"
#include "sky.h"
#include <atomic>
#include <thread>
#include <unordered_set>
#include <deque>
#include <condition_variable>
#include <mutex>
#include "../game/wildlife.h"
#include "../game/village.h"
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
uniform vec4 u_tramp[8];
out vec3 v_col;
out vec3 v_uv;   // grass atlas uv (+ kind)
out vec4 v_fog;  // fog colour, amount (applied after the texture)
uniform float u_texOn;
uniform sampler2D u_waterLv;  // envmap water surface map (unit 21): r = surface height, g = lake/river mask
void kill(){ gl_Position = vec4(2.0, 2.0, 2.0, 1.0); v_col = vec3(0.0); v_uv = vec3(0.0); v_fog = vec4(0.0); }
void main(){
  vec4 h = a_b1;
  vec2 p = a_b0.xz;
  float d = distance(p, u_camPos.xz);
  float fade = u_ring.w;
  if (d > u_ring.z || d < u_ring.y - fade) { kill(); return; }
  if (h.z > a_b0.w * u_blade.z) { kill(); return; }
  // ring edges thin out blade by blade (dithered) instead of shrinking them, so nothing visibly grows as you walk
  float ringK = (1.0 - smoothstep(u_ring.z - fade, u_ring.z, d)) * (u_ring.y > 0.0 ? smoothstep(u_ring.y - fade, u_ring.y, d) : 1.0);
  if (fract(h.x * 17.31 + h.z * 5.77 + h.w * 3.3) > ringK) { kill(); return; }
  vec2 wv = textureLod(u_waterLv, envUV(p), 0.0).rg;
  if (wv.g > 0.3 && wv.r > a_b0.y - 0.05) { kill(); return; }  // nothing grows under lake or river water
  float grow = 1.0 - 0.65 * u_snow;
  // bare-earth patches (same pattern as the baked ground): grass thins out so real soil shows through
  float e1 = textureLod(u_detailMap, envUV(p), 0.0).a;  // precomputed bare-earth noise (envmap detail map), 1 under buildings
  grow *= 1.0 - 0.95 * smoothstep(0.58, 0.7, e1);
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
  // textured grass (Foliage001 atlas): the nearest ring draws single real blades, the outer rings draw whole
  // clump cards that turn to face the camera, so one far instance reads as a tuft instead of a sliver
  // Poly Haven grass_bermuda_01 cards (4x4 atlas of whole plants): every ring draws camera-facing plant cards,
  // so one 4-triangle card shows a full photo plant instead of one blade
  bool tblade = false;
  bool card = u_texOn > 0.5;
  if (tblade) W = H * 0.125 * 1.7 * (0.4 + 0.6 * grow) + 0.004;  // keep the atlas slot's 1:8 shape (slightly widened so blades read at distance)
  if (card) {
    vec2 vd = normalize(p - u_camPos.xz + vec2(1e-4));
    side = vec2(-vd.y, vd.x);
    H *= 1.1 * mix(0.8, 1.2, h.z);
    W = H;  // square atlas cells
  }
  float taper = u_texOn > 0.5 ? 0.0 : 0.85;
  float t = a_pos.y;
  float gust = u_windS * (0.6 + 0.4 * sin(u_windT * 1.1 + p.x * 0.05 + p.y * 0.04));
  // gust waves rolling across the meadow along the wind
  float wave = sin(dot(p, u_windDir) * 0.085 - u_windT * 1.8 + vnoise(p * 0.02) * 3.0);
  wave = max(wave, 0.0); wave *= wave;
  gust *= 0.55 + 1.1 * wave;
  float sway = (0.35 + 0.65 * sin(u_windT * 2.3 + p.x * 0.35 + p.y * 0.27 + h.w * 6.0)) * gust;
  vec2 lean = face * (0.15 + 0.25 * fract(h.y * 9.1)) + u_windDir * sway * 0.55;
  // bend away from the player
  vec2 away = p - u_camPos.xz;
  float pd = length(away);
  if (pd < 1.3 && abs(u_camPos.y - 1.7 - base) < 1.5) lean += away / max(pd, 0.05) * (1.0 - pd / 1.3) * 1.4;
  // flattened where animals and villagers walk
  for (int i = 0; i < 8; i++) {
    if (u_tramp[i].z <= 0.0) break;
    vec2 aw = p - u_tramp[i].xy;
    float ad = length(aw);
    if (ad < u_tramp[i].z) lean += aw / max(ad, 0.05) * (1.0 - ad / u_tramp[i].z) * 1.6;
  }
  vec2 xz = p + side * a_pos.x * W * (1.0 - t * taper) + lean * H * t * t;
  float y = base + H * t * (1.0 - 0.18 * t * length(lean));
  vec3 wpos = vec3(xz.x, y, xz.y);
  vec4 vp = u_view * vec4(wpos, 1.0);
  vec2 g = (p - u_grid.xx) / u_grid.y;
  vec4 bk = textureLod(u_bake, g / (u_grid.z - 1.0), 0.0);
  vec3 gc = bk.rgb * bk.rgb;
  vec3 tip = gc * vec3(1.35, 1.3, 0.95) + vec3(0.02, 0.025, 0.0);
  tip = mix(tip, vec3(0.62, 0.5, 0.22), 0.35 * u_dry + 0.25 * u_autumn);
  tip = mix(tip, vec3(0.85, 0.88, 0.92), u_snow * 0.7);
  // per-clump hue and dry tips
  float clump = vnoise(p * 0.18);
  tip = mix(tip, tip * vec3(1.12, 0.96, 0.78), smoothstep(0.55, 0.8, clump));
  float dryTip = step(0.82, fract(h.x * 13.1 + h.y * 5.3)) * smoothstep(0.7, 1.0, t);
  tip = mix(tip, vec3(0.6, 0.52, 0.28) * (gc.g * 4.0 + 0.3), dryTip * 0.6);
  vec3 alb = mix(gc * 0.62, tip, smoothstep(0.0, 1.0, t)) * (0.85 + 0.3 * h.z) * mix(0.9, 1.08, clump);
  alb *= 1.0 + 0.16 * wave * t * u_windS;  // silvery sheen on the bent crest of each wave
  alb = mix(gc * 0.9, alb, smoothstep(0.0, 0.6, ringK));  // fade into the ground colour at the ring edge
  vec3 n = normalize(vec3(0.0, 1.0, 0.0) + (u_sunDir.xyz * vec3(1.0, 0.0, 1.0)) * 0.35);
  float sh = u_blade.w > 1.5 ? envLight(vec3(p.x, base, p.y)) : shadowAt(t > 0.5 ? wpos : vec3(p.x, base + 0.3, p.y), vec3(0.0, 1.0, 0.0));  // outer rings skip the shadow map
  vec3 c = outColor(lambert(alb, n, sh) * (0.7 + 0.3 * t));
  v_col = c;
  v_fog = vec4(fogTint(wpos), fogGround(wpos, -vp.z));
  if (tblade) v_uv = vec3((floor(h.x * 8.0) + a_pos.x + 0.5) / 16.0, 1.0 - t, 0.0);
  else if (card) { float k = floor(h.y * 15.999); v_uv = vec3((mod(k, 4.0) + a_pos.x + 0.5) * 0.25, (floor(k / 4.0) + 1.0 - t) * 0.25, 1.0); }
  else v_uv = vec3(0.0);
  gl_Position = u_proj * vp;
}
)";

const char* GRASS_FS = R"(
in vec3 v_col;
in vec3 v_uv;
in vec4 v_fog;
uniform sampler2D u_grassTex;
uniform float u_texOn;
uniform vec3 u_texAvg;
out vec4 fragColor;
void main(){
  vec3 c = v_col;
  if (u_texOn > 0.5) {
    vec4 tx = texture(u_grassTex, v_uv.xy);
    // sharpened alpha test: thin blades stay solid in the mip levels instead of dissolving at distance
    float a = (tx.a - 0.4) / max(fwidth(tx.a), 1e-4) + 0.5;
    if (a < 0.5) discard;
    c *= clamp(tx.rgb / u_texAvg, 0.35, 1.9);  // real blade streaks and brown tips on top of the season colour
  }
  fragColor = vec4(mix(c, v_fog.rgb, v_fog.a), 1.0);
}
)";

const char* LEAF_VS = R"(
layout(location=0) in vec2 a_pos;
uniform sampler2D u_height;
uniform sampler2D u_density;
uniform vec4 u_grid;
uniform vec4 u_leaf;
uniform int u_n;
uniform sampler2D u_waterLv;
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
  vec2 wv = textureLod(u_waterLv, (p + 620.0) * (1.0 / 1240.0), 0.0).rg;
  if (wv.g > 0.3 && wv.r > heightAt(g) - 0.1) { kill(); return; }  // no leaves lying under lake or river water
  {  // only on fairly level ground: on steep slopes and cliffs a flat leaf reads as a floating flake from below
    float h0 = heightAt(g), hx = heightAt(g + vec2(0.5, 0.0)), hz = heightAt(g + vec2(0.0, 0.5));
    float grad = length(vec2(hx - h0, hz - h0)) / (0.5 * u_grid.y);
    if (grad > 0.55) { kill(); return; }
  }
  float ang = h.w * 6.2831853;
  vec2 ax = vec2(cos(ang), sin(ang)), az = vec2(-ax.y, ax.x);
  float L = mix(0.08, 0.15, fract(h.x * 5.3)) * (1.0 + d * 0.012);
  vec2 xz = p + ax * a_pos.x * L * 0.55 + az * a_pos.y * L;
  vec2 gg = (xz - u_grid.xx) / u_grid.y;
  float y = heightAt(gg) + 0.012 + 0.01 * fract(h.y * 7.1) + 0.012 * a_pos.x * a_pos.x;  // curled edges
  vec4 cc = u_viewProj * vec4(xz.x, y, xz.y, 1.0);
  v_wpos = vec3(xz.x, y, xz.y);
  vec4 vp = u_view * vec4(v_wpos, 1.0);
  v_depth = -vp.z;
  v_uv = a_pos;
  v_h = h;
  gl_Position = u_proj * vp;
}
)";

// spring wildflowers: a camera-centred grid of tiny camera-facing cards (one per 0.35 m cell, almost all killed in
// the vertex shader), each a stem and a 4-8 petal head drawn in the fragment shader. Sparse across all meadows,
// dense inside the flower fields; every colour.
const char* FLOWER_VS = R"(
layout(location=0) in vec2 a_pos;
uniform sampler2D u_height;
uniform sampler2D u_density;
uniform sampler2D u_waterLv;
uniform vec4 u_grid;
uniform vec4 u_fl;        // spacing, radius, meadow density, field density
uniform vec4 u_fields[4]; // x, z, radius
uniform int u_n;
out vec3 v_wpos;
out vec2 v_q;
out vec4 v_h;
out float v_top;
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
void kill(){ gl_Position = vec4(2.0, 2.0, 2.0, 1.0); v_wpos = vec3(0.0); v_q = vec2(0.0); v_h = vec4(0.0); v_top = 0.0; }
void main(){
  float s = u_fl.x;
  int ix = gl_InstanceID % u_n, iz = gl_InstanceID / u_n;
  vec2 origin = floor(u_camPos.xz / s) * s - float(u_n / 2) * s;
  vec2 cellId = floor(origin / s) + vec2(ix, iz);
  vec4 h = hash4(cellId + 17.3);
  vec2 p = (cellId + h.xy) * s;
  float d = distance(p, u_camPos.xz);
  if (d > u_fl.y) { kill(); return; }
  float field = 0.0;
  for (int i = 0; i < 4; i++) {
    if (u_fields[i].z <= 0.0) continue;
    float fd = distance(p, u_fields[i].xy) + (vnoise(p * 0.08) - 0.5) * u_fields[i].z * 0.5;
    field = max(field, 1.0 - smoothstep(u_fields[i].z * 0.7, u_fields[i].z, fd));
  }
  vec2 g = (p - u_grid.xx) / u_grid.y;
  float dens = texture(u_density, (g + 0.5) / u_grid.z).r;
  float want = dens * (u_fl.z * (0.4 + 1.2 * vnoise(p * 0.15)) + u_fl.w * field);
  if (h.z > want) { kill(); return; }
  if (fract(h.w * 13.7) > 1.0 - smoothstep(u_fl.y * 0.6, u_fl.y, d)) { kill(); return; }
  vec2 wv = textureLod(u_waterLv, (p + 620.0) * (1.0 / 1240.0), 0.0).rg;
  float y0 = heightAt(g);
  if (wv.g > 0.3 && wv.r > y0 - 0.1) { kill(); return; }
  vec4 cc = u_viewProj * vec4(p.x, y0 + 0.2, p.y, 1.0);
  if (cc.w < -0.5 || abs(cc.x) > cc.w * 1.1 + 0.5) { kill(); return; }
  float W = mix(0.07, 0.12, fract(h.x * 9.1)), H = mix(0.18, 0.42, fract(h.y * 5.3));
  vec2 vd = normalize(p - u_camPos.xz + vec2(1e-4));
  vec2 side = vec2(-vd.y, vd.x);
  float t = a_pos.y * 0.5 + 0.5;
  float sway = sin(u_windT * 2.1 + p.x * 0.7 + p.y * 0.5) * u_windS * 0.04 * t * t;
  vec2 xz = p + side * a_pos.x * W * 0.5 + u_windDir * sway;
  v_wpos = vec3(xz.x, y0 + t * H, xz.y);
  v_q = vec2(a_pos.x, t * H / (W * 0.5));   // half-width units: the head is a unit circle at the top
  v_top = H / (W * 0.5);
  v_h = h;
  gl_Position = u_viewProj * vec4(v_wpos, 1.0);
}
)";
const char* FLOWER_FS = R"(
in vec3 v_wpos;
in vec2 v_q;
in vec4 v_h;
in float v_top;
out vec4 fragColor;
vec3 petalCol(float k){
  if (k < 0.16) return vec3(0.62, 0.04, 0.04);   // red
  if (k < 0.32) return vec3(0.86, 0.62, 0.03);   // yellow
  if (k < 0.45) return vec3(0.82, 0.8, 0.75);    // white
  if (k < 0.58) return vec3(0.28, 0.07, 0.55);   // purple
  if (k < 0.7) return vec3(0.08, 0.16, 0.68);    // blue
  if (k < 0.84) return vec3(0.86, 0.26, 0.42);   // pink
  return vec3(0.88, 0.28, 0.02);                 // orange
}
void main(){
  vec2 c = vec2(v_q.x, v_q.y - (v_top - 1.0));
  float r = length(c);
  float n = 4.0 + floor(fract(v_h.x * 3.7) * 5.0);
  float th = atan(c.x, c.y);
  float pr = 0.55 + 0.45 * pow(abs(cos(th * n * 0.5)), 0.7);
  bool head = r < pr;
  bool stem = abs(v_q.x) < 0.09 && v_q.y < v_top - 0.6;
  if (!head && !stem) discard;
  vec3 alb = petalCol(fract(v_h.w * 7.31 + v_h.z * 3.17));
  alb *= 0.8 + 0.3 * smoothstep(0.0, 0.8, r);
  if (head && r < 0.28) alb = fract(v_h.y * 11.0) < 0.6 ? vec3(0.75, 0.5, 0.02) : vec3(0.12, 0.07, 0.03);
  if (!head) alb = vec3(0.09, 0.2, 0.04);
  vec3 nn = normalize(vec3(c.x * 0.3, 1.0, 0.4));
  float sh = shadowAt(v_wpos, vec3(0.0, 1.0, 0.0));
  vec3 col = outColor(lambert(alb, nn, sh));
  float dpt = distance(v_wpos, u_camPos);
  col = mix(col, fogTint(v_wpos), fogGround(v_wpos, dpt));
  fragColor = vec4(col, 1.0);
}
)";

const char* LEAF_FS = R"(
in vec3 v_wpos;
in float v_depth;
in vec2 v_uv;
in vec4 v_h;
uniform float u_leafAut;
out vec4 fragColor;
// leaf silhouette in local coords: q.y along leaf (-1 stem .. 1 tip), q.x across. returns signed inside (>0 inside)
float leafShape(vec2 q, float kind){
  float y = q.y * 0.5 + 0.5;
  if (y < 0.0 || y > 1.0) return -1.0;
  float w;
  if (kind < 0.33) {            // ovate, pointed tip, fine teeth
    w = 0.48 * pow(sin(y * 3.14159), 0.75) * (1.15 - 0.5 * y) * (1.0 + 0.06 * sin(y * 60.0));
  } else if (kind < 0.66) {     // oak: rounded lobes
    w = 0.5 * pow(sin(y * 3.14159), 0.55) * (0.62 + 0.38 * abs(sin(y * 14.0 + 0.4))) * (1.0 - 0.2 * y);
  } else {                      // maple: five pointed lobes (polar)
    vec2 c = vec2(q.x, q.y + 0.25);
    float r = length(c), th = atan(c.x, c.y);
    if (abs(th) > 2.6) return -1.0;
    float lob = 0.42 + 0.5 * pow(abs(cos(th * 2.5)), 0.6);
    return lob * 0.85 - r;
  }
  return w - abs(q.x);
}
void main(){
  vec2 q = v_uv;
  float kind = fract(v_h.x * 7.3 + v_h.y * 2.9);
  float inside = leafShape(q, kind);
  float stem = step(abs(q.x), 0.035) * step(q.y, -0.9) * step(-1.2, q.y);
  if (inside < 0.0 && stem < 0.5) discard;
  float k = fract(v_h.z * 17.3 + v_h.y * 3.1);
  vec3 c0 = vec3(0.62, 0.16, 0.03), c1 = vec3(0.8, 0.38, 0.05), c2 = vec3(0.74, 0.56, 0.1), c3 = vec3(0.34, 0.18, 0.07), c4 = vec3(0.22, 0.13, 0.06);
  vec3 alb = k < 0.25 ? c0 : k < 0.5 ? c1 : k < 0.68 ? c2 : k < 0.86 ? c3 : c4;
  // outside autumn the litter is last year's leaves and needles: browns and greys, not fresh orange
  alb = mix(alb, mix(vec3(0.3, 0.21, 0.12), vec3(0.42, 0.33, 0.2), fract(v_h.w * 7.7)), 1.0 - u_leafAut);
  // decay: brown spots and drier edges on older leaves
  float spot = smoothstep(0.62, 0.7, vnoise(v_wpos.xz * 40.0 + v_h.xy * 9.0));
  alb = mix(alb, c4, spot * 0.6);
  alb *= 0.82 + 0.3 * fract(v_h.w * 29.0);
  float rib = 1.0 - 0.28 * smoothstep(0.04, 0.0, abs(q.x));
  float vt = q.y * 4.0 - abs(q.x) * 3.0;
  float vein = 1.0 - 0.12 * smoothstep(0.1, 0.0, abs(fract(vt) - 0.5));
  float edge = mix(0.65, 1.0, smoothstep(0.0, 0.06, inside));
  float curl = mix(0.82, 1.06, smoothstep(0.0, 0.4, abs(q.x)));     // edges catch more light than the creased middle
  alb *= rib * vein * edge * curl;
  if (stem > 0.5) alb = c4;
  alb = mix(alb, vec3(0.85, 0.88, 0.92), u_snow * 0.8);
  vec3 n = normalize(vec3(q.x * 0.35, 1.0, 0.0));
  float sh = shadowAt(v_wpos, vec3(0.0, 1.0, 0.0));
  vec3 c = outColor(lambert(alb, n, sh));
  c = mix(c, fogTint(v_wpos), fogGround(v_wpos, v_depth));
  fragColor = vec4(c, 1.0);
}
)";

GLuint g_vao = 0, g_vbo = 0, g_leafVao = 0, g_leafVbo = 0;
GLuint g_grassTex = 0;
const bool g_farBlades = std::getenv("HW_FARGRASS") != nullptr;  // HW_FARGRASS=1: old far blade rings back  // assets/textures/grass_atlas.png: 8 real blades + 4 composed clumps (built offline from Foliage001)
Texture g_density;
int g_lastBlades = 0;

struct Ring { float spacing, rIn, rOut, fade, width, height, density; };

std::vector<uint8_t> g_densCpu;
const int GT = 64;
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
bool g_noCap = false;  // loading screen: build every tile around the player at once

// grass tile blade lists are built on a worker thread (terrain height lookups per blade are the expensive part);
// the main thread only uploads finished lists, so walking into new ground no longer costs frame time
struct TileJob { uint64_t key; int ring; float s; int tx, tz; };
struct TileDone { uint64_t key; std::vector<float> v; };
std::mutex g_tq;
std::condition_variable g_tcv;
std::deque<TileJob> g_jobs;
std::vector<TileDone> g_done;
std::unordered_set<uint64_t> g_pending;
std::thread g_tileWorker;
std::atomic<bool> g_tileStop{false};

std::vector<float> buildTileData(float s, int tx, int tz) {
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
      static const bool keepBare = std::getenv("HW_NOHILL") != nullptr;  // A/B measurement switch
      if (!keepBare && envmap::bareAt(x, z) > 0.69f) continue;  // bare-earth patch: the shader would shrink it to a 5% stub anyway
      float y = world::getTerrainHeight(x, z);
      float b[8] = {x, y, z, dn, h[0], h[1], h[2], h[3]};
      v.insert(v.end(), b, b + 8);
    }
  return v;
}

void tileWorkerLoop() {
  while (!g_tileStop) {
    TileJob j;
    {
      std::unique_lock<std::mutex> lk(g_tq);
      g_tcv.wait(lk, [] { return g_tileStop || !g_jobs.empty(); });
      if (g_tileStop) return;
      j = g_jobs.front();
      g_jobs.pop_front();
    }
    std::vector<float> v = buildTileData(j.s, j.tx, j.tz);
    std::lock_guard<std::mutex> lk(g_tq);
    g_done.push_back({j.key, std::move(v)});
  }
}

GTile& storeTile(uint64_t key, const std::vector<float>& v) {
  GTile& t = g_tiles[key];
  t.used = g_frame;
  t.n = (int)(v.size() / 8);
  t.bytes = v.size() * 4;
  glGenBuffers(1, &t.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, t.vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)std::max<size_t>(t.bytes, 4), v.empty() ? nullptr : v.data(), GL_STATIC_DRAW);
  g_tileBytes += t.bytes;
  gfx::addBufferBytes((long long)t.bytes);
  return t;
}

void drainTiles() {
  std::vector<TileDone> done;
  { std::lock_guard<std::mutex> lk(g_tq); done.swap(g_done); }
  for (auto& d : done) {
    g_pending.erase(d.key);
    if (!g_tiles.count(d.key)) storeTile(d.key, d.v);
  }
}

GTile* tileFor(int ring, float s, int tx, int tz, bool prewarm = false) {
  uint64_t key = ((uint64_t)ring << 56) ^ ((uint64_t)(uint32_t)(tx + 100000) << 28) ^ (uint64_t)(uint32_t)(tz + 100000);
  auto it = g_tiles.find(key);
  if (it != g_tiles.end()) { it->second.used = g_frame; return &it->second; }
  if (g_noCap || !g_tileWorker.joinable()) { g_newTiles++; return &storeTile(key, buildTileData(s, tx, tz)); }  // loading: synchronous
  if (g_pending.insert(key).second) {
    std::lock_guard<std::mutex> lk(g_tq);
    if (prewarm) g_jobs.push_back({key, ring, s, tx, tz});
    else g_jobs.push_front({key, ring, s, tx, tz});  // visible tiles jump the queue
    g_tcv.notify_one();
  }
  return nullptr;
}

void evictTiles() {
  const size_t CAP = 64u << 20;
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
void setPreload(bool on) {
  g_noCap = on;
  if (!on && !g_tileWorker.joinable()) g_tileWorker = std::thread(tileWorkerLoop);  // loading done: build on the worker from now on
}

void init() {
  {
    int w = 0, h = 0;
    std::vector<uint8_t> px = assets::loadImageRGBA("textures/grass_atlas.png", w, h);
    if (!px.empty()) {
      glGenTextures(1, &g_grassTex);
      glBindTexture(GL_TEXTURE_2D, g_grassTex);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glTexImage2D(GL_TEXTURE_2D, 0, gfx::hasS3TC() ? 0x83F3 /* DXT5: keeps the blade edges */ : GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
      glGenerateMipmap(GL_TEXTURE_2D);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glBindTexture(GL_TEXTURE_2D, 0);
      gfx::addBufferBytes((long long)w * h * (gfx::hasS3TC() ? 1 : 4) * 4 / 3);
    } else std::fprintf(stderr, "grass atlas missing: plain blades\n");
  }
  const float V[] = {-0.5f, 0, 0.5f, 0, -0.42f, 0.3f, 0.42f, 0.3f, -0.28f, 0.62f, 0.28f, 0.62f, 0, 1,
                     -0.5f, 0, 0.5f, 0, -0.34f, 0.5f, 0.34f, 0.5f, 0, 1,
                     -0.5f, 0, 0.5f, 0, 0, 1,
                     -0.5f, 0, 0.5f, 0, -0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 1, 0.5f, 1};  // 7-, 5-, 3-vertex blades, 6-vertex textured strip
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
  drainTiles();
  float R = g_settings.fullMap ? 220.0f : std::max(40.0f, g_settings.grassRadius * 3.3f);
  R *= (0.5f + 0.5f * forest::detailLevel()) * g_settings.grassAuto * (1.0f - 0.3f * season.snow) * (1.0f - 0.3f * sky::rainAmount());
  float RF = std::max(R + 40.0f, 300.0f * std::min(1.5f, g_settings.drawDist() / 2.0f));
  if (!g_settings.fullMap) RF = std::max(R + 10.0f, std::min(RF, envmap::g_cullQ));  // far blades end where the haze is opaque
  const float cs = g_grassTex ? 1.6f : 1.0f;  // clump cards carry 15-30 blades each: fewer far instances for the same density
  // 3D blades only within ~48 m: beyond that the photo grass on the ground carries the look (Far Cry-style)
  Ring rings[4] = {
      {g_grassTex ? 0.26f : 0.17f, 0, std::min(26.0f, R * 0.25f), 6, 0.055f, 1.0f, 1.0f},  // whole-plant cards: ~2.3x fewer near instances
      {0.36f * cs, std::min(26.0f, R * 0.25f), g_farBlades ? R * 0.55f : std::min(R * 0.55f, 48.0f), 10, 0.1f, 1.05f, 0.95f},
      {1.15f * cs, R * 0.55f, R, 14, 0.34f, 1.1f, 0.9f},
      {3.0f * cs, R, RF, 40, 0.75f, 1.25f, 0.85f},  // narrower so far blades read as grass, not triangles  // sparse wide blades out to the horizon
  };
  static const std::string vs = std::string(terrain_render::groundCommon()) + GRASS_VS;
  Shader& s = render::program("grassgpu", vs.c_str(), GRASS_FS);
  s.set1i("u_bake", 4);
  s.set1i("u_grassTex", 5);
  s.set1i("u_waterLv", 21);
  s.set1f("u_texOn", g_grassTex ? 1.0f : 0.0f);
  s.set3f("u_texAvg", 0.303f, 0.333f, 0.168f);  // mean colour of the atlas (from the offline build)
  glActiveTexture(GL_TEXTURE5);
  glBindTexture(GL_TEXTURE_2D, g_grassTex);
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
  for (int ri = 0; ri < 4; ri++) {
    if (ri >= 2 && !g_farBlades) continue;
    const Ring& r = rings[ri];
    s.set4f("u_ring", r.spacing, r.rIn, r.rOut, r.fade);
    s.set4f("u_blade", r.width, r.height, r.density * snowK, (float)ri);
    if (ri > 0) { float z[32] = {}; glUniform4fv(s.loc("u_tramp"), 8, z); }  // far rings: no per-blade trample loop
    if (ri == 0) {
      float tr[24 * 3] = {};
      const Vec3& cp = render::cam.pos;
      int n = wildlife::nearPositions(cp.x, cp.z, 30, tr, 8);
      n += village::nearPositions(cp.x, cp.z, 30, tr + n * 3, 8 - n);
      float u[32] = {};
      for (int i = 0; i < n && i < 8; i++) { u[i * 4] = tr[i * 3]; u[i * 4 + 1] = tr[i * 3 + 1]; u[i * 4 + 2] = tr[i * 3 + 2]; }
      glUniform4fv(s.loc("u_tramp"), 8, u);
    }
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
        // tile height bounds never change: computed once (9 terrain lookups) and kept
        static std::unordered_map<uint64_t, std::pair<float, float>> s_bounds;
        uint64_t bk = ((uint64_t)ri << 58) ^ ((uint64_t)(uint32_t)(tx + (1 << 20)) << 29) ^ (uint64_t)(uint32_t)(tz + (1 << 20));
        auto bit = s_bounds.find(bk);
        if (bit == s_bounds.end()) {
          float a = 1e9f, b = -1e9f;
          for (float u : {0.0f, 0.5f, 1.0f})
            for (float v : {0.0f, 0.5f, 1.0f}) {
              float h = world::getTerrainHeight(x0 + (x1 - x0) * u, z0 + (z1 - z0) * v);
              a = std::min(a, h), b = std::max(b, h);
            }
          bit = s_bounds.emplace(bk, std::make_pair(a, b)).first;
        }
        float h0 = bit->second.first, h1 = bit->second.second;
        if (!cam.box({x0, h0 - 4, z0}, {x1, h1 + 6, z1})) continue;
        GTile* tp = tileFor(ri, r.spacing, tx, tz);
        if (!tp || tp->n <= 0) continue;
        GTile& t = *tp;
        glBindBuffer(GL_ARRAY_BUFFER, t.vbo);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 32, (void*)0);
        glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, 32, (void*)16);
        if (g_grassTex) glDrawArraysInstanced(GL_TRIANGLE_STRIP, 15, 6, t.n);  // textured strip (blade or clump card)
        else glDrawArraysInstanced(GL_TRIANGLE_STRIP, ri == 0 ? 0 : 7, ri == 0 ? 7 : 5, t.n);  // all outer rings: tapered 5-vertex blades (3-vertex read as flat triangles)
        gfx::drawCalls++;
        g_lastBlades += t.n;
      }
  }
  for (int ri = 0; ri < 4; ri++) {
    if (ri >= 2 && !g_farBlades) continue;
    const Ring& r = rings[ri];
    float ts = GT * r.spacing, R2 = r.rOut + ts;
    int tx0 = (int)std::floor((cam.pos.x - R2) / ts), tx1 = (int)std::floor((cam.pos.x + R2) / ts);
    int tz0 = (int)std::floor((cam.pos.z - R2) / ts), tz1 = (int)std::floor((cam.pos.z + R2) / ts);
    for (int tz = tz0; tz <= tz1; tz++)
      for (int tx = tx0; tx <= tx1; tx++) {
        float x0 = tx * ts, z0 = tz * ts;
        float nx = clampf(cam.pos.x, x0, x0 + ts) - cam.pos.x, nz = clampf(cam.pos.z, z0, z0 + ts) - cam.pos.z;
        if (nx * nx + nz * nz > R2 * R2) continue;
        tileFor(ri, r.spacing, tx, tz, true);
      }
  }
  g_newTiles = 0;
  evictTiles();
  glBindVertexArray(0);
  float autK = clampf(season.autumn * 1.1f + season.fall * 0.5f - season.spring * 0.6f - season.dry * 0.4f, 0, 1);
  float amt = std::max(autK, 0.3f * (1 - season.snow));  // a thin brown carpet under trees all year, thick orange in autumn
  float bl = bloom();
  if (bl > 0.03f) {
    Shader& f = render::program("flowers", FLOWER_VS, FLOWER_FS);
    f.set1i("u_height", 1);
    f.set1i("u_density", 3);
    f.set1i("u_waterLv", 21);
    f.set4f("u_grid", GRID_MIN, GRID_STEP, (float)GRID_N, 0);
    const float sp = 0.35f, RF = 34;
    int n = (int)std::ceil(2 * RF / sp) + 2;
    f.set4f("u_fl", sp, RF, 0.05f * bl, 0.75f * bl);
    glUniform4fv(f.loc("u_fields"), 4, flowerFields());
    f.set1i("u_n", n);
    glBindVertexArray(g_leafVao);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, n * n);
    gfx::drawCalls++;
    glBindVertexArray(0);
  }
  if (amt > 0.03f) {
    Shader& l = render::program("leaflitter", LEAF_VS, LEAF_FS);
    l.set1i("u_height", 1);
    l.set1i("u_density", 3);
    l.set1i("u_waterLv", 21);
    l.set4f("u_grid", GRID_MIN, GRID_STEP, (float)GRID_N, 0);
    const float sp = 0.24f, RL = 26;
    int n = (int)std::ceil(2 * RL / sp) + 2;
    l.set4f("u_leaf", sp, RL, amt * 0.75f, 0);
    l.set1f("u_leafAut", autK);
    l.set1i("u_n", n);
    glBindVertexArray(g_leafVao);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, n * n);
    gfx::drawCalls++;
    glBindVertexArray(0);
  }
}

int bladeCount() { return g_lastBlades; }

float bloom() {
  float summer = clampf(1 - season.spring - season.autumn - season.snow, 0, 1);
  return clampf(season.spring * 1.1f + 0.3f * summer, 0, 1) * (1 - season.snow) * (1 - season.fall);
}

const float* flowerFields() {
  // found once: the most open, flat, dry meadows between the settlement and the mountains, far apart
  static float F[16] = {};
  static bool done = false;
  if (done) return F;
  done = true;
  struct C { float x, z, s; };
  std::vector<C> cand;
  for (float r = SETTLEMENT_RADIUS + 50; r < 300; r += 18)
    for (float a = 0; a < TAU; a += 14.0f / r) {
      float x = std::cos(a) * r, z = std::sin(a) * r;
      if (std::fabs(x) > MAP_HALF - 60 || std::fabs(z) > MAP_HALF - 60) continue;
      float h = world::getTerrainHeight(x, z);
      if (h < 0 || h > 70) continue;
      float score = 0;
      bool ok = true;
      for (int k = 0; k < 9 && ok; k++) {
        float sx = x + (k % 3 - 1) * 14.0f, sz = z + (k / 3 - 1) * 14.0f;
        float sl = world::slopeAt(sx, sz), fd = world::forestDensity(sx, sz);
        if (sl > 0.35f || world::riverMask(sx, sz) > 0.02f || world::lakeAt(sx, sz, 8) >= 0 || world::nearRoute(sx, sz) < 6) ok = false;
        score += (1 - fd) * 1.0f + (0.35f - sl);
      }
      if (ok) cand.push_back({x, z, score});
    }
  std::sort(cand.begin(), cand.end(), [](const C& a, const C& b) { return a.s > b.s; });
  int n = 0;
  for (auto& c : cand) {
    bool far = true;
    for (int i = 0; i < n; i++) if (hypot2(c.x - F[i * 4], c.z - F[i * 4 + 1]) < 160) far = false;
    if (!far) continue;
    F[n * 4] = c.x; F[n * 4 + 1] = c.z; F[n * 4 + 2] = 26.0f + 8.0f * (n % 2); F[n * 4 + 3] = 0;
    std::printf("flower field %d at %.0f %.0f\n", n, c.x, c.z);
    if (++n >= 4) break;
  }
  return F;
}

void shutdown() {
  if (g_tileWorker.joinable()) { { std::lock_guard<std::mutex> lk(g_tq); g_tileStop = true; } g_tcv.notify_all(); g_tileWorker.join(); }
  if (g_vbo) glDeleteBuffers(1, &g_vbo);
  if (g_vao) glDeleteVertexArrays(1, &g_vao);
  g_density.destroy();
  if (g_leafVbo) glDeleteBuffers(1, &g_leafVbo);
  if (g_leafVao) glDeleteVertexArrays(1, &g_leafVao);
}

}  // namespace grass
