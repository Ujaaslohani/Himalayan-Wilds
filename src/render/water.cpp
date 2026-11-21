#include "water.h"
#include "render.h"
#include "../world/terrain.h"
#include "../game/season.h"
#include "../game/engine.h"
#include <cstdlib>
#include <vector>

namespace {

struct WVert { float p[3]; float flow[4]; float t[2]; };
float g_rip[16][4];
int g_ripHead = 0;
struct Drop { Vec3 p, v; float life; };
std::vector<Drop> g_drops;
GLuint g_dropVao = 0, g_dropVbo = 0;
const char* DROP_VS = R"(
layout(location = 0) in vec4 a_pd;
out float v_a;
void main(){
  vec4 vp = u_view * vec4(a_pd.xyz, 1.0);
  gl_Position = u_proj * vp;
  gl_PointSize = clamp(90.0 / max(-vp.z, 0.5), 1.5, 9.0);
  v_a = a_pd.w;
}
)";
const char* DROP_FS = R"(
in float v_a;
out vec4 fragColor;
void main(){
  vec2 c = gl_PointCoord - 0.5;
  float r = dot(c, c);
  if (r > 0.25) discard;
  fragColor = vec4(vec3(0.86, 0.92, 0.95) * (0.25 + 0.75 * u_daylight), v_a * (1.0 - r * 3.0));
}
)";

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
uniform vec4 u_rip[16];  // ripple sources: x, z, start time, strength
uniform int u_ripN;
uniform sampler2D u_refl;
uniform float u_reflOn;
out vec4 fragColor;
float lightK0(){ return 1.0; }
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
  vec2 g = vec2(hx - h0, hy - h0) / e * (0.08 + rapid * 0.08 + u_lake * 0.03);
  vec3 wn = u_lake > 0.5 ? normalize(vec3(-g.x, 1.0, -g.y)) : normalize(vec3(-(g.x * tg.x + g.y * nm.x), 1.0, -(g.x * tg.y + g.y * nm.y)));
  // wind waves: a few crossing wave trains in world space, so the surface shows rolling bands and broken
  // reflections instead of a flat mirror (stronger on lakes and calm pools, scaled by the wind)
  {
    vec2 wd = normalize(u_windDir + vec2(1e-4));
    vec2 gw = vec2(0.0);
    float amp = (0.10 + 0.14 * u_windS) * mix(0.55, 1.0, u_lake) * (1.0 - 0.8 * u_ice);
    for (int k = 0; k < 4; k++) {
      float a = float(k) * 0.9 - 1.35;
      vec2 d = vec2(wd.x * cos(a) - wd.y * sin(a), wd.x * sin(a) + wd.y * cos(a));
      float f = 0.9 + float(k) * 0.75, sp = 1.3 / sqrt(f);
      float ph = dot(v_wpos.xz, d) * f - t * sp * 2.2 + float(k) * 1.7 + vnoise(v_wpos.xz * 0.08 + float(k)) * 2.5;
      gw += d * cos(ph) * amp / (1.0 + float(k) * 0.6);
    }
    vec2 cp = v_wpos.xz * 2.2 + wd * t * 0.6;  // fine capillary ripples
    gw += (vec2(vnoise(cp), vnoise(cp + 17.3)) - 0.5) * 0.12;
    wn = normalize(wn + vec3(-gw.x, 0.0, -gw.y) * (1.0 - 0.6 * clamp(rapid * 2.0, 0.0, 1.0)));
  }
  float rainDots = 0.0;
  if (u_rain > 0.02 && u_ice < 0.5) {
    // raindrops: every 0.55 m cell gets a drop at its own moment; its ring spreads and fades in ~0.6 s.
    // Two offset layers so the rings overlap like real rain. Within 45 m (smaller than a pixel beyond)
    float rk = smoothstep(0.02, 0.4, u_rain) * (1.0 - smoothstep(25.0, 45.0, v_depth));
    if (rk > 0.0) {
      vec2 rg = vec2(0.0);
      for (int L = 0; L < 2; L++) {
        vec2 q = v_wpos.xz / 0.55 + float(L) * vec2(0.37, 0.61);
        vec2 cell = floor(q);
        float h = fract(sin(dot(cell, vec2(127.1, 311.7)) + float(L) * 17.0) * 43758.5);
        vec2 c = cell + 0.25 + 0.5 * vec2(fract(h * 13.1), fract(h * 7.7));
        vec2 dv = q - c;
        float r = length(dv);
        float age = fract(u_clock * (1.3 + 0.6 * h) + h);
        float R = age * 0.55;
        float ring = exp(-pow((r - R) * 18.0, 2.0)) * (1.0 - age) * step(h, 0.35 + 0.6 * u_rain);
        rg += dv / max(r, 1e-3) * ring * sin((r - R) * 40.0);
        rainDots += ring * (1.0 - age);
      }
      wn = normalize(wn + vec3(rg.x, 0.0, rg.y) * 0.9 * rk);
      rainDots *= rk;
    }
  }
  // disturbances: rings spreading out from footsteps, wading animals and arrows (2.4 s each)
  float ringLight = 0.0;
  for (int i = 0; i < u_ripN; i++) {
    vec4 r = u_rip[i];
    float age = u_clock - r.z;
    if (age < 0.0 || age > 2.4) continue;
    vec2 dv = v_wpos.xz - r.xy;
    float d = length(dv);
    float rad = 0.25 + age * 1.15, fade = (1.0 - age / 2.4) * r.w;
    float band = exp(-pow((d - rad) * 3.2, 2.0)) * fade;
    if (band < 0.003) continue;
    float wave = sin((d - rad) * 18.0) * band;
    wn = normalize(wn + vec3(dv.x, 0.0, dv.y) / max(d, 0.05) * wave * 0.55);
    ringLight += max(wave, 0.0) * 0.5;
  }
  vec3 V = normalize(u_camPos - v_wpos);
  float fres = 0.02 + 0.98 * pow(1.0 - max(dot(wn, V), 0.0), 5.0);
  vec3 shallow = vec3(0.17, 0.24, 0.19), deepc = vec3(0.025, 0.07, 0.08);  // natural: peaty green shallows, near-black depths
  vec3 col = mix(shallow, deepc, smoothstep(0.05, 0.9, depth));
  col = mix(col, vec3(0.22, 0.36, 0.30), smoothstep(0.0, 0.25, depth) * (1.0 - smoothstep(0.25, 0.6, depth)) * 0.35);
  if (u_lake > 0.5) {
    // glacial tarns: clear turquoise over pale silt in the shallows, deep teal-blue in the middle, with a sunlit
    // caustic shimmer on the shallow bed
    vec3 lsh = vec3(0.22, 0.42, 0.40), lmid = vec3(0.06, 0.26, 0.30), ldeep = vec3(0.015, 0.09, 0.13);
    vec3 lc = mix(lsh, lmid, smoothstep(0.05, 0.45, depth));
    lc = mix(lc, ldeep, smoothstep(0.45, 1.2, depth));
    float caus = pow(abs(sin(vnoise(v_wpos.xz * 1.3 + t * 0.15) * 9.0)), 6.0) * (1.0 - smoothstep(0.1, 0.5, depth));
    lc += vec3(0.25, 0.3, 0.22) * caus * 0.35 * u_daylight * max(u_sunDir.y, 0.0);
    col = lc;
  }
  vec3 R = reflect(-V, wn);
  vec3 sky = mix(u_fogCol.rgb, u_misc.rgb, pow(clamp(R.y, 0.0, 1.0), 0.55));
  {  // reflected clouds: soft drifting puffs in the sky the water mirrors
    vec2 cp = R.xz / max(R.y, 0.08) * 2.2 + vec2(t * 0.02, t * 0.01);
    float cl = smoothstep(0.45, 0.8, vnoise(cp) * 0.65 + vnoise(cp * 2.7) * 0.35) * smoothstep(0.02, 0.2, R.y);
    sky = mix(sky, mix(u_fogCol.rgb, vec3(1.0), 0.6) * (0.4 + 0.6 * u_daylight), cl * 0.6);
  }
  // reflected surroundings: low reflection angles mirror the dark banks, trees and ridges, not open sky
  vec3 land = mix(vec3(0.07, 0.1, 0.07), vec3(0.16, 0.18, 0.2), smoothstep(-0.1, 0.5, u_sunDir.y)) * (0.4 + 0.6 * u_daylight);
  float shore = 1.0 - smoothstep(0.02, 0.22 + 0.25 * vnoise(v_wpos.xz * 0.05), R.y);
  sky = mix(sky, land, shore * 0.85 * (1.0 - u_lake * 0.3));
  // calm water is a dark mirror: reflection dominates, especially at a glance across it
  float calm = 1.0 - clamp(rapid * 2.0, 0.0, 1.0);
  col = mix(col, sky, clamp(max(fres, 0.25 * calm), 0.0, 0.93));
  col += vec3(0.85, 0.95, 1.0) * smoothstep(0.62, 0.9, n1 * 0.55 + n2 * 0.45) * rapid * 0.3;  // white ripples only in rapids
  float bank = 1.0 - smoothstep(0.02, 0.18, depth);
  float foamLines = smoothstep(0.55, 0.8, vnoise(vec2(fp.x * 1.3, fp.y * 6.0 + n1 * 2.0)));
  float foam = bank * max(smoothstep(0.35, 0.75, n3), foamLines * 0.8) * (0.25 + 0.75 * clamp(rapid * 2.0, 0.0, 1.0));  // no foam rim on calm water
  foam = max(foam, rapid * smoothstep(0.45 - rapid * 0.25, 0.85, n1 * 0.5 + n3 * 0.5));
  if (u_lake > 0.5 && u_ice < 0.3) {   // lakes: a thin, broken wash line where small waves reach the shore
    float wash = (1.0 - smoothstep(0.0, 0.05, depth)) * smoothstep(0.5, 0.75, vnoise(v_wpos.xz * 2.5 + vec2(t * 0.3, 0.0)));
    foam = max(foam, wash * 0.5);
  }
  if (u_lake < 0.5 && u_ice < 0.3) {   // v1.1.11: foam broken off the rapids drifts downstream in thin, wandering streaks
    float streak = smoothstep(0.72, 0.9, vnoise(vec2(fp.x * 0.5 + n1 * 0.6, fp.y * 9.0 + n3 * 1.5))) * smoothstep(0.7, 0.95, vnoise(vec2(fp.x * 0.12, fp.y * 1.3)));
    foam = max(foam, streak * 0.45 * (1.0 - smoothstep(25.0, 60.0, v_depth)));
  }
  col = mix(col, vec3(0.94, 0.97, 0.98), foam * 0.85);
  col += vec3(0.5, 0.55, 0.6) * rainDots * 0.18 * (0.3 + 0.7 * u_daylight);   // each ring catches a little light
  vec3 H = normalize(u_sunDir.xyz + V);
  float spec = pow(max(dot(wn, H), 0.0), 220.0) * 3.0 + pow(max(n2, 0.0), 9.0) * 0.6;
  // sun glitter: sparkling points on the ripples toward the sun
  spec += step(0.985, vnoise(fp * 9.0 + vec2(t * 2.0, -t))) * pow(max(dot(R, u_sunDir.xyz), 0.0), 8.0) * 4.0;
  col += u_sunCol.rgb * spec * 0.35 * u_daylight;
  float padA = 0.0;
  if (u_lake > 0.5 && u_ice < 0.3) {
    // lily pads float in the lake shallows: notched discs with an occasional pink flower
    vec2 lp = v_wpos.xz * 0.9;
    vec2 cell = floor(lp), f = fract(lp) - 0.5;
    float hh = fract(sin(dot(cell, vec2(41.3, 289.1))) * 43758.5);
    vec2 o = (vec2(fract(hh * 13.7), fract(hh * 7.9)) - 0.5) * 0.4;
    vec2 q = f - o;
    float rr = 0.17 + 0.12 * fract(hh * 3.3);
    float ang = atan(q.y, q.x) - hh * 6.28;
    float notch = step(0.25, abs(mod(ang + 3.14159, 6.28318) - 3.14159));
    float pad = smoothstep(rr, rr - 0.02, length(q)) * notch * step(0.55, hh);
    float zone = smoothstep(0.08, 0.2, depth) * (1.0 - smoothstep(0.45, 0.8, depth)) * smoothstep(0.35, 0.6, vnoise(v_wpos.xz * 0.12));
    pad *= zone;
    vec3 padc = mix(vec3(0.16, 0.3, 0.1), vec3(0.28, 0.42, 0.14), fract(hh * 5.1)) * (0.7 + 0.5 * max(u_sunDir.y, 0.0));
    float bloom = smoothstep(0.06, 0.03, length(q - vec2(0.03, 0.02))) * step(0.9, hh) * zone;
    col = mix(col, padc, pad);
    col = mix(col, vec3(0.95, 0.68, 0.78), bloom);
    padA = pad * 0.98;
  }
  float ice = u_ice * (0.55 + 0.45 * (1.0 - smoothstep(0.0, 0.35, depth))) * (0.85 + 0.15 * n1) * (1.0 - rapid * 0.7);
  col = mix(col, vec3(0.8, 0.88, 0.93), ice);
  float lightK = clamp(0.16 + 0.84 * u_daylight, 0.0, 1.0);
  col += vec3(0.7, 0.8, 0.85) * ringLight * 0.08 * (1.0 - u_ice);
  col *= lightK;
  if (u_lake > 0.5 && u_ice < 0.5) {
    // gentle lapping at the shore: a thin wet line that creeps in and out with the waves
    float lap = sin(t * 1.3 + vnoise(v_wpos.xz * 0.3) * 6.0) * 0.5 + 0.5;
    float edge = exp(-pow((depth - 0.025 - lap * 0.03) * 60.0, 2.0)) * (0.5 + 0.5 * vnoise(v_wpos.xz * 2.0 + t * 0.2));
    col = mix(col, vec3(0.86, 0.9, 0.9) * lightK0(), edge * 0.55);
    foam = max(foam, edge * 0.5);
  }
  float alpha = mix(smoothstep(0.0, 0.05, depth) * 0.9, 0.97, max(ice, foam * 0.6));
  if (u_lake > 0.5) alpha = mix(alpha * mix(0.55, 1.0, smoothstep(0.08, 0.5, depth)), alpha, ice);   // see the bed through the shallows
  alpha = max(alpha, padA);
  alpha = max(alpha, fres * 0.6 * smoothstep(0.0, 0.03, depth));
  if (u_reflOn > 0.5 && v_depth < 220.0 && u_ice < 0.5) {
    // screen-space reflection: march the reflected ray through the half-res copy of what is already drawn,
    // so the banks, trees, houses and lantern really appear mirrored. Where the ray leaves the screen or finds
    // nothing, the sky/bank estimate above stays
    vec3 nr = normalize(mix(vec3(0.0, 1.0, 0.0), wn, 0.35 + 0.65 * clamp(rapid * 2.0, 0.0, 1.0)));
    vec3 Rr = reflect(-V, nr);
    if (Rr.y > 0.0) {
      vec3 q = v_wpos + vec3(0.0, 0.03, 0.0);
      float st = 0.35 + v_depth * 0.012;
      vec3 hitC = vec3(0.0); float hk = 0.0;
      for (int i = 0; i < 28; i++) {
        q += Rr * st; st *= 1.13;
        vec4 cp = u_proj * (u_view * vec4(q, 1.0));
        if (cp.w <= 0.05) break;
        vec2 uv = cp.xy / cp.w * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) break;
        vec4 sp = textureLod(u_refl, uv, 0.0);
        float dz = cp.w - sp.a;
        if (dz > 0.0 && dz < st * 2.5 + 0.6) {
          vec2 e = min(uv, 1.0 - uv);
          hitC = sp.rgb; hk = smoothstep(0.0, 0.1, min(e.x, e.y)) * (1.0 - float(i) / 40.0);
          break;
        }
      }
      float rk = hk * clamp(max(fres, 0.62 * calm), 0.0, 0.92) * (1.0 - 0.7 * clamp(rapid * 2.0, 0.0, 1.0));
      col = mix(col, hitC * mix(0.85, 0.7, calm), rk);   // water darkens what it mirrors a little
      if (u_reflOn > 1.5) { col = vec3(hk, rk, Rr.y); alpha = 1.0; }
      alpha = max(alpha, rk * smoothstep(0.0, 0.03, depth));
    }
  }
  // dawn mist: a pale veil drifting low over the water in the early hours
  float dawn = smoothstep(0.02, 0.2, u_daylight) * (1.0 - smoothstep(0.35, 0.6, u_daylight)) * (u_sunDir.x > 0.0 ? 1.0 : 0.3);
  float veil = dawn * (0.35 + 0.4 * vnoise(v_wpos.xz * 0.08 + vec2(t * 0.05, 0.0))) * smoothstep(4.0, 25.0, v_depth);
  col = mix(col, fogTint(v_wpos) * 1.05 + vec3(0.04), veil);
  alpha = max(alpha, veil * 0.8);
  col = mix(col, fogTint(v_wpos), fogObject(v_wpos, v_depth));
  fragColor = vec4(col, alpha);
}
)";

const char* FALL_VS = R"(
in vec3 a_pos;
in vec4 a_col;   // x: across 0..1, y: metres down the flow from the spring, z: part (0 stream, 1 cliff, 2 ledge, 3 free fall), w: 0..1 down the free fall
out vec4 v_f;
out vec3 v_wpos;
out float v_depth;
void main(){
  v_f = a_col;
  v_wpos = a_pos;
  vec4 vp = u_view * vec4(a_pos, 1.0);
  v_depth = -vp.z;
  gl_Position = u_proj * vp;
}
)";

const char* FALL_FS = R"(
in vec4 v_f;
in vec3 v_wpos;
in float v_depth;
out vec4 fragColor;
void main(){
  float u = v_f.x, m = v_f.y, part = v_f.z, drop = v_f.w;
  float speed = part < 0.5 ? 1.2 : part < 2.5 ? 5.5 : 9.0;           // m/s: stream, cascade, free fall
  float stretch = part < 0.5 ? 1.0 : 4.0;
  float y = (m - u_clock * speed) / stretch;
  // long vertical streaks (ropes of water), finer streaks over them, and slow lumps of aerated foam
  float ropes = vnoise(vec2(u * 60.0, y * 0.9));
  float fine = vnoise(vec2(u * 150.0 + 5.0, y * 2.6));
  float lumps = vnoise(vec2(u * 20.0 + 11.0, (m - u_clock * speed * 0.8) * 0.35));
  float aer = clamp(ropes * 0.55 + fine * 0.3 + lumps * 0.35 - 0.15, 0.0, 1.0);
  if (part < 0.5) aer *= 0.35;                                        // the stream on top is mostly clear
  vec3 deep = vec3(0.12, 0.22, 0.25), foam = vec3(0.78, 0.82, 0.85);
  vec3 col = mix(deep, foam, smoothstep(0.3, 0.85, aer));
  // white where it breaks over an edge, and churned white at the foot
  col = mix(col, foam, clamp(drop * drop * 0.9, 0.0, 0.9));
  // light: sun on the face plus sky; a little forward-scatter glow when the sun is behind the curtain
  float sunK = clamp(0.2 + 0.8 * u_daylight, 0.0, 1.0);
  vec3 vdir = normalize(v_wpos - u_camPos);
  float back = pow(max(dot(vdir, u_sunDir.xyz), 0.0), 4.0) * u_daylight;
  col = col * sunK * mix(vec3(0.85, 0.9, 0.95), u_sunCol.rgb * 1.1, 0.35) + foam * back * 0.18 * aer;
  col = mix(col, fogTint(v_wpos), fogObject(v_wpos, v_depth));
  // ragged edges and gaps between the ropes; thin at the spring
  float edge = smoothstep(0.0, 0.18 + 0.1 * vnoise(vec2(y * 1.7, 3.0)), u) * smoothstep(1.0, 0.82 - 0.1 * vnoise(vec2(y * 1.9, 9.0)), u);
  float a = part < 0.5 ? 0.55 + 0.35 * aer : mix(0.6, 0.98, smoothstep(0.15, 0.6, aer));
  a *= edge * smoothstep(0.0, 3.0, m) * (1.0 - 0.4 * u_ice);
  fragColor = vec4(col, a);
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
  col = mix(col, fogTint(v_wpos), fogObject(v_wpos, v_depth));
  fragColor = vec4(col, foam * (1.0 - smoothstep(4.5, 6.0, r)));
}
)";

const char* SPRAY_VS = R"(
in vec3 a_pos;
uniform vec3 u_origin;
uniform vec3 u_dir;
out float v_a;
out float v_sun;
void main(){
  vec3 sd = a_pos;
  float t = fract(sd.x + u_clock * (0.18 + sd.y * 0.22));
  float big = step(0.7, sd.z);
  vec3 side = normalize(cross(vec3(0.0, 1.0, 0.0), u_dir));
  vec3 p = u_origin + side * (sd.y - 0.5) * 14.0 * (0.4 + t) + vec3(0.0, t * mix(3.0, 8.0, big), 0.0) + u_dir * (2.0 + (sd.z - 0.5) * 6.0 + t * 2.0);
  v_a = (1.0 - t) * mix(0.55, 0.28, big) * clamp(0.25 + u_daylight, 0.0, 1.0);
  v_a *= smoothstep(2.0, 9.0, distance(p, u_camPos));   // mist fades out right in front of the eye
  vec4 vp = u_view * vec4(p, 1.0);
  // spray lights up when the sun is behind it (forward scattering)
  v_sun = pow(max(dot(normalize(p - u_camPos), u_sunDir.xyz), 0.0), 6.0) * u_daylight;
  gl_Position = u_proj * vp;
  gl_PointSize = min(mix(120.0, 900.0, big) * (0.5 + t) / max(-vp.z, 0.5) * (u_viewH / 900.0), 256.0);
}
)";

const char* SPRAY_FS = R"(
in float v_a;
in float v_sun;
out vec4 fragColor;
void main(){
  float d = length(gl_PointCoord - 0.5) * 2.0;
  if (d > 1.0) discard;
  float a = (1.0 - d) * 0.75;
  vec3 c = mix(vec3(0.82, 0.86, 0.9), vec3(1.25, 1.15, 0.95), v_sun) * (0.45 + 0.55 * u_daylight);
  fragColor = vec4(c, a * v_a * (1.0 + v_sun));
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
  m.bmin = {1e9f, 1e9f, 1e9f}; m.bmax = {-1e9f, -1e9f, -1e9f};
  for (auto& q : v) { m.bmin = {std::min(m.bmin.x, q.p[0]), std::min(m.bmin.y, q.p[1]), std::min(m.bmin.z, q.p[2])}; m.bmax = {std::max(m.bmax.x, q.p[0]), std::max(m.bmax.y, q.p[1]), std::max(m.bmax.z, q.p[2])}; }
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
    float hw = R.hw[i] + 6.0f;  // banks are ragged (bankNoise): cover the widest inlets
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
    float rmax = L.r * 1.3f + 8;  // shoreline bulges out up to ~28% (world::lakeDist)
    for (int r = 0; r <= RINGS; r++) {
      float rr = rmax * r / RINGS;
      int segs = r == 0 ? 1 : SEG;
      for (int s = 0; s < segs; s++) {
        float a = (float)s / SEG * TAU;
        float x = L.x + std::cos(a) * rr, z = L.z + std::sin(a) * rr;
        float depth = L.level - world::getTerrainHeight(x, z);
        // past the (ragged) shoreline the sheet must vanish even where the bank dips below lake level,
        // or it floats over the hollows around the lake
        float out = world::lakeDist(L, x, z) - L.r;
        if (out > 0.5f) depth = std::min(depth, -0.3f - out * 0.05f);
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

CaveFrame g_cave;

void buildFall() {
  const River& R = world::RIVER;
  float hx = R.head[0], hz = R.head[1];
  float dirx = 0.08f, dirz = -1.0f;   // into the cliff
  float dl = hypot2(dirx, dirz);
  dirx /= dl; dirz /= dl;
  float sx = -dirz, sz = dirx;
  g_fallDir = {-dirx, 0, -dirz};
  float wy = R.wy[0];
  auto H = [&](float b, float o) { return world::getTerrainHeight(hx + dirx * b + sx * o, hz + dirz * b + sz * o); };
  auto P = [&](float b, float o, float y) { return Vec3{hx + dirx * b + sx * o, y, hz + dirz * b + sz * o}; };
  // the cliff: its foot (ground clears the water by 1.2 m) and its lip (within 1 m of the top)
  float bFoot = 0;
  for (float b = -12; b < 40; b += 0.25f) if (H(b, 0) > wy + 1.2f) { bFoot = b; break; }
  float top = -1e9f;
  for (float b = bFoot; b < bFoot + 45; b += 0.5f) top = std::max(top, H(b, 0));
  float bLip = bFoot;
  for (float b = bFoot; b < bFoot + 45; b += 0.25f) if (H(b, 0) >= top - 1.0f) { bLip = b; break; }
  // the cave: a hole in the cliff at its foot, hidden behind the curtain (game/secrets.cpp builds the tunnel)
  float floorY = wy + 0.35f;
  float bMouth = bFoot;
  for (float b = bFoot - 4; b < bFoot + 6; b += 0.1f) if (H(b, 0) >= floorY + 0.15f) { bMouth = b; break; }
  floorY = std::max(floorY, H(bMouth - 0.4f, 0) + 0.05f);
  const float DEPTH = 9.0f, HALF = 1.7f, MOUTH_H = 3.4f;
  g_cave = {P(bMouth, 0, floorY), P(bMouth, 0, floorY), std::atan2(-dirx, -dirz), DEPTH, HALF, floorY + MOUTH_H, floorY, 0};
  // the water's path as (b, y, part): spring-fed stream on top, down the cliff face, then a free fall that stands
  // ~2 m off the foot so there is a dry gap behind it (you walk between the curtain and the cave mouth)
  struct PP { float b, y, part; };
  std::vector<PP> path;
  for (float b = bLip + 8; b > bLip; b -= 0.5f) path.push_back({b, H(b, 0) + 0.12f, 0});
  float freeTop = floorY + MOUTH_H + 3.0f, bFree = bMouth;
  for (float b = bLip; b > bFoot - 1; b -= 0.4f) {
    float h = H(b, 0);
    if (h < freeTop) { bFree = b; break; }
    float hn = H(b - 0.6f, 0);
    float push = clampf((h - hn) / 0.6f * 0.25f, 0.25f, 1.2f);   // stand a little off the steep face
    path.push_back({b - push, h + 0.4f, 1});
  }
  float b0 = path.back().b, y0 = path.back().y, bOut = bMouth - 2.3f;
  for (int i = 1; i <= 22; i++) {
    float t = i / 22.0f;
    float bb = b0 + (bOut - b0) * std::sqrt(t), yy = y0 + t * ((wy - 0.6f) - y0);
    path.push_back({bb, yy, 3});
  }
  (void)bFree;
  std::vector<WVert> v;
  std::vector<uint32_t> idx;
  const int COLS = 28;
  float m = 0;
  for (size_t r = 0; r < path.size(); r++) {
    const PP& q = path[r];
    if (r > 0) m += hypot2(path[r].b - path[r - 1].b, path[r].y - path[r - 1].y);
    // a real river's worth of water: ~9 m wide on the face, spreading to ~11 m where it hits the pool
    float half = q.part < 0.5f ? 1.0f + 3.0f * (1 - (q.b - bLip) / 8) : q.part < 2.5f ? 4.5f : 4.6f + 1.0f * (r + 23 - path.size()) / 22.0f;
    float drop = q.part > 2.5f ? (float)(r + 23 - path.size()) / 22.0f : 0;
    for (int c = 0; c <= COLS; c++) {
      float u = (float)c / COLS, off = (u * 2 - 1) * half;
      float y = q.y;
      if (q.part < 1.5f) y = H(q.b, off) + (q.part < 0.5f ? 0.12f : 0.4f);
      v.push_back({{0, 0, 0}, {u, m, q.part, drop}, {0, 0}});
      Vec3 pp = P(q.b, off, y);
      v.back().p[0] = pp.x; v.back().p[1] = pp.y; v.back().p[2] = pp.z;
    }
    if (r > 0) {
      uint32_t r0 = (uint32_t)((r - 1) * (COLS + 1)), r1 = (uint32_t)(r * (COLS + 1));
      for (int c = 0; c < COLS; c++) idx.insert(idx.end(), {r0 + c, r0 + c + 1, r1 + c, r0 + c + 1, r1 + c + 1, r1 + c});
    }
  }
  g_fall = uploadW(v, idx);
  float hxP = hx + dirx * (bOut - 2.0f), hzP = hz + dirz * (bOut - 2.0f);
  hx = hxP + dirx * 2.6f; hz = hzP + dirz * 2.6f;   // keep the pool/spray code below centred just in front of the curtain
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
  std::vector<float> seeds(160 * 3);
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

CaveFrame caveFrame() { return g_cave; }

void init() {
  buildRiver();
  buildLakes();
  buildFall();
}

GLuint g_reflTex = 0;
void draw() {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  Shader& s = render::program("water", WATER_VS, WATER_FS);
  glActiveTexture(GL_TEXTURE5);   // unit 11 is the previous shadow map; 5 is a scratch unit the terrain rebinds each draw
  glBindTexture(GL_TEXTURE_2D, g_reflTex);
  glActiveTexture(GL_TEXTURE0);
  s.set1i("u_refl", 5);
  s.set1f("u_reflOn", g_reflTex ? (std::getenv("HW_SSRDBG") ? 2.0f : 1.0f) : 0.0f);
  glUniform4fv(s.loc("u_rip"), 16, &g_rip[0][0]);
  glUniform1i(s.loc("u_ripN"), 16);
  s.set1f("u_lake", 0);
  g_river.draw();
  s.set1f("u_lake", 1);
  g_lakes.draw();
  Vec3 fc = (g_fall.bmin + g_fall.bmax) * 0.5f;
  float fr = (g_fall.bmax - g_fall.bmin).length() * 0.5f + 8;
  if (render::sphereVisible(fc, fr) || render::sphereVisible(g_poolCenter, 10)) {
    render::program("fall", FALL_VS, FALL_FS);
    g_fall.draw();
    Shader& p = render::program("pool", WATER_VS, POOL_FS);
    p.set3("u_center", g_poolCenter);
    g_pool.draw();
  }
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void setReflection(unsigned tex) { g_reflTex = tex; }
unsigned reflection() { return g_reflTex; }
void ripple(float x, float y, float z, float strength, float splash) {
  if (render::cam.pos.distanceTo({x, y, z}) > 90) return;
  float* r = g_rip[g_ripHead]; g_ripHead = (g_ripHead + 1) % 16;
  r[0] = x; r[1] = z; r[2] = E.clock; r[3] = std::min(1.5f, strength);
  int n = (int)(splash * 14);
  for (int k = 0; k < n && g_drops.size() < 160; k++) {
    float a = (float)std::rand() / RAND_MAX * 6.2832f, sp = 0.6f + (float)std::rand() / RAND_MAX * 1.6f * splash;
    g_drops.push_back({{x, y + 0.02f, z}, {std::cos(a) * sp, 1.6f + (float)std::rand() / RAND_MAX * 2.4f * splash, std::sin(a) * sp}, 0.5f + (float)std::rand() / RAND_MAX * 0.4f});
  }
}

void drawParticles() {
  if (!g_drops.empty()) {
    // splash droplets: simple ballistic points, a few hundred bytes a frame
    static double last = 0; double now = E.clock; float dt = (float)std::min(0.05, std::max(0.0, now - last)); last = now;
    std::vector<float> buf; buf.reserve(g_drops.size() * 4);
    for (size_t i = 0; i < g_drops.size();) {
      Drop& d = g_drops[i];
      d.life -= dt; d.v.y -= 9.8f * dt; d.p += d.v * dt;
      if (d.life <= 0) { d = g_drops.back(); g_drops.pop_back(); continue; }
      buf.insert(buf.end(), {d.p.x, d.p.y, d.p.z, std::min(1.0f, d.life * 2.5f) * 0.8f});
      i++;
    }
    if (!buf.empty()) {
      if (!g_dropVao) {
        glGenVertexArrays(1, &g_dropVao); glGenBuffers(1, &g_dropVbo);
        glBindVertexArray(g_dropVao); glBindBuffer(GL_ARRAY_BUFFER, g_dropVbo);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 16, nullptr);
      }
      glBindVertexArray(g_dropVao); glBindBuffer(GL_ARRAY_BUFFER, g_dropVbo);
      glBufferData(GL_ARRAY_BUFFER, buf.size() * 4, buf.data(), GL_STREAM_DRAW);
      glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE); glEnable(GL_PROGRAM_POINT_SIZE);
      render::program("drops", DROP_VS, DROP_FS);
      glDrawArrays(GL_POINTS, 0, (GLsizei)(buf.size() / 4));
      gfx::drawCalls++;
      glBindVertexArray(0);
      glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    }
  }
  if (render::cam.pos.distanceTo(g_poolCenter) > 400 || !render::sphereVisible(g_poolCenter, 20)) return;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  Shader& s = render::program("spray", SPRAY_VS, SPRAY_FS);
  s.set3("u_origin", g_poolCenter);
  s.set3("u_dir", g_fallDir);
  glBindVertexArray(g_sprayVao);
  glDrawArrays(GL_POINTS, 0, 160);
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
