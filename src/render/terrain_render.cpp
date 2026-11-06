#include <cstring>
#include "terrain_render.h"
#include "../world/terrain.h"
#include "../platform/platform.h"
#include <vector>
#include <cmath>
#include "../game/season.h"
#include "render.h"
#include "grass.h"

namespace {

const char* GROUND_COMMON = R"(
// pre-baked 32 m ground detail tile: rg = bump slope, b = trail-stone shade, a = puddle mask
uniform sampler2D u_gdet;
vec4 gdet(vec3 wp){ return texture(u_gdet, wp.xz * (1.0 / 32.0)); }
vec3 groundShade(vec3 sc, vec3 wp, float ny, float road){
  // seasons travel: autumn colour starts high and creeps down the slopes, spring green rises from the valley floor
  float lAut = clamp(u_autumn * 1.5 - (1.0 - smoothstep(20.0, 200.0, wp.y)) * 0.5, 0.0, 1.0);
  float lSpr = clamp(u_spring * 1.5 - smoothstep(10.0, 160.0, wp.y) * 0.5, 0.0, 1.0);
  float sG = clamp((sc.g - max(sc.r, sc.b)) * 14.0, 0.0, 1.0);
  float sN1 = 0.5 + 0.5 * sin(wp.x * 2.7 + sin(wp.z * 2.1)) * sin(wp.z * 3.1 + sin(wp.x * 1.9));
  float sN2 = 0.5 + 0.5 * sin(wp.x * 0.21 + sin(wp.z * 0.17) * 2.0) * sin(wp.z * 0.19 + 1.3);
  vec3 sAu = mix(vec3(0.42, 0.26, 0.07), vec3(0.5, 0.18, 0.05), sN2);
  sc = mix(sc, sAu * (0.9 + 0.2 * sN1), lAut * sG * 0.45);
  sc = mix(sc, sc * vec3(0.88, 1.18, 0.82), lSpr * sG);
  sc = mix(sc, sc * vec3(1.25, 1.05, 0.6), u_dry * sG * (0.35 + 0.45 * sN2));
#ifndef GROUND_BAKE
  if (road > 0.01) {
    float sCam = 1.0 - smoothstep(20.0, 70.0, length(wp - u_camPos));
    // trail stones come from the pre-baked tile (was 8 noise taps per pixel)
    sc = mix(sc, sc * (gdet(wp).b * 1.4), road * sCam);
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
  // snow settles unevenly: drifts on flats, thin or bare on bumps and slopes, with grass tufts, earth and rock
  // showing through and a dirty, trodden fringe where it thins out (never one flat white sheet)
  float sAmt = 0.0;
  if (u_snow > 0.01) {
    float f1 = vnoise(wp.xz * 0.045), f2 = vnoise(wp.xz * 0.21 + 3.0), f3 = sNear ? vnoise(wp.xz * 0.9 + 7.0) : 0.5;
    float field = f1 * 0.45 + f2 * 0.33 + f3 * 0.22 + sFlat * 0.3 - (1.0 - ny) * 0.8;
    float cover = clamp(u_snow, 0.0, 1.0) * 0.72 * sDry;          // even deep winter leaves bare patches (~25% of flats)
    float thr = 1.15 - cover;
    sAmt = smoothstep(thr, thr + 0.16, field);
    float fringe = sAmt * (1.0 - sAmt) * 4.0;                      // where the snow thins
    vec3 sCol = mix(vec3(0.70, 0.75, 0.83), vec3(0.9, 0.92, 0.96), f2) * (0.92 + 0.08 * sN1);
    sCol = mix(sCol, vec3(0.5, 0.48, 0.45), fringe * 0.45);       // slushy, earthy edge
    vec3 bare = mix(sc, mix(vec3(0.3, 0.25, 0.18), vec3(0.42, 0.38, 0.24), f3), 0.45 * u_snow);  // frozen earth and dead grass between drifts
    sc = mix(bare, sCol, sAmt);
  }
  return sc;
}
float mountainDetailK(vec3 wp, float ny, float k){
  float steep = 1.0 - smoothstep(0.45, 0.85, ny);
#ifndef GROUND_BAKE
  vec3 big = texture(u_ptexA, vec2(wp.x + wp.z * 0.6, wp.y * 1.3) * 0.012).rgb * 2.0;
  float bl = dot(big, vec3(0.333));
#else
  float bl = 1.0;
#endif
  vec2 sp = vec2(wp.x + wp.z * 0.7, wp.y) + vec2(vnoise(wp.xz * 0.006) * 90.0, vnoise(wp.zx * 0.008 + 3.0) * 40.0);
  sp *= k;
  float rb = vnoise(sp * vec2(0.03, 0.05)) * 0.5 + vnoise(wp.xz * 0.011 + 4.0) * 0.3 + vnoise(sp * 0.13 + 2.0) * 0.2;
  float gul = smoothstep(0.5, 0.85, vnoise(vec2(sp.x * 0.14, sp.y * 0.012)));
  float rid = smoothstep(0.62, 0.92, vnoise(vec2(sp.x * 0.09, sp.y * 0.02) + 7.0));
  float hi = smoothstep(80.0, 150.0, wp.y);
  return mix(1.0, mix(0.6, 1.3, rb) * (1.0 - 0.4 * gul) * (1.0 + 0.28 * rid) * mix(1.0, bl, 0.7), (0.3 + 0.7 * steep) * mix(0.3, 1.0, hi));
}
float mountainDetail(vec3 wp, float ny){
#ifndef GROUND_BAKE
  return mountainDetailK(wp, ny, mix(1.0, 0.2, smoothstep(150.0, 700.0, length(wp - u_camPos))));
#else
  return mountainDetailK(wp, ny, 1.0);
#endif
}
vec3 groundBump(vec3 n, vec3 wp, float road){
  float sK = (0.42 + 0.35 * road) * (1.0 - smoothstep(8.0, 15.0, length(wp - u_camPos)));
  if (sK > 0.001) {
    // bump slope from the pre-baked tile (was 3 noise taps)
    vec2 g = gdet(wp).rg * 2.0 - 1.0;
    n = normalize(n + vec3(-g.x * sK, 0.0, -g.y * sK));
  }
  return n;
}
)";

// one-off generator for the ground detail tile (periodic noise, so the 32 m tile repeats seamlessly)
const char* GDET_FS = R"(
in vec2 v_uv;
out vec4 o;
float hp(vec2 i, float P){ i = mod(i, P); return fract(sin(dot(i, vec2(127.1, 311.7))) * 43758.5453); }
float vn(vec2 p, float P){ vec2 i = floor(p), f = fract(p); vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(hp(i, P), hp(i + vec2(1, 0), P), u.x), mix(hp(i + vec2(0, 1), P), hp(i + vec2(1, 1), P), u.x), u.y); }
// noise at a whole number of cells per tile
float tn(vec2 uv, float cells, float seed){ return vn(uv * cells + seed * 17.0, cells); }
void main(){
  vec2 uv = v_uv;                  // 0..1 across 32 m
  // bump: one octave at ~1.3 per metre, forward-difference slope
  float bc = 42.0, e = 0.12 / bc, c0 = tn(uv, bc, 0.0);
  vec2 g = vec2(tn(uv + vec2(e, 0.0), bc, 0.0) - c0, tn(uv + vec2(0.0, e), bc, 0.0) - c0) * 5.0;
  // trail stones: warped grain, two stone sizes blended along the trail, pebbles and dark gaps
  vec2 w = (vec2(tn(uv, 5.0, 1.0), tn(uv, 6.0, 2.0)) - 0.5) * (4.0 / 32.0);  // +-2 m warp
  vec2 q = uv + w;
  float g1 = tn(q, 224.0, 3.0), g2 = tn(q, 74.0, 4.0);
  float sz = tn(q, 3.0, 5.0);
  float pb = mix(tn(q, 320.0, 6.0), tn(q, 128.0, 7.0), sz);
  float dk = mix(tn(q, 208.0, 8.0), tn(q, 84.0, 9.0), sz);
  float peb = smoothstep(0.74, 0.84, pb);
  float drk = smoothstep(0.76, 0.86, dk) * smoothstep(0.3, 0.6, tn(q, 7.0, 10.0));
  float f = 0.84 + 0.22 * g1 + 0.12 * g2;
  f = mix(f, 1.25, peb * 0.45);
  f = mix(f, 0.52, drk * 0.55);
  // puddle mask
  float pm = tn(uv, 11.0, 11.0) * 0.7 + tn(uv, 42.0, 12.0) * 0.3;
  o = vec4(clamp(g * 0.5 + 0.5, 0.0, 1.0), clamp(f / 1.4, 0.0, 1.0), pm);
}
)";

// one-off bake of the mountain rock detail over the whole terrain (r: near pattern, g: the 5x larger far pattern).
// The far terrain used to compute this per vertex every frame (7 noise taps + a texture read each).
const char* MDET_FS = R"(
in vec2 v_uv;
uniform sampler2D u_height;
uniform vec4 u_grid;
out vec4 o;
float hAt(ivec2 c){ c = clamp(c, ivec2(0), ivec2(int(u_grid.z) - 1)); return texelFetch(u_height, c, 0).r; }
float heightAt(vec2 g){
  vec2 i = floor(g); vec2 f = g - i; ivec2 c = ivec2(i);
  float a = hAt(c), b = hAt(c + ivec2(1, 0)), cc = hAt(c + ivec2(0, 1)), d = hAt(c + ivec2(1, 1));
  if (f.x + f.y <= 1.0) return a + (b - a) * f.x + (cc - a) * f.y;
  return d + (cc - d) * (1.0 - f.x) + (b - d) * (1.0 - f.y);
}
void main(){
  vec2 g = v_uv * (u_grid.z - 1.0);
  vec2 w = u_grid.x + g * u_grid.y;
  float h = heightAt(g);
  ivec2 ci = ivec2(floor(g + 0.5));
  float hl = hAt(ci - ivec2(1, 0)), hr = hAt(ci + ivec2(1, 0)), hd = hAt(ci - ivec2(0, 1)), hu = hAt(ci + ivec2(0, 1));
  vec3 n = normalize(vec3(hl - hr, 2.0 * u_grid.y, hd - hu));
  vec3 wp = vec3(w.x, h, w.y);
  o = vec4(mountainDetailK(wp, n.y, 1.0) / 3.0, mountainDetailK(wp, n.y, 0.2) / 3.0, 0.0, 1.0);
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
  {
    float grassy = smoothstep(0.7, 0.9, n.y) * (1.0 - smoothstep(70.0, 120.0, h));
    float m1 = vnoise(wp3.xz * 0.009), m2 = vnoise(wp3.xz * 0.031 + 7.0), m3 = vnoise(wp3.xz * 0.11 + 3.0);
    vec3 tint = mix(vec3(0.86, 1.04, 0.8), vec3(1.14, 1.06, 0.74), smoothstep(0.35, 0.75, m1));  // lush vs sun-dried
    tint = mix(tint, vec3(0.78, 0.92, 0.72), smoothstep(0.62, 0.85, m2) * 0.7);                   // mossy hollows
    tint *= 0.9 + 0.2 * m3;                                                                       // fine mottling
    sc *= mix(vec3(1.0), tint, grassy * (1.0 - u_snow));
    // real ground is never one green: bare earth, dry golden tufts, needle litter and stony scree mixed through it
    float e1 = vnoise(wp3.xz * 0.05 + 11.0) * 0.6 + vnoise(wp3.xz * 0.27 + 2.0) * 0.4;
    float earth = smoothstep(0.6, 0.72, e1) * (1.0 - u_snow);
    sc = mix(sc, vec3(0.2, 0.15, 0.1) * (0.85 + 0.3 * m3), earth * 0.75);
    float dryT = smoothstep(0.58, 0.7, vnoise(wp3.xz * 0.6 + 5.0)) * (1.0 - earth) * (1.0 - u_snow);
    sc = mix(sc, sc * vec3(1.35, 1.12, 0.62), dryT * 0.55);
    float litter = smoothstep(0.5, 0.8, vnoise(wp3.xz * 0.018 + 9.0)) * smoothstep(0.55, 0.85, m3) * (1.0 - u_snow);
    sc = mix(sc, vec3(0.24, 0.15, 0.08), litter * 0.45);
    float scree = smoothstep(0.66, 0.74, vnoise(wp3.xz * 0.9 + 17.0)) * smoothstep(0.5, 0.8, 1.0 - n.y + e1 * 0.3);
    sc = mix(sc, vec3(0.34, 0.33, 0.31) * (0.8 + 0.4 * m3), scree * 0.6);
  }
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
uniform sampler2D u_mdet;
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
// Catmull-Rom bicubic: curved surface through the grid points, no creases between 2.4 m cells
vec4 crw(float t){ float t2 = t * t, t3 = t2 * t; return vec4(-0.5*t3 + t2 - 0.5*t, 1.5*t3 - 2.5*t2 + 1.0, -1.5*t3 + 2.0*t2 + 0.5*t, 0.5*t3 - 0.5*t2); }
vec4 crd(float t){ float t2 = t * t; return vec4(-1.5*t2 + 2.0*t - 0.5, 4.5*t2 - 5.0*t, -4.5*t2 + 4.0*t + 0.5, 1.5*t2 - t); }
// height + analytic gradient from one set of 16 texel fetches
vec3 heightCubicD(vec2 g){
  vec2 i = floor(g); vec2 f = g - i; ivec2 c = ivec2(i) - ivec2(1);
  vec4 wx = crw(f.x), wz = crw(f.y), dx = crd(f.x), dz = crd(f.y);
  vec3 r = vec3(0.0);
  for (int z = 0; z < 4; z++) {
    vec4 row = vec4(hAt(c + ivec2(0, z)), hAt(c + ivec2(1, z)), hAt(c + ivec2(2, z)), hAt(c + ivec2(3, z)));
    float a = dot(row, wx);
    r += vec3(a * wz[z], dot(row, dx) * wz[z], a * dz[z]);
  }
  return r;
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
  bool fine = st < 0.99;
  vec3 hc = fine ? heightCubicD(cell) : vec3(0.0);
  float h = fine ? hc.x : heightAt(cell);
  if (a_grid.z > 0.5) h -= 1.5 + st * 1.2;
  if (fine) {
    v_nrm = normalize(vec3(-hc.y, u_grid.y, -hc.z));
  } else {
    ivec2 ci = clamp(ivec2(floor(cell + 0.5)), ivec2(0), ivec2(int(u_grid.z) - 1));
    v_nrm = texelFetch(u_height, ci, 0).gba;  // precomputed normal
  }
  v_wpos = vec3(w.x, h, w.y);
  v_cell = cell;
  vec4 vp = u_view * vec4(v_wpos, 1.0);
  v_depth = -vp.z;
  v_farCol = vec3(0.0);
  if (d > 30.0) {
    vec4 t = textureLod(u_bake, cell / (u_grid.z - 1.0), clamp(log2(d / 30.0), 0.0, 6.0));
    vec2 md = textureLod(u_mdet, cell / (u_grid.z - 1.0), clamp(log2(d / 90.0), 0.0, 6.0)).rg * 3.0;  // pre-baked rock detail
    vec3 sc = t.rgb * t.rgb * mix(md.x, md.y, smoothstep(150.0, 700.0, length(v_wpos - u_camPos)));
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
uniform sampler2D u_height;
uniform sampler2D u_litter;
uniform sampler2DArray u_ground;  // photo ground layers (render.cpp)
uniform vec4 u_fields[4];         // spring flower fields: x, z, radius (grass.cpp); u_fields[0].w = bloom strength
vec3 flowerPal(float k){
  if (k < 0.16) return vec3(0.62, 0.04, 0.04);
  if (k < 0.32) return vec3(0.86, 0.62, 0.03);
  if (k < 0.45) return vec3(0.82, 0.8, 0.75);
  if (k < 0.58) return vec3(0.28, 0.07, 0.55);
  if (k < 0.7) return vec3(0.08, 0.16, 0.68);
  if (k < 0.84) return vec3(0.86, 0.26, 0.42);
  return vec3(0.88, 0.28, 0.02);
}
uniform sampler2D u_waterLv;      // r water surface (ground where dry), g 1 lake / 0.6 river / 0 dry (envmap.cpp)
// photo grass as detail: texture / its own mean colour, so the season colour underneath stays and only the
// blade-and-clump pattern is added. Lush near water, dry up high and in patches, meadow elsewhere.
vec3 grassRatio(vec2 xz, float y, float wet){
  vec2 uv = xz * 0.35;
  float wl = smoothstep(0.12, 0.45, wet);
  float wd = clamp(smoothstep(70.0, 150.0, y) * 0.7 + smoothstep(0.55, 0.8, vnoise(xz * 0.02)) * 0.6 + u_dry * 0.5, 0.0, 1.0) * (1.0 - wl);
  vec3 t = texture(u_ground, vec3(uv, 9.0)).rgb / vec3(0.1204, 0.2168, 0.0235);
  if (wl > 0.01) t = mix(t, texture(u_ground, vec3(uv, 7.0)).rgb / vec3(0.0418, 0.0734, 0.0181), wl);
  if (wd > 0.01) t = mix(t, texture(u_ground, vec3(uv, 8.0)).rgb / vec3(0.0557, 0.0702, 0.0139), wd);
  float l = dot(t, vec3(0.3, 0.59, 0.11));
  return clamp(mix(vec3(l), t, 0.35), vec3(0.4), vec3(1.9));
}
out vec4 fragColor;
void main(){
  float fd = distance(v_wpos.xz, u_camPos.xz);
  if (fd > 50.0) {
    // far ground: per-pixel grass texture (clumps, dry tufts, darker hollows) instead of flat vertex colour;
    // finer layers fade out with distance so nothing shimmers
    vec3 c = v_farCol;
    float green = clamp((c.g - max(c.r, c.b)) * 10.0, 0.0, 1.0);
    vec2 q = v_wpos.xz;
    float amp = green * (1.0 - smoothstep(250.0, 650.0, fd));
    if (amp > 0.01) c *= mix(vec3(1.0), grassRatio(q, v_wpos.y, envAt(q).g), amp);  // photo grass texture
    fragColor = vec4(c, 1.0);
    return;
  }
  vec4 b = texture(u_bake, v_cell / (u_grid.z - 1.0));
  vec3 sc = b.rgb * b.rgb;
  float road = b.a;
  // per-pixel normal from the filtered height field: smooth curved shading instead of triangle facets
  // normals come precomputed per vertex from the smooth (bicubic) surface: no per-pixel height lookups
  vec3 n = normalize(v_nrm);
  if (road > 0.01 && fd < 70.0) {
    vec4 t = texture(u_color, (v_cell + 0.5) / u_grid.z);
    sc = groundShade(srgbToLinear(t.rgb), v_wpos, n.y, t.a);
  }
  vec3 nb = groundBump(n, v_wpos, road);
  // snow surface: wind-carved ripples and soft blue shade in hollows
  {
    float lum = dot(sc, vec3(0.3, 0.59, 0.11));
    float snowy = smoothstep(0.42, 0.62, lum) * (1.0 - smoothstep(0.06, 0.16, max(sc.r, max(sc.g, sc.b)) - min(sc.r, min(sc.g, sc.b)))) * (1.0 - smoothstep(10.0, 16.0, fd));
    if (snowy > 0.01) {
      vec2 q = v_wpos.xz;
      float rip = vnoise(vec2(q.x * 0.35 + vnoise(q * 0.08) * 3.0, q.y * 1.4));
      float drift = vnoise(q * 0.05);
      float hol = clamp(rip * 0.6 + (1.0 - drift) * 0.4, 0.0, 1.0);
      sc = mix(sc, sc * mix(vec3(1.03, 1.03, 1.04), vec3(0.84, 0.9, 1.0), hol), snowy);
      vec2 g = vec2(vnoise(vec2((q.x + 0.4) * 0.35, q.y * 1.4)) - rip, vnoise(vec2(q.x * 0.35, (q.y + 0.3) * 1.4)) - rip) * 2.0;
      nb = normalize(nb + vec3(-g.x, 0.0, -g.y) * 0.35 * snowy);
    }
  }
  if (fd < 50.0) {
    // photographed ground up close (Poly Haven CC0): each material goes where the baked maps say it belongs.
    // Only layers with weight are sampled, so most pixels read one or two textures.
    float fadeG = (1.0 - smoothstep(30.0, 45.0, fd)) * (1.0 - u_snow);
    vec4 dm = texture(u_detailMap, envUV(v_wpos.xz));
    float wet = envAt(v_wpos.xz).g;
    vec2 uv = v_wpos.xz * 0.5;  // ~2 m per tile
    float macro = 0.85 + 0.3 * vnoise(v_wpos.xz * 0.13);
    vec2 wv = texture(u_waterLv, envUV(v_wpos.xz)).rg;
    float uw = wv.g > 0.3 ? smoothstep(0.03, 0.3, wv.r - v_wpos.y) : 0.0;  // under a lake or river
    float dryG = 1.0 - uw;
    float green0 = clamp((sc.g - max(sc.r, sc.b)) / max(sc.g, 1e-3) * 3.0, 0.0, 1.0);
    float wForest = (1.0 - smoothstep(0.6, 0.92, dm.r)) * (1.0 - road) * dryG;
    float wTrail = smoothstep(0.05, 0.5, road) * dryG;
    float wMud = smoothstep(0.35, 0.7, wet) * (1.0 - road) * (1.0 - wForest * 0.5) * dryG;
    float wDirt = smoothstep(0.6, 0.74, dm.a) * (1.0 - road) * (1.0 - wForest) * dryG;
    float wScree = smoothstep(0.93, 0.78, n.y) * (1.0 - road) * mix(0.5, 1.0, smoothstep(40.0, 110.0, v_wpos.y)) * dryG;
    {
      if (wForest > 0.01 && fadeG > 0.01) {
        float k = smoothstep(0.35, 0.65, vnoise(v_wpos.xz * 0.21));  // patches of fresh litter among the older floor
        vec3 f = mix(texture(u_ground, vec3(uv, 0.0)).rgb, texture(u_ground, vec3(v_wpos.xz * 0.6, 5.0)).rgb, k) * mix(1.0, 1.15, u_autumn);
        sc = mix(sc, f * macro, wForest * 0.85 * fadeG);
      }
      if (wMud > 0.01) sc = mix(sc, texture(u_ground, vec3(uv, 1.0)).rgb * macro, wMud * 0.8 * fadeG);
      if (wTrail > 0.01) sc = mix(sc, texture(u_ground, vec3(uv, 2.0)).rgb * macro, wTrail * 0.75 * fadeG);
      if (wDirt > 0.01) sc = mix(sc, texture(u_ground, vec3(uv, 3.0)).rgb * macro, wDirt * 0.8 * fadeG);
      if (wScree > 0.01) sc = mix(sc, texture(u_ground, vec3(uv * 0.8, 4.0)).rgb * macro, wScree * 0.8 * fadeG);
      if (uw > 0.01) sc = mix(sc, texture(u_ground, vec3(uv * 0.7, 6.0)).rgb * macro * 0.8, uw * (wv.g > 0.8 ? 0.85 : 0.5) * fadeG);  // moss on lake beds, lighter in rivers
      float rest = (1.0 - max(max(max(wForest, wMud), max(wTrail, wDirt)), max(wScree, uw))) * green0;
      if (rest > 0.01) sc *= mix(vec3(1.0), grassRatio(v_wpos.xz, v_wpos.y, wet), rest * 0.9 * (1.0 - u_snow));  // all the way to the far ground (same texture there)
    }
  }
  float steepN = (1.0 - smoothstep(0.4, 0.82, n.y)) * (1.0 - smoothstep(20.0, 32.0, fd)) * mix(0.35, 1.0, max(smoothstep(70.0, 125.0, v_wpos.y), 1.0 - smoothstep(0.4, 0.62, n.y)));
  if (steepN > 0.01) {
    vec3 nr = nb;
    vec3 alb = ptexRock(v_wpos, nr, 0.16, 1.0);
    sc *= mix(vec3(1.0), alb, steepN);
    nb = normalize(mix(nb, nr, steepN));
  }
  float sh = shadowAt(v_wpos, n);
  // rain: soaked ground darkens, puddles collect on trails and flat hollows and mirror the sky
  float wetG = u_wet * (1.0 - u_snow);
  float puddle = 0.0;
  if (wetG > 0.01) {
    sc *= 1.0 - 0.3 * wetG;
  }
  if (wetG > 0.01 && fd < 35.0) {  // puddles within 35 m (ripple rings stay within 15 m)
    float pm = gdet(v_wpos).a;
    puddle = smoothstep(0.62, 0.7, pm + road * 0.18) * smoothstep(0.85, 0.97, n.y) * wetG * (1.0 - smoothstep(27.0, 35.0, fd));
    sc = mix(sc, sc * 0.45, puddle);
    nb = normalize(mix(nb, vec3(0.0, 1.0, 0.0), puddle));
    if (u_rain > 0.05 && puddle > 0.05 && fd < 15.0) {
      // raindrop rings: each 0.5 m cell spawns an expanding ring on its own cycle
      vec2 cell = floor(v_wpos.xz * 2.0), f = fract(v_wpos.xz * 2.0) - 0.5;
      float h = fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.55);
      float ph = fract(u_clock * 1.3 + h);
      vec2 o = (vec2(fract(h * 7.1), fract(h * 3.7)) - 0.5) * 0.4;
      float r = length(f - o);
      float ring = smoothstep(0.04, 0.0, abs(r - ph * 0.45)) * (1.0 - ph) * u_rain;
      vec2 dir = (f - o) / max(r, 1e-3);
      nb = normalize(nb + vec3(dir.x, 0.0, dir.y) * ring * 0.6 * puddle);
    }
  }
  // spring flower fields: colour speckle under the flower cards up close, drifts of colour from afar
  float fieldK = 0.0, fcov = 0.0;
  vec3 fcol = vec3(0.0);
  if (u_fields[0].w > 0.01) {
    for (int i = 0; i < 4; i++) {
      if (u_fields[i].z <= 0.0) continue;
      float dd = distance(v_wpos.xz, u_fields[i].xy) + (vnoise(v_wpos.xz * 0.08) - 0.5) * u_fields[i].z * 0.5;
      fieldK = max(fieldK, 1.0 - smoothstep(u_fields[i].z * 0.7, u_fields[i].z, dd));
    }
    fieldK *= u_fields[0].w;
    if (fieldK > 0.01) {
      // up close: small round blossoms (one per 20 cm cell at a random offset, ~half the cells), far: colour drifts
      vec2 sp = v_wpos.xz * 5.0, cid = floor(sp), fr = fract(sp) - 0.5;
      float hh = fract(sin(dot(cid, vec2(12.9898, 78.233))) * 43758.55);
      vec2 off = (vec2(fract(hh * 13.1), fract(hh * 7.7)) - 0.5) * 0.45;
      float dotK = 1.0 - smoothstep(0.16, 0.24, length(fr - off));
      float drift = fract(vnoise(v_wpos.xz * 0.35) * 2.7);
      float nearK = 1.0 - smoothstep(18.0, 45.0, fd);
      fcol = mix(flowerPal(drift), flowerPal(fract(hh * 7.13 + drift)), nearK);
      fcov = fieldK * mix(0.45, step(0.45, hh) * dotK * 0.95, nearK);
      sc = mix(sc, fcol, fcov);
    }
  }
  vec3 litT = lambert(sc, nb, sh);
  if (wetG > 0.01 && fd < 35.0) {
    // puddle sky reflection and wet sheen folded into the same lighting (single tone-map)
    vec3 Vt = normalize(u_camPos - v_wpos);
    float fr = pow(1.0 - max(Vt.y, 0.0), 5.0);
    vec3 skyR = mix(u_fogCol.rgb, u_misc.rgb, 0.4) * (0.35 + 0.65 * u_daylight);
    litT = mix(litT, skyR * 0.6, puddle * (0.35 + 0.6 * fr));
    litT += u_sunCol.rgb * pow(max(dot(reflect(-u_sunDir.xyz, nb), Vt), 0.0), 60.0) * sh * (0.15 + puddle) * wetG * 0.3;
  }
  vec3 c = outColor(litT);
  c = mix(c, fogTint(v_wpos), fogGround(v_wpos, v_depth));
  if (fd > 40.0) c = mix(c, v_farCol, smoothstep(40.0, 50.0, fd));
  if (fd > 40.0 && fcov > 0.0) c = mix(c, mix(outColor(lambert(fcol, vec3(0.0, 1.0, 0.0), sh)), fogTint(v_wpos), fogGround(v_wpos, v_depth)), fcov * smoothstep(40.0, 50.0, fd));
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
uniform float u_trailMat;          // 1 = trail stonework (stairs, stepping pillars): the rocky-trail photo
uniform sampler2DArray u_ground;   // photo ground layers (render.cpp)
out vec4 fragColor;
void main(){
  vec3 n = normalize(v_nrm);
  vec3 sc = groundShade(v_col, v_wpos, n.y, 0.0);
  if (u_trailMat > 0.5) {
    // same rocky-trail texture as the paths, projected from three sides so step faces and pillar sides tile too
    vec3 an = abs(n) + 1e-4;
    vec3 tw = an / (an.x + an.y + an.z);
    vec3 t = texture(u_ground, vec3(v_wpos.zy * 0.5, 2.0)).rgb * tw.x + texture(u_ground, vec3(v_wpos.xz * 0.5, 2.0)).rgb * tw.y
           + texture(u_ground, vec3(v_wpos.xy * 0.5, 2.0)).rgb * tw.z;
    sc = mix(sc, t * (0.85 + 0.3 * vnoise(v_wpos.xz * 0.13)), 0.9);
  } else
  sc *= mountainDetail(v_wpos, n.y);
  vec3 nb = groundBump(n, v_wpos, 0.0);
  {
    // rough crags at a distance: two scales of ridged noise bend the normals and shade the clefts, so far
    // ridges read as broken rock instead of smooth low-poly slopes
    float steep = 1.0 - smoothstep(0.55, 0.9, n.y);
    float k = steep * smoothstep(40.0, 140.0, v_wpos.y);
    if (k > 0.01) {
      vec2 p1 = vec2(v_wpos.x + v_wpos.z, v_wpos.y * 1.6) * 0.05;
      vec2 p2 = v_wpos.xz * 0.11 + v_wpos.y * 0.07;
      float r0 = 1.0 - abs(vnoise(p1) * 2.0 - 1.0), r1 = 1.0 - abs(vnoise(p2) * 2.0 - 1.0);
      float gx = (1.0 - abs(vnoise(p1 + vec2(0.15, 0.0)) * 2.0 - 1.0)) - r0 + (1.0 - abs(vnoise(p2 + vec2(0.15, 0.0)) * 2.0 - 1.0)) - r1;
      float gz = (1.0 - abs(vnoise(p1 + vec2(0.0, 0.15)) * 2.0 - 1.0)) - r0 + (1.0 - abs(vnoise(p2 + vec2(0.0, 0.15)) * 2.0 - 1.0)) - r1;
      nb = normalize(nb + vec3(gx, 0.0, gz) * 3.5 * k);
      sc *= mix(1.0, 0.62 + 0.5 * (r0 * 0.6 + r1 * 0.4), k);
    }
  }
  if (v_wpos.y > 120.0) {
    // alpine look: snow clings to gentler faces above the snowline, steep faces show streaked rock
    vec2 sp = vec2(v_wpos.x + v_wpos.z * 0.6, v_wpos.y);
    float gul = vnoise(vec2(sp.x * 0.03, sp.y * 0.004));
    float line = 210.0 + vnoise(v_wpos.xz * 0.004) * 70.0;
    float alt = smoothstep(line - 40.0, line + 30.0, v_wpos.y + gul * 30.0);
    float slope = smoothstep(0.42, 0.72, n.y + (gul - 0.5) * 0.45 + vnoise(sp * 0.02) * 0.2);
    float snow = alt * slope;
    vec3 rockc = mix(vec3(0.17, 0.15, 0.13), vec3(0.38, 0.33, 0.27), vnoise(sp * vec2(0.02, 0.05))) * mix(0.75, 1.15, gul);
    // sedimentary bands across the cliffs, bent by the gullies: the faces read as layered rock, not flat grey
    float band = 0.5 + 0.5 * sin(v_wpos.y * 0.2 + gul * 7.0 + vnoise(sp * 0.008) * 9.0 + vnoise(sp * 0.03) * 3.0);
    band *= smoothstep(0.25, 0.6, vnoise(sp * vec2(0.006, 0.02) + 5.0));  // strata show in patches, not as a regular stripe pattern
    rockc *= mix(0.92, 1.08, band) * mix(vec3(1.0), vec3(1.06, 0.99, 0.9), band);
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
GLuint g_gdetTex = 0, g_mdetTex = 0;
void buildMountainDetail() {
  const int N = 1024;
  glGenTextures(1, &g_mdetTex);
  glBindTexture(GL_TEXTURE_2D, g_mdetTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, N, N, 0, GL_RG, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  GLint vp[4], prev = 0;
  glGetIntegerv(GL_VIEWPORT, vp);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_mdetTex, 0);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_BLEND);
  glViewport(0, 0, N, N);
  static const std::string fs = std::string(GROUND_COMMON) + MDET_FS;
  Shader& sh = render::program("mdetgen", BAKE_VS, fs.c_str());
  sh.set1i("u_height", 1);
  sh.set4f("u_grid", GRID_MIN, GRID_STEP, (float)GRID_N, 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, g_heightTex.id);
  glActiveTexture(GL_TEXTURE0);
  render::drawFullscreen();
  glBindTexture(GL_TEXTURE_2D, g_mdetTex);
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindFramebuffer(GL_FRAMEBUFFER, prev);
  glDeleteFramebuffers(1, &fbo);
  glViewport(vp[0], vp[1], vp[2], vp[3]);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  gfx::addBufferBytes((long long)N * N * 2 * 4 / 3);
}
GLuint g_vao = 0, g_vbo = 0, g_ibo = 0, g_ivbo = 0;
GLsizei g_indexCount = 0;
size_t g_icap = 0;
Mesh g_backdrop;
std::vector<float> g_minH[LEVELS], g_maxH[LEVELS];
int g_nodes[LEVELS];
float g_range[8];
const float FINE_R[2] = {60.0f, 0.0f};  // radius of 1.2 m and 0.6 m terrain rings
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

// sub-grid patches: same 16x16 mesh at half / quarter cell spacing, height from the bicubic surface
void selectFine(int f, float cx, float cz, float y0, float y1) {
  float st = f == 0 ? 0.5f : 0.25f, span = 16 * st;
  Vec3 mn(GRID_MIN + cx * GRID_STEP, -1e4f, GRID_MIN + cz * GRID_STEP);
  Vec3 mx(mn.x + span * GRID_STEP, 1e4f, mn.z + span * GRID_STEP);
  mn.y = y0; mx.y = y1;
  if (!render::cam.box(mn, mx)) return;
  Vec3 p = render::cam.pos; p.y = 0; mn.y = mx.y = 0;
  float d = boxDist(p, mn, mx);
  if (f == 0 && d < FINE_R[1]) {
    for (int c = 0; c < 4; c++) selectFine(1, cx + (c & 1) * span * 0.5f, cz + (c >> 1) * span * 0.5f, y0, y1);
    return;
  }
  g_inst.push_back(cx);
  g_inst.push_back(cz);
  g_inst.push_back(st);
  g_inst.push_back((float)(6 + f));
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
  if (L == 0 && d < FINE_R[0]) {
    for (int c = 0; c < 4; c++) selectFine(0, (float)(nx * size + (c & 1) * 8), (float)(nz * size + (c >> 1) * 8), mn.y, mx.y);
    return;
  }
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
  {
    // height + precomputed normal in one texel (r = height, gba = normal): far vertices read one texel instead of five
    std::vector<float> HN((size_t)GRID_N * GRID_N * 4);
    auto at = [&](int i, int j) { i = std::max(0, std::min(GRID_N - 1, i)); j = std::max(0, std::min(GRID_N - 1, j)); return H[(size_t)j * GRID_N + i]; };
    for (int j = 0; j < GRID_N; j++)
      for (int i = 0; i < GRID_N; i++) {
        Vec3 n = Vec3(at(i - 1, j) - at(i + 1, j), 2 * GRID_STEP, at(i, j - 1) - at(i, j + 1)).normalized();
        float* o = &HN[((size_t)j * GRID_N + i) * 4];
        o[0] = H[(size_t)j * GRID_N + i]; o[1] = n.x; o[2] = n.y; o[3] = n.z;
      }
    g_heightTex.w = g_heightTex.h = GRID_N;
    glGenTextures(1, &g_heightTex.id);
    glBindTexture(GL_TEXTURE_2D, g_heightTex.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, GRID_N, GRID_N, 0, GL_RGBA, GL_FLOAT, HN.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_heightTex.bytes = HN.size() * 4;
    gfx::addBufferBytes((long long)g_heightTex.bytes);
  }
  const auto& C = world::colorGrid();
  g_colorTex = createRGBA8Texture((const uint8_t*)C.data(), GRID_N, GRID_N, true);
  buildPatchMesh();
  buildBounds();
  g_backdrop = uploadMesh(world::buildBackdrop());
  {
    const int N = 1024;
    glGenTextures(1, &g_gdetTex);
    glBindTexture(GL_TEXTURE_2D, g_gdetTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (gfx::maxAniso() > 1.0f) glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, std::min(4.0f, gfx::maxAniso()));
    GLint vp[4], prev = 0;
    glGetIntegerv(GL_VIEWPORT, vp);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_gdetTex, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glViewport(0, 0, N, N);
    render::program("gdetgen", BAKE_VS, GDET_FS);
    render::drawFullscreen();
    glBindTexture(GL_TEXTURE_2D, g_gdetTex);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindFramebuffer(GL_FRAMEBUFFER, prev);
    glDeleteFramebuffers(1, &fbo);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glEnable(GL_DEPTH_TEST);
    gfx::addBufferBytes((long long)N * N * 4 * 4 / 3);
  }
}

GLuint g_bakeTex = 0, g_bakeFbo = 0;
GLuint heightTexture() { return g_heightTex.id; }
GLuint bakeTexture() { return g_bakeTex; }
GLuint colorTexture() { return g_colorTex.id; }
const char* groundCommon() { return GROUND_COMMON; }
int patchCount() { return g_lastCount; }

const int BAKE_N = 2048, BAKE_T = 512;  // far ground (>50 m) colour bake: 0.6 m texels (was 3072: 2.25x the memory)
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
  static int bakeTick = 0;
  int n = first ? total : ((bakeTick++ % 3) == 0 ? 1 : 0);  // spread season re-bakes thinner: no GPU spikes
  for (int k = 0; k < n && g_bakeNext < total; k++) { bakeTile(g_bakeNext++); if (first) platform::pumpLoad(); }
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
  if (!g_mdetTex) buildMountainDetail();  // needs the rock texture, which exists once shadows are bound
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
    s.set1i("u_gdet", 3);
    s.set1i("u_mdet", 5);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, g_gdetTex);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, g_mdetTex);
    s.set4f("u_grid", GRID_MIN, GRID_STEP, (float)GRID_N, 0);
    {
      float ff[16];
      std::memcpy(ff, grass::flowerFields(), sizeof ff);
      ff[3] = grass::bloom();
      glUniform4fv(s.loc("u_fields"), 4, ff);
    }
    g_range[6] = FINE_R[0] / 0.92f; g_range[7] = FINE_R[1] / 0.92f;
    glUniform1fv(s.loc("u_range"), 8, g_range);
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

void drawGroundMesh(const Mesh& m, bool trail) {
  if (!m.valid()) return;
  static const std::string fsM = std::string(GROUND_COMMON) + MESH_FS;
  Shader& g = render::program("groundmesh", MESH_VS, fsM.c_str());
  g.set1f("u_trailMat", trail ? 1.0f : 0.0f);
  g.set1i("u_gdet", 3);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, g_gdetTex);
  glActiveTexture(GL_TEXTURE0);
  m.draw();
}

void shutdown() {
  g_heightTex.destroy();
  g_colorTex.destroy();
  g_backdrop.destroy();
}

}  // namespace terrain_render
