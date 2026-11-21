#include <algorithm>
#include "../render/terrain_render.h"
#include "../world/terrain.h"
#include "fx.h"
#include "../render/water.h"
#include "season.h"
#include "engine.h"
#include "../render/render.h"
#include "../models/models.h"
#include "../world/terrain.h"
#include "../world/forest.h"
#include "../render/sky.h"
static float sky_rain() { return sky::rainAmount(); }
#include <cmath>

namespace {
// v1.1.11: campfire smoke: a column of soft puffs that rises, widens and bends downwind (flattens when it's damp)
const char* SMOKE_VS = R"(
uniform vec4 u_place;   // fire xyz, size
out float v_a;
out float v_t;
void main(){
  float i = float(gl_VertexID);
  float t = fract(u_clock * 0.06 + i / 22.0 + u_place.x * 0.013);
  float s = u_place.w;
  vec2 wd = normalize(u_windDir + vec2(1e-4));
  float rise = (6.0 + 3.0 * fract(i * 0.618)) * s * (1.0 - 0.55 * u_rain);
  float bend = (0.8 + 3.2 * u_windS) * s;
  vec3 p = u_place.xyz + vec3(0.0, 0.6 * s + t * rise, 0.0) + vec3(wd.x, 0.0, wd.y) * bend * t * t * 3.0
         + vec3(sin(u_clock * 0.4 + i * 2.1), 0.0, cos(u_clock * 0.33 + i * 1.3)) * t * 0.9 * s;
  vec4 mv = u_view * vec4(p, 1.0);
  gl_Position = u_proj * mv;
  gl_PointSize = min(400.0, (0.4 + 2.6 * t) * s * 420.0 / max(-mv.z, 0.3) * (u_viewH / 900.0));
  v_a = smoothstep(0.0, 0.08, t) * (1.0 - t) * (1.0 - t);
  v_t = t + i * 0.37;
}
)";
const char* SMOKE_FS = R"(
in float v_a;
in float v_t;
out vec4 fragColor;
void main(){
  vec2 q = gl_PointCoord * 2.0 - 1.0;
  float r = dot(q, q) + (vnoise(gl_PointCoord * 3.0 + v_t * 5.0) - 0.5) * 0.7;
  if (r > 1.0) discard;
  float lum = 0.12 + 0.5 * u_daylight;
  fragColor = vec4(vec3(0.62, 0.62, 0.64) * lum, v_a * 0.22 * (1.0 - r));
}
)";
// v1.1.11: fireflies on warm nights (blinking green-gold glows) and midge swarms by water at dusk (tiny dark specks)
const char* FLY_VS = R"(
uniform vec4 u_fly[64];   // xyz, w = kind (1 firefly, 2 midge, 0 none)
out float v_k;
out float v_b;
void main(){
  vec4 f = u_fly[gl_VertexID];
  float i = float(gl_VertexID);
  if (f.w < 0.5) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); gl_PointSize = 0.0; v_k = 0.0; v_b = 0.0; return; }
  vec3 p = f.xyz;
  if (f.w < 1.5) {
    p += vec3(sin(u_clock * 0.31 + i * 1.7) * 1.2, sin(u_clock * 0.47 + i * 2.3) * 0.4, cos(u_clock * 0.27 + i * 0.9) * 1.2);
    float ph = fract(u_clock * 0.22 + i * 0.137);
    v_b = smoothstep(0.0, 0.05, ph) * (1.0 - smoothstep(0.08, 0.2, ph));   // a slow pulse, then dark for a few seconds
  } else {
    p += vec3(sin(u_clock * 7.0 + i * 3.1), sin(u_clock * 9.0 + i * 1.3) * 0.6, cos(u_clock * 8.0 + i * 2.7)) * 0.35;
    v_b = 1.0;
  }
  v_k = f.w;
  vec4 mv = u_view * vec4(p, 1.0);
  gl_Position = u_proj * mv;
  gl_PointSize = clamp((f.w < 1.5 ? 0.09 : 0.02) * 900.0 / max(-mv.z, 0.2) * (u_viewH / 900.0), 1.0, 24.0);
}
)";
const char* FLY_FS = R"(
in float v_k;
in float v_b;
out vec4 fragColor;
void main(){
  float d = length(gl_PointCoord - 0.5) * 2.0;
  if (d > 1.0) discard;
  if (v_k < 1.5) { float g = (1.0 - d) * (1.0 - d) * v_b; fragColor = vec4(vec3(0.75, 1.0, 0.25) * g * 2.5, 0.0); }
  else fragColor = vec4(vec3(0.02) * 0.6, 0.6 * (1.0 - d));
}
)";
// v1.1.11: spider webs strung beside tree trunks: nearly invisible, until dew beads them at dawn or the sun is behind
const char* WEB_VS = R"(
uniform vec4 u_web[12];   // centre xyz, w = facing angle (w < -50: unused)
out vec2 v_q;
out vec3 v_wpos;
void main(){
  int q = gl_VertexID / 6, k = gl_VertexID % 6;
  vec2 c = k == 0 ? vec2(-1, -1) : k == 1 ? vec2(1, -1) : k == 2 ? vec2(1, 1) : k == 3 ? vec2(-1, -1) : k == 4 ? vec2(1, 1) : vec2(-1, 1);
  vec4 w = u_web[q];
  if (w.w < -50.0) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); v_q = vec2(9.0); v_wpos = vec3(0.0); return; }
  vec3 side = vec3(cos(w.w), 0.0, sin(w.w));
  vec3 p = w.xyz + side * c.x * 0.32 + vec3(0.0, c.y * 0.32, 0.0) + vec3(sin(u_clock * 1.3 + w.x) * 0.01 * u_windS);
  v_q = c; v_wpos = p;
  gl_Position = u_proj * u_view * vec4(p, 1.0);
}
)";
const char* WEB_FS = R"(
in vec2 v_q;
in vec3 v_wpos;
uniform float u_dew;
out vec4 fragColor;
void main(){
  float r = length(v_q);
  if (r > 1.0) discard;
  float a = atan(v_q.y, v_q.x);
  float spokes = 1.0 - smoothstep(0.0, 0.035 / max(r, 0.08), abs(fract(a / (2.0 * PI) * 14.0 + 0.5) - 0.5));
  float spiral = 1.0 - smoothstep(0.0, 0.09, abs(fract(r * 11.0 - a / (2.0 * PI)) - 0.5) * 2.0 * 0.5);
  float silk = max(spokes, spiral * smoothstep(0.1, 0.2, r)) * (1.0 - smoothstep(0.85, 1.0, r));
  vec3 V = normalize(u_camPos - v_wpos);
  float back = pow(max(dot(-V, u_sunDir.xyz), 0.0), 6.0) * smoothstep(0.0, 0.15, u_sunDir.y);   // the sun behind it lights every thread
  float beads = step(0.82, h21(floor(v_q * 60.0))) * u_dew;
  float k = silk * (0.05 + 0.9 * back + 0.6 * u_dew) + beads * silk * 1.5;
  float fade = 1.0 - smoothstep(10.0, 22.0, length(u_camPos - v_wpos));
  fragColor = vec4(mix(vec3(0.8, 0.85, 0.9), u_sunCol.rgb, 0.4) * k * fade * (0.25 + 0.75 * u_daylight), 0.0);
}
)";


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
  c = mix(c, fogTint(v_wpos), fogObject(v_wpos, v_depth) * u_fog);
  fragColor = vec4(c, u_color.a);
}
)";

const char* GLOW_VS = R"(
in vec4 a_pos;
out float v_kind;
void main(){
  vec4 mv = u_view * vec4(a_pos.xyz, 1.0);
  float size = abs(a_pos.w);
  v_kind = a_pos.w < 0.0 ? 1.0 : a_pos.w >= 100.0 ? 2.0 : 0.0;
  if (v_kind > 1.5) size -= 100.0;
  float ps = v_kind > 1.5 ? clamp(size * u_viewH * 0.55 / -mv.z, 2.0, 900.0)
           : v_kind > 0.5 ? clamp(size * u_viewH * 0.55 / -mv.z, 1.0, 400.0) : clamp(size * u_viewH * 0.55 / -mv.z, u_viewH * 0.009, 220.0);
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
  if (v_kind > 1.5) {
    // light scattered in the haze around a fire or lamp: a wide, soft warm bloom that lights the air nearby
    float a = exp(-d * d * 3.2) * (1.0 - d);
    float thick = 1.0 + 1.2 * u_rain + 1.4 * u_volFog;   // more mist in the air: more light caught in it
    float fl = 0.9 + 0.1 * sin(u_clock * 7.3 + gl_FragCoord.x * 0.001) * sin(u_clock * 3.1);
    fragColor = vec4(vec3(1.0, 0.6, 0.28) * a * 0.24 * thick * fl * max(u_nightAmt, 0.25 * u_volFog), 1.0);
    return;
  }
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

const char* FLAME_VS = R"(
uniform vec4 u_place;
uniform float u_layer;
out vec2 v_uv;
out float v_depth;
out vec3 v_wpos;
void main(){
  vec2 q = vec2((gl_VertexID == 1 || gl_VertexID == 2 || gl_VertexID == 4) ? 1.0 : 0.0, (gl_VertexID >= 2 && gl_VertexID != 3) ? 1.0 : 0.0);
  if (gl_VertexID == 5) q = vec2(0.0, 1.0);
  v_uv = q;
  vec3 right = normalize(vec3(u_view[0][0], u_view[1][0], u_view[2][0]));
  vec3 fwd = normalize(vec3(u_view[0][2], 0.0, u_view[2][2]));
  float s = u_place.w * (1.0 - 0.18 * u_layer);
  vec3 p = u_place.xyz + right * ((q.x - 0.5) * 1.15 * s + (u_layer - 1.0) * 0.12 * s) + vec3(0.0, q.y * 2.0 * s - 0.1 * s, 0.0) + fwd * (u_layer - 1.0) * 0.08 * s;
  v_wpos = p;
  vec4 vp = u_view * vec4(p, 1.0);
  v_depth = -vp.z;
  gl_Position = u_proj * vp;
}
)";
const char* FLAME_FS = R"(
in vec2 v_uv;
in float v_depth;
in vec3 v_wpos;
uniform float u_layer;
uniform float u_seed;
out vec4 fragColor;
void main(){
  vec2 uv = v_uv;
  float t = u_clock * (1.6 + 0.25 * u_layer) + u_seed;
  // rising turbulence, stronger toward the tips
  float n1 = vnoise(vec2(uv.x * 4.0 + u_seed, uv.y * 3.0 - t * 2.2));
  float n2 = vnoise(vec2(uv.x * 9.0 - u_seed, uv.y * 7.0 - t * 3.7));
  float turb = (n1 * 0.65 + n2 * 0.35 - 0.5);
  float x = uv.x - 0.5 + turb * 0.35 * uv.y + sin(t * 1.7 + uv.y * 5.0) * 0.04 * uv.y;
  // teardrop: wide base, narrowing tongue
  float width = 0.42 * pow(1.0 - uv.y, 0.75) * (0.75 + 0.25 * smoothstep(0.0, 0.25, uv.y));
  float body = 1.0 - smoothstep(width * 0.55, width, abs(x));
  float top = 1.0 - smoothstep(0.55 + 0.3 * n1, 1.0, uv.y);
  float a = body * top * smoothstep(0.0, 0.06, uv.y);
  if (a < 0.02) discard;
  float core = (1.0 - smoothstep(0.0, width * 0.55, abs(x))) * (1.0 - smoothstep(0.1, 0.65, uv.y));
  vec3 red = vec3(0.85, 0.16, 0.02), orange = vec3(1.0, 0.45, 0.06), yellow = vec3(1.0, 0.82, 0.32), white = vec3(1.0, 0.96, 0.82);
  vec3 c = mix(red, orange, smoothstep(0.1, 0.5, a));
  c = mix(c, yellow, smoothstep(0.45, 0.85, a) * (1.0 - uv.y * 0.6));
  c = mix(c, white, core * 0.85);
  float lum = mix(1.2, 2.4, core) * (0.85 + 0.3 * n2);
  if (u_layer > 1.5) { c = mix(c, red, 0.5); lum *= 0.6; }
  fragColor = vec4(c * lum * a, 1.0);
}
)";
GLuint g_flameVao = 0;

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

// v1.1.3: breath vapour: soft little clouds puffed out in the cold (the player, and animals nearby)
const char* PUFF_VS = R"(
uniform vec4 u_puffs[32];   // xyz, w = age 0..1 (<0 unused)
uniform float u_psize[32];   // v1.1.10: size scale (1 breath, ~6 river mist)
out float v_a;
out float v_big;
void main(){
  vec4 p = u_puffs[gl_VertexID];
  if (p.w < 0.0) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); gl_PointSize = 0.0; v_a = 0.0; return; }
  vec4 mv = u_view * vec4(p.xyz, 1.0);
  gl_Position = u_proj * mv;
  float sz = u_psize[gl_VertexID];
  gl_PointSize = min(512.0, (0.12 + 0.45 * p.w) * sz * 420.0 / max(-mv.z, 0.15) * (u_viewH / 900.0));
  v_a = sin(3.14159 * min(1.0, p.w * 1.6)) * (1.0 - p.w) * (sz > 1.5 ? 0.55 : 1.0);
  v_big = sz > 1.5 ? 1.0 : 0.0;
}
)";
const char* PUFF_FS = R"(
in float v_a;
in float v_big;
out vec4 fragColor;
void main(){
  vec2 q = gl_PointCoord * 2.0 - 1.0;
  float r = dot(q, q);
  if (r > 1.0) discard;
  if (v_big > 0.5) r = clamp(r + (vnoise(gl_PointCoord * 4.0 + v_a * 3.0) - 0.5) * 0.6, 0.0, 1.0);   // wispy edges
  float lum = 0.55 + 0.45 * u_daylight;
  fragColor = vec4(vec3(0.9, 0.92, 0.95) * lum, v_a * 0.22 * (1.0 - r) * (1.0 - r));
}
)";
// v1.1.4: spindrift: wind tearing long plumes of snow off the highest peaks, streaming downwind
const char* PLUME_VS = R"(
uniform vec4 u_peaks[12];   // xyz top, w size
uniform float u_str;
out vec2 v_uv;
out float v_a;
void main(){
  int q = gl_VertexID / 6, k = gl_VertexID % 6;
  vec2 c = k == 0 ? vec2(0, 0) : k == 1 ? vec2(1, 0) : k == 2 ? vec2(1, 1) : k == 3 ? vec2(0, 0) : k == 4 ? vec2(1, 1) : vec2(0, 1);
  vec4 pk = u_peaks[q];
  vec2 wd2 = normalize(u_windDir + vec2(1e-4));
  vec3 down = normalize(vec3(wd2.x, -0.12, wd2.y));
  vec3 toCam = normalize(u_camPos - pk.xyz);
  vec3 side = normalize(cross(down, toCam));
  float L = pk.w * (2.6 + 0.6 * sin(u_clock * 0.05 + float(q)));
  vec3 p = pk.xyz + down * (c.x * L - pk.w * 0.1) + side * (c.y - 0.5) * pk.w * (0.25 + 0.9 * c.x) + vec3(0.0, pk.w * 0.05 * c.x, 0.0);
  v_uv = c; v_a = u_str * clamp(0.25 + 0.6 * u_windS, 0.0, 1.0) * step(0.0, pk.w);
  gl_Position = u_proj * u_view * vec4(p, 1.0);
}
)";
const char* PLUME_FS = R"(
in vec2 v_uv;
in float v_a;
out vec4 fragColor;
void main(){
  float x = v_uv.x, y = abs(v_uv.y - 0.5) * 2.0;
  float n = vnoise(vec2(x * 6.0 - u_clock * 0.4, v_uv.y * 3.0)) * 0.6 + vnoise(vec2(x * 15.0 - u_clock * 0.9, v_uv.y * 7.0)) * 0.4;
  float a = smoothstep(0.0, 0.08, x) * (1.0 - x) * (1.0 - y * y) * smoothstep(0.25, 0.75, n + (1.0 - x) * 0.3);
  vec3 col = mix(u_fogCol.rgb, vec3(0.95, 0.96, 1.0) * (0.4 + 0.6 * u_daylight), 0.7);
  fragColor = vec4(col, a * v_a * 0.8);
}
)";
// v1.1.4: heat shimmer: the air above a fire wobbles the view behind it (reads the half-res scene copy)
const char* SHIM_VS = R"(
uniform vec4 u_place;
out vec2 v_q;
out vec4 v_clip;
void main(){
  int k = gl_VertexID;
  vec2 c = k == 0 ? vec2(0, 0) : k == 1 ? vec2(1, 0) : k == 2 ? vec2(1, 1) : k == 3 ? vec2(0, 0) : k == 4 ? vec2(1, 1) : vec2(0, 1);
  vec3 toCam = u_camPos - u_place.xyz; toCam.y = 0.0; toCam = normalize(toCam + vec3(1e-4));
  vec3 side = vec3(-toCam.z, 0.0, toCam.x);
  vec3 p = u_place.xyz + side * (c.x - 0.5) * 1.6 * u_place.w + vec3(0.0, (0.6 + c.y * 2.8) * u_place.w, 0.0);
  v_q = c;
  gl_Position = u_proj * u_view * vec4(p, 1.0);
  v_clip = gl_Position;
}
)";
const char* SHIM_FS = R"(
in vec2 v_q;
in vec4 v_clip;
uniform sampler2D u_scene;
out vec4 fragColor;
void main(){
  vec2 uv = v_clip.xy / v_clip.w * 0.5 + 0.5;
  float m = sin(v_q.x * 3.14159) * smoothstep(0.0, 0.25, v_q.y) * (1.0 - v_q.y);
  vec2 off = vec2(vnoise(vec2(v_q.x * 7.0, v_q.y * 5.0 - u_clock * 3.0)) - 0.5, vnoise(vec2(v_q.x * 6.0 + 3.0, v_q.y * 4.0 - u_clock * 2.5)) - 0.5) * 0.012 * m;
  vec3 c = textureLod(u_scene, uv + off, 0.0).rgb;
  fragColor = vec4(c, m * 0.85);   // the copy is already tone-mapped
}
)";
struct Puff { Vec3 p, v; float t, life, size = 1; };
std::vector<Puff> g_puffs;
GLuint g_puffVao = 0;

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
void campfire(float x, float y, float z, float s) {
  if (render::sphereVisible({x, y + s, z}, s * 2 + 1)) g_fires.push_back({x, y, z, s});
  if (render::sphereVisible({x, y + 1.5f, z}, 14 * s + 1)) g_glows.insert(g_glows.end(), {x, y + 1.2f * s, z, 100.0f + 13.0f * s});  // firelight glowing in the haze
}
void halo(float x, float y, float z, float size) { if (render::sphereVisible({x, y, z}, size + 1)) g_glows.insert(g_glows.end(), {x, y, z, 100.0f + size}); }
void glow(float x, float y, float z, float size) { if (render::sphereVisible({x, y, z}, std::fabs(size) * 1.5f + 1)) g_glows.insert(g_glows.end(), {x, y, z, size}); }
void pool(float x, float y, float z, float r) { if (render::sphereVisible({x, y, z}, r + 1)) g_pools.insert(g_pools.end(), {x, y, z, r}); }
void breath(const Vec3& p, const Vec3& dir) {
  if (g_puffs.size() >= 32) return;
  g_puffs.push_back({p, dir * 0.45f + Vec3(0, 0.08f, 0), 0, 1.6f, 1});
}
void mist(const Vec3& p) {   // v1.1.10: a wisp of mist lifting off the river on a cold morning
  if (g_puffs.size() >= 32) return;
  g_puffs.push_back({p, Vec3(0, 0.22f, 0), 0, 7.0f, 6});
}
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
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_ONE, GL_ONE);
    if (!g_flameVao) glGenVertexArrays(1, &g_flameVao);
    Shader& fl = render::program("flame", FLAME_VS, FLAME_FS);
    glBindVertexArray(g_flameVao);
    for (auto& f : g_fires) {
      if (render::cam.pos.distanceTo({f.x, f.y, f.z}) > 260 || !render::sphereVisible({f.x, f.y + f.s, f.z}, 2 * f.s + 1)) continue;
      fl.set4f("u_place", f.x, f.y, f.z, f.s);
      fl.set1f("u_seed", f.x * 0.37f + f.z * 0.11f);
      for (int l = 2; l >= 0; l--) {  // outer red, mid orange, bright core
        fl.set1f("u_layer", (float)l);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        gfx::drawCalls++;
      }
    }
    glBlendFunc(GL_ONE, GL_ONE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    Shader& e = render::program("ember", EMBER_VS, EMBER_FS);
    glBindVertexArray(g_emberVao);
    for (auto& f : g_fires) {
      if (render::cam.pos.distanceTo({f.x, f.y, f.z}) > 160 || !render::sphereVisible({f.x, f.y + 2 * f.s, f.z}, 3 * f.s + 1)) continue;
      e.set4f("u_place", f.x, f.y, f.z, f.s);
      glDrawArrays(GL_POINTS, 0, 24);
      gfx::drawCalls++;
    }
    if (unsigned st = water::reflection()) {   // heat shimmer above fires within 40 m (needs the water-reflection copy)
      static GLuint vao = 0;
      if (!vao) glGenVertexArrays(1, &vao);
      Shader& sh = render::program("shimmer", SHIM_VS, SHIM_FS);
      glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, st); glActiveTexture(GL_TEXTURE0);
      sh.set1i("u_scene", 5);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBindVertexArray(vao);
      for (auto& f : g_fires) {
        if (render::cam.pos.distanceTo({f.x, f.y, f.z}) > 40) continue;
        sh.set4f("u_place", f.x, f.y, f.z, f.s);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        gfx::drawCalls++;
      }
      glBlendFunc(GL_ONE, GL_ONE);
    }
    for (auto& f : g_fires) {
      float night = E.isNight ? 1.5f : 1;
      g_glows.insert(g_glows.end(), {f.x, f.y + 0.8f * f.s, f.z, -(3.0f + std::sin(E.clock * 6) * 0.25f) * night * f.s});
    }
    glDepthMask(GL_TRUE);
  }
  {  // breath puffs: drift, slow, rise and fade
    static double last = 0;
    float dt = (float)std::min(0.1, std::max(0.0, E.clock - last)); last = E.clock;
    for (auto& q : g_puffs) { q.t += dt; q.p = q.p + q.v * dt; if (q.size < 1.5f) q.v = q.v * (1.0f - 1.5f * dt) + Vec3(0, 0.12f * dt, 0); }
    g_puffs.erase(std::remove_if(g_puffs.begin(), g_puffs.end(), [](const Puff& q) { return q.t >= q.life; }), g_puffs.end());
    if (!g_puffs.empty()) {
      if (!g_puffVao) glGenVertexArrays(1, &g_puffVao);
      float u[32 * 4], sz[32];
      for (int i = 0; i < 32; i++) {
        sz[i] = i < (int)g_puffs.size() ? g_puffs[i].size : 1;
        if (i < (int)g_puffs.size()) { u[i * 4] = g_puffs[i].p.x; u[i * 4 + 1] = g_puffs[i].p.y; u[i * 4 + 2] = g_puffs[i].p.z; u[i * 4 + 3] = g_puffs[i].t / g_puffs[i].life; }
        else { u[i * 4] = u[i * 4 + 1] = u[i * 4 + 2] = 0; u[i * 4 + 3] = -1; }
      }
      Shader& ps = render::program("puff", PUFF_VS, PUFF_FS);
      glUniform4fv(glGetUniformLocation(ps.prog, "u_puffs"), 32, u);
      glUniform1fv(glGetUniformLocation(ps.prog, "u_psize"), 32, sz);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(GL_FALSE);
      glEnable(GL_PROGRAM_POINT_SIZE);
      glBindVertexArray(g_puffVao);
      glDrawArrays(GL_POINTS, 0, (GLsizei)g_puffs.size());
      gfx::drawCalls++;
      glDepthMask(GL_TRUE);
    }
  }
  if (!g_fires.empty()) {  // v1.1.11: smoke above each fire
    static GLuint vao = 0;
    if (!vao) glGenVertexArrays(1, &vao);
    Shader& sm = render::program("smoke", SMOKE_VS, SMOKE_FS);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glBindVertexArray(vao);
    for (auto& f : g_fires) {
      if (render::cam.pos.distanceTo({f.x, f.y, f.z}) > 200) continue;
      sm.set4f("u_place", f.x, f.y, f.z, f.s);
      glDrawArrays(GL_POINTS, 0, 22);
      gfx::drawCalls++;
    }
    glDepthMask(GL_TRUE);
  }
  {  // v1.1.11: fireflies (warm, dry nights, in the grass and at the forest edge) and midges (over water, at dusk)
    const Vec3& P = E.player.pos;
    float warm = std::max(0.0f, 1.0f - season.snow * 2 - season.autumn * 0.8f);
    float dry = 1 - std::min(1.0f, sky_rain() * 3);
    float ffK = warm * dry * sstep(1 - E.daylight, 0.6f, 0.9f);
    float dusk = sstep(E.time, 17.0f, 18.0f) * (1 - sstep(E.time, 19.5f, 20.5f)) + sstep(E.time, 5.5f, 6.0f) * (1 - sstep(E.time, 7.0f, 8.0f));
    float mgK = warm * dry * dusk;
    float u[64 * 4]; int n = 0;
    auto hsh = [](int a, int b) { unsigned h = (unsigned)a * 73856093u ^ (unsigned)b * 19349663u; h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return (h & 0xffff) / 65535.0f; };
    if (ffK > 0.02f) {
      int cx = (int)std::floor(P.x / 4), cz = (int)std::floor(P.z / 4);
      for (int i = -6; i <= 6 && n < 48; i++)
        for (int j = -6; j <= 6 && n < 48; j++) {
          float h = hsh(cx + i, cz + j);
          if (h > ffK * 0.35f) continue;
          float x = (cx + i + hsh(cx + i + 7, cz + j)) * 4, z = (cz + j + hsh(cx + i, cz + j + 7)) * 4;
          if (world::waterHeightAt(x, z) > world::getTerrainHeight(x, z)) continue;
          float y = world::getTerrainHeight(x, z) + 0.4f + 1.4f * hsh(cx + i + 3, cz + j + 5);
          u[n * 4] = x; u[n * 4 + 1] = y; u[n * 4 + 2] = z; u[n * 4 + 3] = 1; n++;
        }
    }
    if (mgK > 0.05f) {
      int cx = (int)std::floor(P.x / 9), cz = (int)std::floor(P.z / 9);
      for (int i = -2; i <= 2; i++)
        for (int j = -2; j <= 2; j++) {
          if (n > 64 - 12) break;
          float x = (cx + i + 0.5f) * 9, z = (cz + j + 0.5f) * 9;
          RiverHit rh = world::riverQuery(x, z);
          if (!(rh.d < rh.hw + 3.0f) || hsh(cx + i, cz + j) > mgK * 0.8f) continue;
          float y = std::max(rh.wy, world::getTerrainHeight(x, z)) + 1.3f;
          for (int k = 0; k < 12; k++) { u[n * 4] = x; u[n * 4 + 1] = y; u[n * 4 + 2] = z; u[n * 4 + 3] = 2; n++; }
        }
    }
    if (n) {
      for (int i = n; i < 64; i++) u[i * 4 + 3] = 0;
      static GLuint vao = 0;
      if (!vao) glGenVertexArrays(1, &vao);
      Shader& fs = render::program("flies", FLY_VS, FLY_FS);
      glUniform4fv(glGetUniformLocation(fs.prog, "u_fly"), 64, u);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(GL_FALSE);
      glEnable(GL_PROGRAM_POINT_SIZE);
      glBindVertexArray(vao);
      glDrawArrays(GL_POINTS, 0, n);
      gfx::drawCalls++;
      glDepthMask(GL_TRUE);
    }
  }
  {  // v1.1.11: spider webs beside the nearest trunks (about a third of trees carry one)
    const Vec3& P = E.player.pos;
    static std::vector<float> t;
    static float lx = 1e9, lz = 1e9;
    if (std::fabs(P.x - lx) + std::fabs(P.z - lz) > 3) { forest::nearTrees(P.x, P.z, 20, t, 24); lx = P.x; lz = P.z; }
    float u[12 * 4]; int n = 0;
    if (season.snow < 0.5f)
      for (size_t i = 0; i + 3 < t.size() && n < 12; i += 4) {
        float x = t[i], z = t[i + 2];
        unsigned h = (unsigned)(int)std::floor(x * 3.1f) * 2654435761u ^ (unsigned)(int)std::floor(z * 5.7f) * 40503u;
        if ((h % 100) > 35) continue;
        float ang = (h % 628) / 100.0f, off = 0.55f + 0.25f * t[i + 3];
        float wx = x + std::cos(ang) * off, wz = z + std::sin(ang) * off;
        u[n * 4] = wx; u[n * 4 + 1] = world::getTerrainHeight(wx, wz) + 0.9f + (h % 7) * 0.1f; u[n * 4 + 2] = wz; u[n * 4 + 3] = ang + 1.5708f; n++;
      }
    if (n) {
      for (int i = n; i < 12; i++) u[i * 4 + 3] = -99;
      static GLuint vao = 0;
      if (!vao) glGenVertexArrays(1, &vao);
      Shader& ws = render::program("webs", WEB_VS, WEB_FS);
      glUniform4fv(glGetUniformLocation(ws.prog, "u_web"), 12, u);
      float dew = sstep(E.time, 4.5f, 5.5f) * (1 - sstep(E.time, 8.5f, 10.0f)) * (1 - std::min(1.0f, season.snow * 2));
      ws.set1f("u_dew", std::max(dew, std::min(1.0f, sky_rain() * 2)));
      glBlendFunc(GL_ONE, GL_ONE);
      glDepthMask(GL_FALSE);
      glBindVertexArray(vao);
      glDrawArrays(GL_TRIANGLES, 0, n * 6);
      gfx::drawCalls++;
      glDepthMask(GL_TRUE);
    }
  }
  {  // spindrift plumes (windy, clear-ish days; strongest in winter)
    float str = (0.6f + 0.4f * season.snow) * E.daylight;
    if (str > 0.05f && !world::BACKDROP_PEAKS.empty()) {
      static GLuint vao = 0;
      if (!vao) glGenVertexArrays(1, &vao);
      float u[12 * 4];
      int n = 0;
      for (auto& q : world::BACKDROP_PEAKS) { if (n >= 12) break; u[n * 4] = q.x; u[n * 4 + 1] = q.y - 8; u[n * 4 + 2] = q.z; u[n * 4 + 3] = 70.0f + 0.1f * (q.y - 300); n++; }
      Shader& ps = render::program("plume", PLUME_VS, PLUME_FS);
      glUniform4fv(glGetUniformLocation(ps.prog, "u_peaks"), 12, u);
      ps.set1f("u_str", str);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(GL_FALSE);
      glBindVertexArray(vao);
      glDrawArrays(GL_TRIANGLES, 0, n * 6);
      gfx::drawCalls++;
      glDepthMask(GL_TRUE);
    }
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
