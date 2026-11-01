#include "../audio/audio.h"
#include <algorithm>
#include "sky.h"
#include "render.h"
#include "../game/engine.h"
#include "../game/season.h"
#include "../world/terrain.h"
#include "../world/forest.h"
#include "../platform/platform.h"
#include <vector>
#include <cstdlib>
#include "../core/noise.h"
#include "terrain_render.h"
#include "../game/events.h"

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
uniform sampler2D u_cloudTex;
uniform vec3 u_top;
uniform vec3 u_bottom;
uniform vec3 u_sun;
uniform vec3 u_moon;
uniform vec3 u_sunTint;
uniform float u_cover;
uniform float u_storm;   // 0..1 rain strength
uniform float u_flash;   // lightning
out vec4 fragColor;
void main(){
  vec3 d = normalize(v_dir);
  float h = max(d.y, 0.0);
  vec3 col = mix(u_bottom, u_top, pow(h, 0.55));
  float sd = max(dot(d, u_sun), 0.0);
  float hz = smoothstep(-0.02, 0.03, d.y);
  float day = u_daylight;
  float sunVis = 1.0 - smoothstep(0.05, 0.5, u_storm);  // rain clouds hide the sun and its glow
  col += u_sunTint * (pow(sd, 1400.0) * 0.6 + pow(sd, 90.0) * 0.35 + pow(sd, 10.0) * 0.2) * day * sunVis;
  vec3 disc = mix(vec3(1.0, 0.97, 0.88) * 1.6 + u_sunTint * 0.2, u_sunTint * 1.45, 1.0 - smoothstep(0.03, 0.4, u_sun.y));
  col = mix(col, disc, smoothstep(0.99955, 0.99972, sd) * hz * sunVis);
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
    col = mix(col, mc * mix(0.75, 1.15, night), disc * mix(0.35, 1.0, night) * sunVis);
  }
  col += vec3(0.55, 0.65, 0.85) * (pow(max(md, 0.0), 400.0) * 0.22 + pow(max(md, 0.0), 40.0) * 0.05) * night * hz;
  if (d.y > 0.0) {
    vec2 uv = d.xz / (d.y + 0.15) * 1.35 + vec2(u_clock * 0.004, u_clock * 0.0017);
    // pre-baked fbm: 2 texture reads instead of 7 noise taps per sky pixel
    float n = texture(u_cloudTex, uv * (1.0 / 32.0)).r;
    float c = smoothstep(0.66 - u_cover * 0.3, 0.8 - u_cover * 0.2, n);
    float n2 = texture(u_cloudTex, (uv + u_sun.xz * 0.12) * (1.0 / 32.0)).g;
    float lit = clamp(0.7 + (n - n2) * 3.0, 0.15, 1.25);
    float mglow = pow(max(md, 0.0), 6.0);
    float cday = smoothstep(0.0, 0.3, day);  // clouds stay lit until late dusk
    vec3 dark = mix(vec3(0.025, 0.03, 0.05), vec3(0.52, 0.56, 0.64), cday);
    vec3 bright = mix(vec3(0.07, 0.085, 0.12) + vec3(0.16, 0.18, 0.22) * mglow, mix(u_sunTint, vec3(1.0), 0.55), cday);
    vec3 cc = mix(dark, bright, clamp(lit, 0.0, 1.0)) + u_sunTint * pow(sd, 6.0) * (1.0 - c) * 0.6 * day * sunVis + vec3(0.35, 0.42, 0.55) * pow(max(md, 0.0), 160.0) * night * 0.35;
    float fade = smoothstep(0.0, 0.07, d.y);
    cc *= mix(1.0, 0.42, u_storm);                  // rain clouds are heavy and dark
    col = mix(col, cc, c * fade * 0.94);
    if (u_storm > 0.02) {
      // low ragged storm layer racing under the main deck
      vec2 uv2 = d.xz / (d.y + 0.08) * 0.8 + vec2(u_clock * 0.02, u_clock * 0.011);
      float s2 = smoothstep(0.42, 0.7, texture(u_cloudTex, uv2 * (1.0 / 32.0) + 0.37).g) * u_storm;
      vec3 sc2 = mix(vec3(0.03, 0.035, 0.045), vec3(0.24, 0.26, 0.29), smoothstep(0.0, 0.3, day));
      col = mix(col, sc2, s2 * fade * 0.85);
    }
  }
  col += vec3(0.7, 0.75, 0.9) * u_flash * (0.6 + 0.4 * max(d.y, 0.0));
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
  float n = vnoise(v_uv * 1.6 + vec2(v_seed * 17.0 + u_clock * 0.01, v_seed * 9.0)) * 0.75 + 0.18;
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
uniform float u_blow;
out float v_a;
void main(){
  vec3 box = vec3(40.0, 20.0, 40.0);
  vec3 sd = a_pos;
  vec3 p = sd * box;
  p.y -= u_clock * (1.0 + sd.x * 0.7) * (1.0 + u_blow);
  p.xz += u_windDir * u_clock * u_blow * (7.0 + sd.y * 4.0);  // blizzard: snow driven sideways
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
const char* RAIN_VS = R"(
uniform float u_rainAmt;
out float v_a;
float hh(float n){ return fract(sin(n * 12.9898) * 43758.5453); }
void main(){
  int id = gl_VertexID / 2, end = gl_VertexID - id * 2;
  vec3 sd = vec3(hh(float(id)), hh(float(id) + 17.3), hh(float(id) + 41.7));
  vec3 box = vec3(36.0, 24.0, 36.0);
  vec3 p = sd * box;
  p.y -= u_clock * 16.0 * (0.9 + sd.x * 0.3);
  vec3 rel = mod(p - u_camPos + box * 0.5, box) - box * 0.5;
  vec3 slant = vec3(u_windDir.x, 0.0, u_windDir.y) * 0.25 * u_windS;
  vec3 wp = u_camPos + rel + (end == 1 ? vec3(0.0, 0.55, 0.0) - slant : vec3(0.0));
  gl_Position = u_viewProj * vec4(wp, 1.0);
  float on = step(sd.z, u_rainAmt);
  v_a = on * (1.0 - smoothstep(10.0, 18.0, length(rel))) * (end == 1 ? 0.0 : 1.0);
}
)";
const char* RAIN_FS = R"(
in float v_a;
out vec4 fragColor;
void main(){ fragColor = vec4(vec3(0.75, 0.8, 0.88) * (0.3 + 0.7 * u_daylight), v_a * 0.35); }
)";
// ambient life: fireflies (summer nights), butterflies (spring/summer days), pollen motes in sunlight
const char* LIFE_VS = R"(
uniform sampler2D u_height;
uniform vec4 u_grid;
uniform vec4 u_life;   // x fireflies, y butterflies, z pollen
out vec4 v_c;
out float v_kind;
out float v_flap;
float hh(float n){ return fract(sin(n * 12.9898) * 43758.5453); }
float hAt(vec2 w){ vec2 g = (w - u_grid.xx) / u_grid.y; ivec2 c = clamp(ivec2(g + 0.5), ivec2(0), ivec2(int(u_grid.z) - 1)); return texelFetch(u_height, c, 0).r; }
void main(){
  int id = gl_VertexID;
  float kind = id < 300 ? 0.0 : id < 340 ? 1.0 : 2.0;
  float f = float(id);
  vec3 sd = vec3(hh(f), hh(f + 17.3), hh(f + 41.7));
  float box = kind == 1.0 ? 40.0 : 30.0;
  vec2 base = sd.xy * box;
  float t = u_clock;
  // gentle wandering paths
  base += vec2(sin(t * (0.2 + sd.z * 0.3) + f), cos(t * (0.17 + sd.x * 0.25) + f * 1.3)) * (kind == 2.0 ? 1.5 : 3.0);
  vec2 rel = mod(base - u_camPos.xz + box * 0.5, box) - box * 0.5;
  vec2 w = u_camPos.xz + rel;
  float gy = hAt(w);
  float hgt = kind == 0.0 ? 0.4 + sd.z * 2.2 + sin(t * 0.9 + f) * 0.3 : kind == 1.0 ? 0.5 + sd.z * 1.4 + abs(sin(t * 2.2 + f)) * 0.35 : 0.6 + sd.z * 3.0 + sin(t * 0.3 + f) * 0.5;
  vec3 wp = vec3(w.x, gy + hgt, w.y);
  vec4 vp = u_view * vec4(wp, 1.0);
  gl_Position = u_proj * vp;
  float amt = kind == 0.0 ? u_life.x : kind == 1.0 ? u_life.y : u_life.z;
  float on = step(sd.z * 0.999, amt) * (1.0 - smoothstep(box * 0.35, box * 0.5, length(rel)));
  float blink = kind == 0.0 ? smoothstep(0.55, 1.0, sin(t * (1.3 + sd.x) + f * 3.1)) : 1.0;
  vec3 col = kind == 0.0 ? vec3(0.75, 1.0, 0.35) : kind == 1.0 ? mix(vec3(0.95, 0.75, 0.2), mix(vec3(0.9, 0.95, 1.0), vec3(0.65, 0.4, 0.9), step(0.5, sd.y)), step(0.33, sd.x)) : vec3(1.0, 0.96, 0.82);
  v_c = vec4(col, on * blink);
  v_kind = kind;
  v_flap = abs(sin(t * 14.0 + f));
  float px = kind == 0.0 ? 0.09 : kind == 1.0 ? 0.16 : 0.025;
  gl_PointSize = on * clamp(px * u_viewH / max(-vp.z, 0.1), 1.0, 40.0);
}
)";
const char* LIFE_FS = R"(
in vec4 v_c;
in float v_kind;
in float v_flap;
out vec4 fragColor;
void main(){
  vec2 q = gl_PointCoord * 2.0 - 1.0;
  if (v_kind == 1.0) {
    // butterfly: thin dark body, larger forewings and smaller hindwings, dark wing edges and an eye spot,
    // wings narrowing as they fold through each flap
    float body = step(abs(q.x), 0.06) * step(abs(q.y), 0.55);
    float fx = abs(q.x) / max(0.12, v_flap);
    vec2 fw = vec2(fx - 0.5, q.y + 0.18);            // forewing (upper)
    vec2 hw = vec2(fx - 0.38, q.y - 0.38);           // hindwing (lower)
    float fore = length(fw * vec2(1.0, 1.35)) - 0.48;
    float hind = length(hw * vec2(1.0, 1.2)) - 0.33;
    float wing = min(fore, hind);
    if (wing > 0.0 && body < 0.5) discard;
    vec3 c = v_c.rgb;
    float edge = smoothstep(-0.1, -0.02, wing);
    c = mix(c, vec3(0.08, 0.06, 0.05), edge * 0.85);
    float eye = 1.0 - smoothstep(0.06, 0.1, length(fw - vec2(0.12, -0.08)));
    c = mix(c, vec3(0.1, 0.08, 0.06), eye);
    c = mix(c, vec3(0.06, 0.05, 0.04), body);
    fragColor = vec4(outColor(c * (0.35 + 0.65 * u_daylight) * 0.9), v_c.a);
    return;
  }
  float d = length(q);
  if (d > 1.0) discard;
  float a = v_kind == 0.0 ? pow(1.0 - d, 2.0) : (1.0 - d) * 0.5 * smoothstep(0.1, 0.5, u_daylight);
  fragColor = vec4(v_c.rgb * (v_kind == 0.0 ? 1.6 : 1.0), v_c.a * a);
}
)";
const char* BIRD_VS = R"(
uniform vec4 u_birds[24];   // xyz position, w flap phase (<0 = unused)
out float v_flap;
out float v_on;
void main(){
  vec4 b = u_birds[gl_VertexID];
  v_on = b.w < 0.0 ? 0.0 : 1.0;
  v_flap = abs(sin(b.w));
  vec4 vp = u_view * vec4(b.xyz, 1.0);
  gl_Position = u_proj * vp;
  gl_PointSize = v_on * clamp(0.45 * u_viewH / max(-vp.z, 0.1), 2.0, 48.0);
}
)";
const char* BIRD_FS = R"(
in float v_flap;
in float v_on;
out vec4 fragColor;
void main(){
  if (v_on < 0.5) discard;
  vec2 q = gl_PointCoord * 2.0 - 1.0;
  // small dark bird seen from below/side: body plus two wings sweeping up and down
  float wingY = mix(0.35, -0.35, v_flap);
  float ax = abs(q.x);
  float wing = abs(q.y - (0.0 + (wingY) * ax)) - 0.09 * (1.0 - ax);
  float body = length(q * vec2(2.8, 1.6)) - 0.35;
  if (min(wing, body) > 0.0 || ax > 0.95) discard;
  fragColor = vec4(outColor(vec3(0.05, 0.045, 0.04) * (0.4 + 0.6 * u_daylight)), 1.0);
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
  gl_PointSize = on * clamp((0.13 + sd.x * 0.06) * u_viewH / max(-mv.z, 0.1), 1.5, 26.0);
}
)";
const char* LEAF_FS = R"(
in float v_a;
in float v_rot;
in vec3 v_seed;
uniform float u_petal;
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
  vec2 q = gl_PointCoord - 0.5;
  float c = cos(v_rot), s = sin(v_rot);
  q = vec2(c * q.x - s * q.y, s * q.x + c * q.y);
  float tumble = sin(v_rot * 0.7);
  float flip = 0.3 + 0.7 * abs(tumble);
  q.x /= flip;
  q *= 2.1;
  float kind = fract(v_seed.x * 7.3 + v_seed.z * 3.1);
  float inside = u_petal > 0.5 ? 0.9 - length(q * vec2(1.4, 1.0)) : leafShape(q, kind);
  float stem = u_petal > 0.5 ? 0.0 : step(abs(q.x), 0.03) * step(q.y, -0.9) * step(-1.15, q.y);
  if ((inside < 0.0 && stem < 0.5) || v_a < 0.01) discard;
  vec3 au = mix(vec3(0.95, 0.38, 0.05), vec3(0.72, 0.12, 0.04), smoothstep(0.3, 0.7, v_seed.z));
  au = mix(au, vec3(0.96, 0.7, 0.12), step(0.78, v_seed.x));
  au = mix(au, vec3(0.45, 0.24, 0.08), step(0.9, fract(v_seed.y * 11.0)));
  vec3 pe = mix(vec3(1.0, 0.75, 0.85), vec3(1.0, 0.95, 0.97), v_seed.z);
  vec3 col = mix(au, pe, u_petal);
  float rib = 1.0 - 0.25 * smoothstep(0.035, 0.0, abs(q.x)) * (1.0 - u_petal);
  float vt = q.y * 4.0 - abs(q.x) * 3.0;
  float vein = 1.0 - 0.1 * smoothstep(0.1, 0.0, abs(fract(vt) - 0.5)) * (1.0 - u_petal);
  float edge = mix(0.72, 1.0, smoothstep(0.0, 0.05, inside));
  col *= rib * vein * edge;
  col *= tumble > 0.0 ? 1.0 : 0.78;                      // paler underside / shaded back
  col *= mix(vec3(1.0), u_sunCol.rgb * 0.6 + 0.5, 0.35); // pick up the light colour
  col *= (0.7 + 0.3 * flip) * (0.3 + 0.7 * u_daylight);
  if (stem > 0.5) col = vec3(0.3, 0.18, 0.08) * (0.3 + 0.7 * u_daylight);
  fragColor = vec4(col, v_a);
}
)";

struct SkyCols { Color zen, hor; };
const Color NIGHT = Color::hex("0b1430"), NIGHTH = Color::hex("1c2846"), DAWN = Color::hex("f08a4b"), DAWNH = Color::hex("fbc99a");
const Color DAY = Color::hex("78bdf0"), DAYH = Color::hex("d3e8ee"), GOLD = Color::hex("f6a35c"), GOLDH = Color::hex("fde3a7");
const Color SUN = Color::hex("fff1d6"), SUNLOW = Color::hex("ffc58a"), SUNSET = Color::hex("ff7a2a"), MOON = Color::hex("a8c0ee");
const Color WINTER_H = Color::hex("e4eaf0"), WINTER_Z = Color::hex("9fb6cc"), AUTUMN_H = Color::hex("f3d9b5");
const Color G_SUMMER = Color::hex("7a9a45"), G_AUTUMN = Color::hex("9a7038"), G_WINTER = Color::hex("c9d3dc"), HEMI_SKY = Color::hex("d6ecff");
const float SHADOW_HALF = 32;

Color g_zen, g_hor, g_sunTint;
Vec3 g_sunDir, g_moonDir, g_lightDir;
Mat4 g_shadowMat;
Vec3 g_shadowC, g_lastShadowC{1e9f, 0, 0};
int g_shadowTick = 0;
bool g_shadowUpdate = true, g_staticDirty = true;
float g_shadowK = 1.0f;
Mat4 g_builtMat, g_prevMat;
float g_mixT = 1.0f;
bool g_haveBuilt = false;
float g_cover = 0.4f;
float g_rain = 0, g_wet = 0, g_flash = 0, g_thunderT = 0, g_blizzard = 0, g_fogBank = 0;
struct Bird { Vec3 p, v; float ph, life; };
std::vector<Bird> g_birds;
float g_birdCheck = 0;
GLuint g_rainVao = 0;
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
  // seasonal day length: summer sun rises ~1h earlier, winter ~0.5h later
  static float sOff = 1e9f;
  // day-length factor: summer sunrise ~4:55 / sunset ~19:05, winter ~6:30 / 17:30
  float tOff = season.index == 1 ? 0.845f : season.index == 3 ? 1.09f : season.index == 0 ? 0.93f : 1.0f;
  sOff = (sOff > 1e8f || std::fabs(tOff - sOff) > 0.05f) ? tOff : sOff + (tOff - sOff) * std::min(1.0f, dt * 0.2f);
  float hr = E.time;
  float h = hr >= 0 ? 12.0f + (hr - 12.0f) * sOff : hr;
  h = std::fmod(h + 48.0f, 24.0f);
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
  // golden hour: as the sun nears the horizon its light turns orange and dims, gradually
  float sunElev = g_sunDir.y;
  float dusk = 1.0f - sstep(sunElev, 0.02f, 0.42f);
  float sunI = 2.1f * std::pow(daylight, 1.15f) * (1 - 0.18f * snow) * (1.0f - 0.28f * dusk * dusk);
  float moonI = 0.5f * moonUp * (1 - daylight);  // brighter blue-silver moonlight, casts soft shadows
  {
    // showers: deterministic weather over game hours; spring/summer/autumn only, ground stays wet for a while after
    float hrs = (season.start * 7 + season.day) * 24.0f + E.time;
    float n = valueNoise(hrs / 9.0f, 3.7f, 913);
    float want = sstep(n, 0.68f, 0.8f) * (1 - sstep(season.snow + season.fall, 0.15f, 0.4f));
    if (g_settings.weather == 1) want = 1 - sstep(season.snow + season.fall, 0.15f, 0.4f);  // config: always rain (snow still wins)
    else if (g_settings.weather == 2) want = 0;
    if (std::getenv("HW_RAIN") || (events::forceRain() && g_settings.weather != 2)) want = 1 - sstep(season.snow + season.fall, 0.15f, 0.4f) * (events::forceRain() ? 0 : 1);  // storm day event
    g_rain += (want - g_rain) * std::min(1.0f, dt * 0.25f);
    // winter: the same storm systems arrive as blizzards (storm-day events too)
    float wantB = season.snow > 0.5f ? std::max(sstep(n, 0.62f, 0.78f), events::forceRain() ? 1.0f : 0.0f) : 0.0f;
    if (g_settings.weather == 2) wantB = 0;
    if (std::getenv("HW_BLIZZARD")) wantB = 1, g_blizzard = std::max(g_blizzard, 0.95f);
    if (wantB > 0.5f && g_blizzard < 0.05f && E.playing) actions::notify("A blizzard is sweeping down the valley. Get indoors or wear a fur coat: the cold will drain your health.");
    g_blizzard += (wantB - g_blizzard) * std::min(1.0f, dt * 0.2f);
    // morning valley fog: on some days a fog bank fills the low ground after dawn and burns off by late morning
    {
      float dayN = valueNoise((float)(season.start * 7 + season.day) * 0.9f, 7.3f, 421);
      float morning = sstep(E.time, 4.8f, 6.0f) * (1 - sstep(E.time, 8.5f, 10.5f));
      float low = 1 - sstep(render::cam.pos.y, 35.0f, 90.0f);
      float want2 = sstep(dayN, 0.55f, 0.7f) * morning * (0.35f + 0.65f * low) * (1 - g_rain);
      g_fogBank += (want2 - g_fogBank) * std::min(1.0f, dt * 0.3f);
    }
    g_wet = g_rain > 0.15f ? std::min(1.0f, g_wet + dt * 0.08f) : std::max(0.0f, g_wet - dt * 0.006f);
    sunI *= 1.0f - 0.5f * std::min(1.0f, g_rain * 2.5f);  // overcast: sunlight drops with the first drops
    // thunderstorms in heavy rain: lightning flash now, thunder arrives 1-4 s later (sound travels slower)
    g_flash = std::max(0.0f, g_flash - dt * 7.0f);
    if (g_rain > 0.6f && dt > 0) {
      g_thunderT -= dt;
      if (g_thunderT <= 0) {
        g_thunderT = 9 + valueNoise(E.clock * 0.37f, 1.3f, 77) * 25;
        float dist = 0.3f + valueNoise(E.clock * 0.53f, 4.1f, 78) * 0.7f;  // 0 near .. 1 far
        g_flash = 1.0f - 0.6f * dist;
        ambience::thunder(1.0f + dist * 3.0f, dist);
      }
    }
    // overcast: sky and haze go grey while it rains
    Color grey = Color::hex("8a9096").lerp(Color::hex("2a2e34"), 1 - daylight);
    float rs = std::min(1.0f, g_rain * 2.5f);  // the sky clouds over as soon as the first drops fall
    g_hor = g_hor.lerp(grey, rs * 0.65f);
    g_zen = g_zen.lerp(grey * 0.9f, rs * 0.75f);
    Color white = Color::hex("c9d2dc").lerp(Color::hex("2c323a"), 1 - daylight);
    g_hor = g_hor.lerp(white, g_blizzard * 0.85f + g_fogBank * 0.35f);
    g_zen = g_zen.lerp(white * 0.92f, g_blizzard * 0.8f);
    sunI *= 1.0f - 0.55f * g_blizzard;
  }
  bool useSun = sunI >= moonI;
  // shadows fade out as sun and moon trade places, so the light direction swap is never visible
  float ratio = std::min(sunI, moonI) / std::max(1e-4f, std::max(sunI, moonI));
  g_shadowK = 1.0f - sstep(ratio, 0.35f, 0.9f);
  g_lightDir = useSun ? g_sunDir : g_moonDir;
  g_lightDir.y = std::max(g_lightDir.y, 0.1f);
  g_lightDir.normalize();
  g_sunTint = SUN.lerp(SUNLOW, sstep(dusk, 0.0f, 0.55f)).lerp(SUNSET, sstep(dusk, 0.45f, 1.0f));
  Color lightCol = useSun ? g_sunTint : MOON;
  float lightI = std::max(std::max(sunI, moonI), 0.05f);
  float hemiI = lerpf(0.32f, 1.05f, daylight) * (1 + g_flash * 2.5f);
  Color ground = G_SUMMER.lerp(G_AUTUMN, au * 0.8f).lerp(G_WINTER, snow);
  g_cover = std::min(0.95f, 0.64f + 0.22f * season.fall + 0.08f * season.autumn + 0.25f * std::min(1.0f, g_rain * 2.5f));

  const Vec3& p = E.player.pos;
  float texel = (SHADOW_HALF * 2) / (float)g_settings.shadowSize;
  float step = texel * std::max(1.0f, std::round(6.0f / texel));
  Vec3 c(std::round(p.x / step) * step, std::round(p.y / step) * step, std::round(p.z / step) * step);
  static Vec3 s_light{0, 1, 0};
  static int s_size = 0;
  bool moved = c.distanceSq(g_shadowC) > 0.01f || s_size != g_settings.shadowSize;
  bool turned = g_lightDir.dot(s_light) < 0.99985f;  // re-render the static (tree/rock) shadow every ~1 degree of sun travel, not every 0.2
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
  g_mixT = std::min(1.0f, g_mixT + dt / 0.3f);
  F.shadowMix[0] = g_mixT * g_mixT * (3 - 2 * g_mixT);
  std::memcpy(F.view, render::cam.view.m, 64);
  std::memcpy(F.proj, render::cam.proj.m, 64);
  std::memcpy(F.viewProj, render::cam.viewProj.m, 64);
  std::memcpy(F.shadowMat, g_shadowMat.m, 64);
  setv(F.cam, render::cam.pos, E.clock);
  float strength = useSun ? std::min(1.0f, sunI / 1.3f) : 0.45f * moonUp;
  setv(F.sunDir, g_lightDir, strength);
  setv(F.sunCol, lightCol * lightI, daylight);
  setv(F.hemiSky, HEMI_SKY.lerp(Color::hex("8aa4d6"), (1 - daylight) * 0.6f) * hemiI, std::max(0.011f, g_shadowK));
  setv(F.hemiGround, ground * hemiI, season.fall);
  float rainFog = (1.0f - 0.45f * g_rain) * (1.0f - 0.72f * g_blizzard) * (1.0f - 0.55f * g_fogBank);  // rain, blizzards and fog banks close the view in
  setv(F.fogCol, g_hor, (60 - 25 * season.fall) * g_settings.drawDist() * rainFog);
  F.fogParam[0] = (260 - 100 * season.fall) * g_settings.drawDist() * rainFog;
  F.fogParam[1] = (float)platform::height();
  F.fogParam[2] = clampf((0.45f - daylight) / 0.35f, 0, 1);
  F.fogParam[3] = g_wet;
  F.shadowMix[1] = g_rain;
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
void onStaticRebuild() {
  g_prevMat = g_haveBuilt ? g_builtMat : g_shadowMat;
  g_mixT = g_haveBuilt ? 0.0f : 1.0f;
  g_builtMat = g_shadowMat;
  g_haveBuilt = true;
  std::memcpy(render::frame.shadowMatPrev, g_prevMat.m, 64);
}
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
  s.set1f("u_storm", std::min(1.0f, g_rain * 2.5f));
  s.set1f("u_flash", g_flash);
  s.set1i("u_cloudTex", 12);
  glActiveTexture(GL_TEXTURE12);
  glBindTexture(GL_TEXTURE_2D, render::fbmTexture());
  glActiveTexture(GL_TEXTURE0);
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

static const char* BIRD_VS = R"(
uniform float u_t;
out float v_flap;
out float v_a;
void main(){
  int f = gl_VertexID / 7, b = gl_VertexID % 7;
  float ff = float(f);
  vec3 c = vec3(sin(ff * 2.1 + 0.7) * 260.0, 70.0 + ff * 18.0, cos(ff * 1.7 + 1.9) * 260.0);
  float r = 55.0 + ff * 20.0;
  float a = u_t * (0.05 + ff * 0.012) + ff * 2.0;
  float sb = float(b);
  float side = (b % 2 == 0 ? 1.0 : -1.0) * floor((sb + 1.0) * 0.5);
  vec3 fwd = vec3(-sin(a), 0.0, cos(a));
  vec3 rt = vec3(fwd.z, 0.0, -fwd.x);
  vec3 p = c + vec3(cos(a), 0.0, sin(a)) * r - fwd * abs(side) * 3.2 + rt * side * 2.6 + vec3(0.0, sin(u_t * 0.4 + sb) * 1.5, 0.0);
  vec4 vp = u_view * vec4(p, 1.0);
  gl_Position = u_proj * vp;
  float d = -vp.z;
  gl_PointSize = clamp(900.0 / d, 3.0, 22.0);
  v_flap = sin(u_t * (5.0 + sb * 0.37) + sb * 1.3);
  v_a = (1.0 - smoothstep(350.0, 700.0, d)) * smoothstep(0.15, 0.4, u_daylight);
}
)";
static const char* BIRD_FS = R"(
in float v_flap;
in float v_a;
out vec4 o;
void main(){
  vec2 q = gl_PointCoord * 2.0 - 1.0;
  q.y = -q.y;
  float wy = abs(q.x) * (0.55 * v_flap) ;
  float w = 1.0 - smoothstep(0.06, 0.16, abs(q.y - wy)) ;
  w *= step(abs(q.x), 0.95);
  if (w * v_a < 0.05) discard;
  o = vec4(vec3(0.06, 0.06, 0.07), w * v_a * 0.85);
}
)";
GLuint g_birdVao = 0;
void drawBirds() {
  if (E.daylight < 0.15f) return;
  if (!g_birdVao) glGenVertexArrays(1, &g_birdVao);
  Shader& s = render::program("birds", BIRD_VS, BIRD_FS);
  s.set1f("u_t", E.clock);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glBindVertexArray(g_birdVao);
  glDrawArrays(GL_POINTS, 0, 4 * 7);
  gfx::drawCalls++;
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void drawStarsAndMist() {
  drawBirds();
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

float rainAmount() { return g_rain; }
float blizzard() { return g_blizzard; }
float fogBank() { return g_fogBank; }

void warmPrograms() {
  // compile every weather/ambient shader up front (rain, snow, life, birds) so the first shower doesn't hitch
  render::program("rain", RAIN_VS, RAIN_FS);
  render::program("snow", SNOW_VS, SNOW_FS);
  render::program("life", LIFE_VS, LIFE_FS);
  render::program("birds", BIRD_VS, BIRD_FS);
}

void scareBirds(const Vec3& from) {
  if (g_birds.size() > 12) return;
  std::vector<float> t;
  forest::nearTrees(from.x, from.z, 18, t, 3);
  for (size_t i = 0; i + 3 < t.size() + 1 && i < t.size(); i += 4) {
    Vec3 top(t[i], t[i + 1] + 6.0f * t[i + 3], t[i + 2]);
    Vec3 away = top - from; away.y = 0;
    float L = std::sqrt(away.x * away.x + away.z * away.z);
    if (L < 0.01f) away = Vec3(1, 0, 0); else away = away * (1.0f / L);
    int n = 3 + (int)(std::fmod(top.x * 13.7f, 1.0f + 1e-3f) * 4);
    for (int k = 0; k < n && g_birds.size() < 24; k++) {
      float a = k * 1.7f + top.z;
      Vec3 v = away * (5.0f + (k % 3)) + Vec3(std::cos(a) * 1.5f, 3.5f + (k % 2), std::sin(a) * 1.5f);
      g_birds.push_back({top + Vec3(std::cos(a) * 0.8f, (k % 3) * 0.4f, std::sin(a) * 0.8f), v, a, 7.0f});
    }
    break;
  }
}

void drawWeather(float dt) {
  // birds: occasionally startled out of trees as you walk by; fly up and away, then are gone
  g_birdCheck -= dt;
  if (g_birdCheck <= 0) {
    g_birdCheck = 1.0f;
    const Vec3& pp = E.player.pos;
    float r = std::fmod(std::fabs(std::sin(E.clock * 12.9898f) * 43758.5f), 1.0f);
    if (E.daylight > 0.25f && g_rain < 0.3f && season.snow < 0.5f && r < 0.06f) scareBirds(pp);
  }
  for (auto& b : g_birds) {
    b.life -= dt;
    b.v.y += dt * 0.6f;
    b.p = b.p + b.v * dt;
    b.ph += dt * 18.0f;
  }
  g_birds.erase(std::remove_if(g_birds.begin(), g_birds.end(), [](const Bird& b) { return b.life <= 0; }), g_birds.end());
  if (!g_birds.empty()) {
    float u[24 * 4];
    for (int i = 0; i < 24; i++) {
      if (i < (int)g_birds.size()) { u[i * 4] = g_birds[i].p.x; u[i * 4 + 1] = g_birds[i].p.y; u[i * 4 + 2] = g_birds[i].p.z; u[i * 4 + 3] = g_birds[i].ph; }
      else { u[i * 4] = u[i * 4 + 1] = u[i * 4 + 2] = 0; u[i * 4 + 3] = -1; }
    }
    if (!g_rainVao) glGenVertexArrays(1, &g_rainVao);
    glEnable(GL_PROGRAM_POINT_SIZE);
    Shader& bs = render::program("birds", BIRD_VS, BIRD_FS);
    glUniform4fv(bs.loc("u_birds"), 24, u);
    glBindVertexArray(g_rainVao);
    glDrawArrays(GL_POINTS, 0, 24);
    gfx::drawCalls++;
  }
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  {
    float night = 1 - sstep(E.daylight, 0.05f, 0.3f), day = sstep(E.daylight, 0.25f, 0.6f);
    float summer = season.index == 1 ? 1.0f : 0.0f, spring = season.index == 0 ? 1.0f : 0.0f;
    float dry = 1 - g_rain;
    float ff = (E.daylight < 0.12f ? night : 0.0f) * summer * dry, bf = day * (spring + summer * 0.6f) * dry, pl = 0.0f;  // pollen off: read as daytime fireflies
    if (ff + bf + pl > 0.02f) {
      if (!g_rainVao) glGenVertexArrays(1, &g_rainVao);
      Shader& l = render::program("life", LIFE_VS, LIFE_FS);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, terrain_render::heightTexture());
      glActiveTexture(GL_TEXTURE0);
      l.set1i("u_height", 1);
      l.set4f("u_grid", GRID_MIN, GRID_STEP, (float)GRID_N, 0);
      l.set4f("u_life", ff, bf, pl, 0);
      glBindVertexArray(g_rainVao);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // glow
      glDrawArrays(GL_POINTS, 0, 300);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDrawArrays(GL_POINTS, 300, 12);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glDrawArrays(GL_POINTS, 340, 400);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      gfx::drawCalls += 3;
    }
  }
  if (g_rain > 0.02f) {
    if (!g_rainVao) glGenVertexArrays(1, &g_rainVao);
    Shader& r = render::program("rain", RAIN_VS, RAIN_FS);
    r.set1f("u_rainAmt", g_rain);
    glBindVertexArray(g_rainVao);
    glDrawArrays(GL_LINES, 0, 2 * 2400);
    gfx::drawCalls++;
  }
  if (season.fall > 0.01f || g_blizzard > 0.01f) {
    Shader& s = render::program("snow", SNOW_VS, SNOW_FS);
    s.set1f("u_fallAmt", std::max(season.fall, g_blizzard));
    s.set1f("u_blow", g_blizzard);
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
