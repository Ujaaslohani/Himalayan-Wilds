#pragma once

namespace glsl {

static const char* FRAME = R"(
layout(std140) uniform Frame {
  mat4 u_view;
  mat4 u_proj;
  mat4 u_viewProj;
  mat4 u_shadowMat;
  vec4 u_cam;
  vec4 u_sunDir;
  vec4 u_sunCol;
  vec4 u_hemiSky;
  vec4 u_hemiGround;
  vec4 u_fogCol;
  vec4 u_fogParam;
  vec4 u_season;
  vec4 u_season2;
  vec4 u_wind;
  vec4 u_misc;
  mat4 u_shadowMatPrev;
  vec4 u_shadowMix;
};
#define u_camPos u_cam.xyz
#define u_clock u_cam.w
#define u_daylight u_sunCol.w
#define u_shadowOn u_hemiSky.w
#define u_fall u_hemiGround.w
#define u_fogNear u_fogCol.w
#define u_fogFar u_fogParam.x
#define u_viewH u_fogParam.y
#define u_night u_fogParam.z
#define u_shadowStrength u_sunDir.w
#define u_snow u_season.x
#define u_autumn u_season.y
#define u_bare u_season.z
#define u_spring u_season.w
#define u_dry u_season2.x
#define u_ice u_season2.y
#define u_windT u_season2.z
#define u_windS u_season2.w
#define u_windDir u_wind.xy
#define u_shadowTexel u_wind.z
#define u_shadowHalf u_wind.w
#define PI 3.141592653589793
#define FADE_NOISE_STATIC() fract(52.9829189 * fract(dot(floor(gl_FragCoord.xy), vec2(0.06711056, 0.00583715))))
#define FADE_NOISE() fract(52.9829189 * fract(dot(floor(gl_FragCoord.xy) + 5.588238 * mod(floor(u_clock * 60.0), 64.0), vec2(0.06711056, 0.00583715))))
)";

static const char* NOISE = R"(
float h21(vec2 p){ p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
uniform sampler2D u_noiseTex;
float vnoise(vec2 p){ vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
  return textureLod(u_noiseTex, (i + f + 0.5) * (1.0 / 256.0), 0.0).r; }
float fbm4(vec2 p){ float s = 0.0, a = 0.5; for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = p * 2.03 + vec2(1.7, 9.2); a *= 0.5; } return s + 0.03; }
uniform sampler2D u_ptexA;
uniform sampler2D u_ptexN;
uniform sampler2D u_ptexW;
// wood grain, triplanar; grain runs vertically on walls/posts and along x on horizontal faces
vec3 ptexWood(vec3 wp, inout vec3 n, float scale, float bumpK){
  vec3 w = pow(abs(n), vec3(6.0)); w /= (w.x + w.y + w.z);
  vec2 ux = wp.yz * scale, uy = wp.xz * scale, uz = wp.yx * scale;
  vec4 ax = texture(u_ptexW, ux), ay = texture(u_ptexW, uy), az = texture(u_ptexW, uz);
  vec3 alb = vec3(ax.r * w.x + ay.r * w.y + az.r * w.z) * 2.0;
  const float e = 1.0 / 512.0;
  vec2 gx = vec2(texture(u_ptexW, ux + vec2(e, 0.0)).g - ax.g, texture(u_ptexW, ux + vec2(0.0, e)).g - ax.g);
  vec2 gy = vec2(texture(u_ptexW, uy + vec2(e, 0.0)).g - ay.g, texture(u_ptexW, uy + vec2(0.0, e)).g - ay.g);
  vec2 gz = vec2(texture(u_ptexW, uz + vec2(e, 0.0)).g - az.g, texture(u_ptexW, uz + vec2(0.0, e)).g - az.g);
  vec3 dn = vec3(0.0, gx.x, gx.y) * w.x + vec3(gy.x, 0.0, gy.y) * w.y + vec3(gz.y, gz.x, 0.0) * w.z;
  n = normalize(n - dn * bumpK * 8.0);
  return alb;
}
// Pre-baked procedural rock material (albedo multiplier*0.5 in rgb, height in a; normal xy + cavity in b).
// Triplanar: returns albedo multiplier, perturbs n.
vec3 ptexRock(vec3 wp, inout vec3 n, float scale, float bumpK){
  vec3 w = pow(abs(n), vec3(4.0)); w /= (w.x + w.y + w.z);
  vec2 ux = wp.zy * scale, uy = wp.xz * scale + 0.37, uz = wp.xy * scale + 0.71;
  vec4 ax = texture(u_ptexA, ux), ay = texture(u_ptexA, uy), az = texture(u_ptexA, uz);
  vec4 bx = texture(u_ptexN, ux), by = texture(u_ptexN, uy), bz = texture(u_ptexN, uz);
  vec3 alb = (ax.rgb * w.x + ay.rgb * w.y + az.rgb * w.z) * 2.0;
  float cav = bx.b * w.x + by.b * w.y + bz.b * w.z;
  vec2 tx = bx.xy * 2.0 - 1.0, ty = by.xy * 2.0 - 1.0, tz = bz.xy * 2.0 - 1.0;
  vec3 dn = vec3(0.0, tx.y, tx.x) * w.x + vec3(ty.x, 0.0, ty.y) * w.y + vec3(tz.x, tz.y, 0.0) * w.z;
  n = normalize(n + dn * bumpK);
  return alb * mix(0.55, 1.05, cav);
}
float fbm3(vec2 p){ float s = 0.0, a = 0.5; for (int i = 0; i < 3; i++) { s += a * vnoise(p); p = p * 2.03 + vec2(1.7, 9.2); a *= 0.5; } return s + 0.06; }
)";

static const char* SHADE = R"(
vec3 srgbToLinear(vec3 c){ return mix(pow(c * 0.9478672986 + vec3(0.0521327014), vec3(2.4)), c * 0.0773993808, vec3(lessThanEqual(c, vec3(0.04045)))); }
vec3 linearToSrgb(vec3 c){ return mix(pow(max(c, vec3(0.0)), vec3(0.41666)) * 1.055 - vec3(0.055), c * 12.92, vec3(lessThanEqual(c, vec3(0.0031308)))); }
vec3 RRTAndODTFit(vec3 v){ vec3 a = v * (v + 0.0245786) - 0.000090537; vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081; return a / b; }
vec3 aces(vec3 color){
  const mat3 IM = mat3(vec3(0.59719, 0.07600, 0.02840), vec3(0.35458, 0.90834, 0.13383), vec3(0.04823, 0.01566, 0.83777));
  const mat3 OM = mat3(vec3(1.60475, -0.10208, -0.00327), vec3(-0.53108, 1.10813, -0.07276), vec3(-0.07367, -0.00605, 1.07602));
  color *= 1.0 / 0.6;
  color = IM * color;
  color = RRTAndODTFit(color);
  color = OM * color;
  return clamp(color, 0.0, 1.0);
}
vec3 lambert(vec3 albedo, vec3 n, float shadow){
  vec3 irr = mix(u_hemiGround.rgb, u_hemiSky.rgb, 0.5 * n.y + 0.5);
  irr += u_sunCol.rgb * max(dot(n, u_sunDir.xyz), 0.0) * shadow;
  return albedo * irr * (1.0 / PI);
}
vec3 outColor(vec3 lit){
  vec3 a = aces(lit);
  // cinematic grade: richer colour, warm highlights / cool shadows at low sun
  float L = dot(a, vec3(0.2126, 0.7152, 0.0722));
  a = max(mix(vec3(L), a, 1.12), 0.0);
  float gold = (1.0 - smoothstep(0.08, 0.5, u_sunDir.y)) * smoothstep(0.05, 0.3, u_daylight);
  a *= mix(vec3(1.0), mix(vec3(0.94, 0.98, 1.06), vec3(1.07, 1.0, 0.9), smoothstep(0.1, 0.6, L)), gold);
  return linearToSrgb(a);
}
float fogObject(vec3 wp, float depth){
  float r = smoothstep(3.0, 45.0, abs(wp.y - u_camPos.y));
  float alt = clamp(u_camPos.y / 50.0, 0.0, 5.0);
  float fn = mix(u_fogNear * (1.0 + alt), u_fogNear * 3.0 * (1.0 + alt), r);
  float ff = mix(u_fogFar * (1.0 + alt * 1.5), 2200.0, r * r);
  return smoothstep(fn, ff, depth);
}
vec3 fogTint(vec3 wp){
  // aerial perspective: haze brightens and warms toward the sun
  vec3 v = normalize(wp - u_camPos);
  float s = max(dot(v, u_sunDir.xyz), 0.0);
  float k = (pow(s, 6.0) * 0.75 + s * s * 0.12) * smoothstep(-0.04, 0.18, u_sunDir.y);
  vec3 warm = min(u_fogCol.rgb * vec3(1.32, 1.18, 0.98) + vec3(0.07, 0.045, 0.0), vec3(1.0));
  return mix(u_fogCol.rgb, warm, k);
}
float valleyMist(vec3 wp, float depth){
  // dawn/dusk mist pooled in the valleys, visible from high viewpoints
  float tw = smoothstep(0.02, 0.3, u_daylight) * (1.0 - smoothstep(0.5, 0.95, u_daylight));
  float low = 1.0 - smoothstep(2.0, 26.0, wp.y);
  float view = smoothstep(18.0, 60.0, u_camPos.y - wp.y + 10.0);
  return tw * low * view * smoothstep(50.0, 320.0, depth) * 0.7;
}
float fogGround(vec3 wp, float depth){
  float r = smoothstep(3.0, 45.0, abs(wp.y - u_camPos.y));
  float alt = clamp(u_camPos.y / 50.0, 0.0, 5.0);
  float nf = smoothstep(u_fogNear * (1.0 + alt), u_fogFar * (1.0 + alt * 1.5), depth) * mix(0.55, 0.22, r);
  return max(max(nf, smoothstep(180.0, 2300.0, depth) * 0.84), valleyMist(wp, depth));
}
uniform sampler2DShadow u_shadowMap;
uniform sampler2DShadow u_shadowPrev;
float shadowTap(sampler2DShadow m, mat4 M, vec3 wp, vec3 n, float dc){
  vec4 sp = M * vec4(wp + n * 0.06, 1.0);
  vec3 c = sp.xyz * 0.5 + 0.5;
  if (c.x <= 0.0 || c.x >= 1.0 || c.y <= 0.0 || c.y >= 1.0 || c.z >= 1.0) return 1.0;
  float t = u_shadowTexel;
  float z = c.z - 0.0006;
  float s;
  if (dc > 45.0) {
    s = (texture(m, vec3(c.xy + vec2(-0.6, -0.6) * t, z)) + texture(m, vec3(c.xy + vec2(0.6, 0.6) * t, z))) * 0.5;
  } else {
    const vec2 D[8] = vec2[8](vec2(-0.94, -0.34), vec2(0.94, 0.34), vec2(-0.34, 0.94), vec2(0.34, -0.94),
                              vec2(-0.6, 0.55), vec2(0.6, -0.55), vec2(0.15, 0.25), vec2(-0.2, -0.15));
    s = 0.0;
    for (int i = 0; i < 8; i++) s += texture(m, vec3(c.xy + D[i] * t * 1.6, z));
    s *= 0.125;
  }
  vec2 e = abs(c.xy - 0.5) * 2.0;
  return mix(s, 1.0, smoothstep(0.82, 1.0, max(e.x, e.y)));
}
float shadowAt(vec3 wp, vec3 n){
  if (u_shadowOn < 0.01) return 1.0;
  float dc = distance(wp, u_camPos);
  float s = shadowTap(u_shadowMap, u_shadowMat, wp, n, dc);
  // crossfade from the previous shadow map so shadows glide as the sun moves instead of stepping
  if (u_shadowMix.x < 0.999) s = mix(shadowTap(u_shadowPrev, u_shadowMatPrev, wp, n, dc), s, u_shadowMix.x);
  return mix(1.0, s, u_shadowOn);
}
)";

static const char* OBJECT_VS = R"(
invariant gl_Position;
#ifdef FUR
out vec3 v_opos;
#endif
in vec3 a_pos;
in vec4 a_col;
#if defined(VF_SMOOTH) || defined(VF_GLB)
in vec4 a_nrm;
#endif
#ifdef VF_GLB
in vec2 a_uv;
out vec2 v_uv;
#endif
#if defined(INST_LITE)
in vec4 a_i0;
in float a_i1;
#elif defined(INST_FULL)
in vec4 a_i0;
in vec4 a_i1;
in vec4 a_i2;
in vec4 a_itint;
#endif
uniform mat4 u_model;
uniform vec4 u_tint;
uniform vec2 u_fade;
uniform vec2 u_fadeIn;
uniform float u_packed;
out float v_fade;
out float v_fadeIn;
out vec3 v_wpos;
out vec4 v_col;
out vec3 v_nrm;
out float v_seed;
out float v_depth;
void main(){
  vec3 p = a_pos;
  vec3 n = vec3(0.0, 1.0, 0.0);
#if defined(VF_SMOOTH) || defined(VF_GLB)
  n = a_nrm.xyz;
#endif
  mat4 M;
#if defined(INST_LITE)
  float cy = cos(a_i1), sy = sin(a_i1), s = a_i0.w;
  M = mat4(vec4(cy * s, 0.0, -sy * s, 0.0), vec4(0.0, s, 0.0, 0.0), vec4(sy * s, 0.0, cy * s, 0.0), vec4(a_i0.xyz, 1.0));
#elif defined(INST_FULL)
  M = mat4(vec4(a_i0.x, a_i1.x, a_i2.x, 0.0), vec4(a_i0.y, a_i1.y, a_i2.y, 0.0), vec4(a_i0.z, a_i1.z, a_i2.z, 0.0), vec4(a_i0.w, a_i1.w, a_i2.w, 1.0));
#else
  M = mat4(1.0);
#endif
  M = u_model * M;
  vec3 sO = M[3].xyz;
  v_fade = u_fade.y > 0.0 ? 1.0 - smoothstep(mix(u_fade.x, u_fade.y, 0.5), u_fade.y, distance(sO.xz, u_camPos.xz)) : 1.0;
  v_fadeIn = u_fadeIn.y > 0.0 ? smoothstep(u_fadeIn.x, mix(u_fadeIn.x, u_fadeIn.y, 0.5), distance(sO.xz, u_camPos.xz)) : 1.0;
#if defined(INST_LITE)
  if (u_packed > 0.5) v_fade *= 1.0 - floor(a_i1 / 6.2831853) / 15.0;
#endif
#ifdef WIND
  vec2 sWo = sO.xz;
  vec3 sWd = transpose(mat3(M)) * vec3(u_windDir.x, 0.0, u_windDir.y) / max(1e-4, dot(M[0].xyz, M[0].xyz));
  float sGust = u_windS * (0.6 + 0.4 * sin(u_windT * 0.9 + sWo.x * 0.045 + sWo.y * 0.035));
  float sSw = 0.55 + 0.45 * sin(u_windT * 1.4 + sWo.x * 0.11 + sWo.y * 0.09);
#if defined(K_GRASS)
  { float b = min(p.y * p.y * 4.0, 1.4);
    p += sWd * ((0.25 + sSw) * 0.32 * sGust * b) + vec3(sin(u_windT * 3.1 + sWo.x * 0.8 + a_pos.x * 9.0), 0.0, cos(u_windT * 2.7 + sWo.y * 0.8)) * 0.07 * sGust * b; }
#elif defined(K_PLANT)
  { float h = clamp(p.y, 0.0, 2.5); float b = h * h * 0.18;
    p += sWd * ((0.2 + sSw) * 0.45 * sGust * b) + vec3(sin(u_windT * 2.9 + a_pos.x * 3.0 + sWo.x), 0.0, cos(u_windT * 2.5 + a_pos.z * 3.0 + sWo.y)) * 0.07 * sGust * h; }
#elif defined(K_FLOWER)
  { float h = clamp(p.y, 0.0, 1.5); p += sWd * ((0.2 + sSw) * 0.3 * sGust * h * h) + vec3(sin(u_windT * 3.3 + sWo.x * 2.0), 0.0, cos(u_windT * 2.9 + sWo.y * 2.0)) * 0.04 * sGust * h; }
#elif defined(K_BARK)
  { float h = max(p.y, 0.0) / 12.0; p += sWd * ((0.2 + sSw) * 1.1 * sGust * h * h); }
#elif defined(K_LEAF)
  { float h = max(p.y, 0.0) / 12.0; p += sWd * ((0.2 + sSw) * 1.1 * sGust * h * h)
      + vec3(sin(u_windT * 4.3 + a_pos.x * 1.7 + sWo.x), sin(u_windT * 5.1 + a_pos.y * 2.3) * 0.5, cos(u_windT * 3.9 + a_pos.z * 1.9 + sWo.y)) * 0.11 * sGust * clamp(h * 1.5, 0.3, 1.5); }
#elif defined(K_PINE)
  { float h = max(p.y, 0.0) / 12.0; p += sWd * ((0.2 + sSw) * 0.9 * sGust * h * h)
      + vec3(sin(u_windT * 3.7 + a_pos.y * 2.0 + sWo.x), 0.0, cos(u_windT * 3.3 + a_pos.y * 1.7 + sWo.y)) * 0.06 * sGust * clamp(h * 1.5, 0.0, 1.2); }
#elif defined(K_BUILD)
  { float wv = a_col.a < 0.999 ? a_col.a * (255.0 / 254.0) : 0.0;
    if (wv > 0.001) {
      float ph = u_windT * 7.0 + a_pos.x * 2.6 + a_pos.z * 2.6 + a_pos.y * 1.5 + sWo.x * 0.3;
      vec3 fl = vec3(sin(ph), 0.35 * sin(ph * 0.8 + 1.7), cos(ph * 1.1 + 0.6));
      p += wv * (0.4 + sGust) * (sWd * 0.5 + fl * 0.32);
    } }
#endif
#endif
#if defined(K_GRASS)
  p.y *= 1.0 - 0.6 * u_snow;
#elif defined(K_PLANT)
  p.y *= 1.0 - 0.45 * u_snow;
#elif defined(K_FLOWER)
  p *= max(0.0, 1.0 - 1.15 * u_snow - 0.6 * u_autumn);
#elif defined(K_MUSH)
  p *= max(0.0, 1.0 - u_snow);
#endif
  vec4 wp = M * vec4(p, 1.0);
  v_wpos = wp.xyz;
#ifdef FUR
  v_opos = a_pos;
#endif
  v_nrm = normalize(mat3(M) * n);
  v_seed = fract(sin(dot(sO.xz, vec2(12.9898, 78.233))) * 43758.5453);
  vec3 col = srgbToLinear(a_col.rgb) * u_tint.rgb;
#ifdef INST_FULL
#ifdef TINT_WHITE
  col *= mix(vec3(1.0), srgbToLinear(a_itint.rgb), step(2.97, a_col.r + a_col.g + a_col.b));
#else
  col *= srgbToLinear(a_itint.rgb);
#endif
#endif
  v_col = vec4(col, a_col.a);
#ifdef VF_GLB
  v_uv = a_uv;
#endif
#ifdef SHADOW_PASS
  gl_Position = u_shadowMat * wp;
  v_depth = 0.0;
#else
  vec4 vp = u_view * wp;
  v_depth = -vp.z;
  gl_Position = u_proj * vp;
#endif
}
)";

static const char* SEASON_FRAG = R"(
float sHash(vec3 p){ return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453); }
const vec3 SNOWC = vec3(0.86, 0.9, 0.96);
#ifdef CAPTURE
#define SNOWK 0.45
#else
#define SNOWK 1.0
#endif
vec3 seasonColor(vec3 sc, vec3 wp, float ny, float seed){
#if defined(K_LEAF)
  float sL = mix(0.85, clamp(dot(sc, vec3(0.3, 0.59, 0.11)) * 3.2, 0.35, 1.25), 0.35);
  float sNear = 1.0 - smoothstep(10.0, 30.0, distance(wp, u_camPos));
  vec3 sAu = mix(vec3(0.95, 0.33, 0.03), vec3(0.62, 0.06, 0.03), mix(0.5, smoothstep(0.35, 0.75, seed), sNear));
  sAu = mix(sAu, vec3(0.98, 0.62, 0.05), step(0.8, seed) * sNear * 0.8);
  sAu = mix(sAu, vec3(0.9, 0.45, 0.04), sHash(floor(wp * 1.3)) * 0.5 * sNear);
  sc = mix(sc, sAu * sL, clamp(u_autumn, 0.0, 1.0));
  sc = mix(sc, sc * vec3(0.9, 1.18, 0.82), u_spring);
  sc = mix(sc, vec3(0.98, 0.55, 0.7) * sL, u_spring * mix(0.12, step(0.72, seed) * 0.8, sNear));
  sc = mix(sc, sc * vec3(1.08, 1.0, 0.72), u_dry * 0.35);
  sc = mix(sc, vec3(sL * 0.5, sL * 0.4, sL * 0.33), u_snow * 0.55);
  sc = mix(sc, SNOWC, u_snow * SNOWK * (0.3 + 0.6 * smoothstep(0.1, 0.7, ny)));
#elif defined(K_PINE)
  sc = mix(sc, sc * vec3(0.85, 1.05, 0.95), u_spring * 0.5);
  sc = mix(sc, sc * vec3(0.8, 0.85, 0.9), u_snow * 0.5);
  sc = mix(sc, SNOWC, u_snow * SNOWK * (0.25 + 0.65 * smoothstep(0.05, 0.6, ny)));
#elif defined(K_PLANT)
  float sL = clamp(dot(sc, vec3(0.3, 0.59, 0.11)) * 3.0, 0.4, 1.2);
  vec3 sAu = mix(vec3(0.75, 0.38, 0.06), vec3(0.55, 0.16, 0.04), seed);
  sc = mix(sc, sAu * sL, u_autumn * 0.85);
  sc = mix(sc, sc * vec3(0.9, 1.2, 0.85), u_spring);
  sc = mix(sc, sc * vec3(1.15, 1.05, 0.7), u_dry * 0.4);
  sc = mix(sc, SNOWC, u_snow * SNOWK * (0.55 + 0.45 * smoothstep(0.0, 0.6, ny)));
#elif defined(K_GRASS)
  float sL = clamp(dot(sc, vec3(0.3, 0.59, 0.11)) * 3.0, 0.45, 1.2);
  sc = mix(sc, vec3(0.62, 0.42, 0.12) * sL, u_autumn * 0.75);
  sc = mix(sc, sc * vec3(0.92, 1.22, 0.85), u_spring);
  sc = mix(sc, sc * vec3(1.3, 1.08, 0.55), u_dry * 0.55);
  sc = mix(sc, SNOWC, u_snow * 0.85);
#elif defined(K_FLOWER)
  sc = mix(sc, sc * 1.25, u_spring);
  sc = mix(sc, sc * vec3(0.7, 0.5, 0.35), u_autumn * 0.6);
#elif defined(K_BARK)
  sc = mix(sc, SNOWC, u_snow * smoothstep(0.45, 0.9, ny) * 0.9);
#elif defined(K_ROCK)
  sc = mix(sc, SNOWC, u_snow * smoothstep(0.35, 0.8, ny));
#elif defined(K_BUILD)
  sc = mix(sc, SNOWC, u_snow * smoothstep(0.55, 0.85, ny));
#endif
  return sc;
}
)";

static const char* OBJECT_FS = R"(
in vec3 v_wpos;
in vec4 v_col;
in vec3 v_nrm;
in float v_seed;
in float v_depth;
#ifdef VF_GLB
in vec2 v_uv;
uniform sampler2D u_tex;
uniform int u_hasTex;
#endif
uniform vec4 u_emissive;
uniform float u_alpha;
uniform float u_recv;
uniform int u_dbg;
uniform int u_tier;
#ifdef FUR
in vec3 v_opos;
#endif
in float v_fade;
in float v_fadeIn;
#ifdef CAPTURE
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragNormal;
#else
out vec4 fragColor;
#endif
void main(){
  if (v_fade < 0.999 || v_fadeIn < 0.999) {
#ifdef SHADOW_PASS
    float dh = FADE_NOISE_STATIC();
#else
    float dh = FADE_NOISE_STATIC();
#endif
    if (dh >= v_fade || dh < 1.0 - v_fadeIn) discard;
  }
#ifdef K_LEAF
  if (u_bare > 0.01 && sHash(floor(v_wpos * 2.6)) < u_bare * (0.8 + 0.2 * v_seed)) discard;
#endif
  vec4 base = vec4(v_col.rgb, 1.0);
#ifdef VF_GLB
  if (u_hasTex != 0) {
    vec4 t = texture(u_tex, v_uv);
    base.rgb *= srgbToLinear(t.rgb);
    base.a = t.a;
  }
#endif
#ifdef CUTOUT
  base.a = smoothstep(0.45, 0.45 + max(fwidth(base.a), 1e-4), base.a);
  if (base.a <= 0.0) discard;
#endif
#ifdef DEPTH_ONLY
  fragColor = vec4(0.0);
  return;
#endif
#ifdef SHADOW_PASS
  fragColor = vec4(1.0);
#else
#ifdef VF_FLAT
  vec3 n = normalize(cross(dFdx(v_wpos), dFdy(v_wpos)));
#else
  vec3 n = normalize(v_nrm);
#ifdef DOUBLE_SIDED
  if (!gl_FrontFacing) n = -n;
#endif
#endif
#if defined(FUR) && !defined(SHADOW_PASS)
  {
    // fur: fine strands along the body, darker roots, soft tonal patches; fades with distance
    float fd = 1.0 - smoothstep(12.0, 40.0, v_depth);
    vec3 q = v_opos;
    float strand = vnoise(vec2(q.x * 90.0 + q.y * 20.0, q.z * 14.0 + q.y * 70.0)) * 0.6 + vnoise(vec2(q.z * 160.0, q.y * 160.0 + q.x * 40.0)) * 0.4;
    float patchv = vnoise(q.xz * 7.0 + q.y * 5.0);
    base.rgb *= mix(1.0, mix(0.78, 1.14, strand) * mix(0.9, 1.08, patchv), fd);
    n = normalize(n + (vec3(vnoise(q.zy * 120.0), vnoise(q.xz * 120.0), vnoise(q.xy * 120.0)) - 0.5) * 0.35 * fd);
  }
#endif
#if defined(K_BUILD) && !defined(CAPTURE) && !defined(SHADOW_PASS)
  if (v_col.a > 0.999) {
    float bd = 1.0 - smoothstep(45.0, 110.0, v_depth);
    if (bd > 0.001) {
      vec3 c = base.rgb;
      float mx = max(c.r, max(c.g, c.b)), mn = min(c.r, min(c.g, c.b));
      float sat = (mx - mn) / max(mx, 1e-4);
      float lum = dot(c, vec3(0.3, 0.59, 0.11));
      float woody = smoothstep(0.18, 0.35, sat) * step(c.b, c.g) * step(c.g, c.r * 1.02) * (1.0 - smoothstep(0.35, 0.6, lum));
      float stony = (1.0 - smoothstep(0.1, 0.22, sat));
      float plaster = (1.0 - woody) * (1.0 - stony);
      vec3 n0 = n;
      vec3 nw = n0, ns = n0, np = n0;
      vec3 aw = ptexWood(v_wpos, nw, 0.45, 1.0);
      // dark grey = dressed stone (rock texture, gentle); light = lime plaster (mottled, fine grain, no cracks)
      float darkStone = stony * (1.0 - smoothstep(0.18, 0.32, lum));
      float lightWall = stony - darkStone;
      plaster += lightWall;
      stony = darkStone;
      vec3 as = mix(vec3(1.0), ptexRock(v_wpos, ns, 0.55, 0.5), 0.75);
      float mot = vnoise(v_wpos.xz * 0.9 + v_wpos.y * 0.7) * 0.6 + vnoise(v_wpos.xy * 3.1 + v_wpos.z) * 0.25 + vnoise((v_wpos.zy + v_wpos.x) * 9.0) * 0.15;
      float stain = smoothstep(0.55, 0.85, vnoise(vec2(v_wpos.x + v_wpos.z, v_wpos.y * 0.25) * 0.8));
      vec3 ap = vec3(mix(0.88, 1.08, mot)) * mix(1.0, 0.86, stain);
      np = normalize(n0 + (vec3(vnoise(v_wpos.zy * 6.0), vnoise(v_wpos.xz * 6.0), vnoise(v_wpos.xy * 6.0)) - 0.5) * 0.08);
      vec3 alb = aw * woody + as * stony + ap * plaster;
      vec3 nn = normalize(nw * woody + ns * stony + np * plaster);
      // grime toward the base, edge wear from cavity-like noise
      float grime = 1.0 - 0.18 * (1.0 - smoothstep(0.0, 1.6, v_wpos.y - u_camPos.y + 1.7)) * (0.5 + 0.5 * vnoise(v_wpos.xz * 1.7));
      base.rgb *= mix(vec3(1.0), alb * grime, bd);
      n = normalize(mix(n0, nn, bd));
    }
  }
#endif
#if defined(K_ROCK) && !defined(CAPTURE)
  {
    // pre-baked procedural rock material, triplanar
    float rd = 1.0 - smoothstep(60.0, 140.0, v_depth);
    base.rgb = mix(base.rgb, vec3(0.3, 0.28, 0.25), 0.7);
    vec3 nr = n;
    vec3 alb = ptexRock(v_wpos, nr, 0.33, 1.1);
    base.rgb *= mix(vec3(1.0), alb, rd);
    n = normalize(mix(n, nr, rd));
    float moss = smoothstep(0.62, 0.92, n.y) * smoothstep(0.45, 0.7, vnoise(v_wpos.xz * 0.9)) * rd;
    base.rgb = mix(base.rgb, base.rgb * vec3(0.72, 0.92, 0.55), moss * 0.45);
  }
#endif
#if defined(CUTOUT) && !defined(CAPTURE)
  float leafFar = smoothstep(8.0, 28.0, v_depth);
  float edgeOn = 1.0 - smoothstep(0.05, 0.45, abs(dot(n, normalize(u_camPos - v_wpos))));
  n = normalize(mix(n, vec3(0.0, 1.0, 0.0), max(0.75 * leafFar, 0.85 * edgeOn)));
#endif
  vec3 sc = seasonColor(base.rgb, v_wpos, n.y, v_seed);
#if defined(CUTOUT) && !defined(CAPTURE)
  if (u_dbg == 1) { fragColor = u_tier == 1 ? vec4(0.1, 0.3, 1.0, 1.0) : vec4(1.0, 0.55, 0.0, 1.0); return; }
  if (u_dbg == 3) { fragColor = vec4(linearToSrgb(base.rgb), 1.0); return; }
#endif
#ifdef CAPTURE
  fragColor = vec4(sqrt(max(sc, vec3(0.0))), 1.0);
  fragNormal = vec4(n * 0.5 + 0.5, 1.0);
  return;
#endif
  float sh = mix(1.0, shadowAt(v_wpos, n), u_recv);
#ifdef CUTOUT
#endif
#ifdef CUTOUT
  sh = mix(0.42, 0.9, sh) * mix(0.75, 1.0, smoothstep(0.08, 0.45, u_sunDir.y));
#ifndef CAPTURE
  sh = mix(sh, 0.62, 0.85 * max(leafFar, edgeOn));
#endif
#endif
  vec3 lit = lambert(sc, n, sh) + u_emissive.rgb * sc + u_emissive.a * vec3(1.0, 0.78, 0.42);
#ifdef SOFT
  {
    vec3 V = normalize(u_camPos - v_wpos);
    float wrap = clamp((dot(n, u_sunDir.xyz) + 0.4) / 1.4, 0.0, 1.0);
    vec3 irr = mix(u_hemiGround.rgb, u_hemiSky.rgb, 0.5 * n.y + 0.5) * 1.2 + u_sunCol.rgb * wrap * mix(0.55, 1.0, sh);
    float rim = pow(1.0 - max(dot(n, V), 0.0), 3.0);
    lit = sc * irr * (1.0 / PI) + rim * 0.22 * sc * (u_hemiSky.rgb + u_sunCol.rgb * 0.25);
  }
#endif
  vec3 c = outColor(lit);
  c = mix(c, fogTint(v_wpos), fogObject(v_wpos, v_depth));
#ifdef CUTOUT
  fragColor = vec4(c, base.a);
#else
  fragColor = vec4(c, u_alpha);
#endif
#endif
}
)";

}  // namespace glsl
