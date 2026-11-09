#include "mhuman.h"
#include "render.h"
#include "../gfx/gfx.h"
#include "../assets/assets.h"
#include "../models/shape.h"
#include "../platform/platform.h"
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>

namespace mh {
namespace {
const int NJ_MAX = 23;
const char* MH_VS = R"(
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in uvec4 a_joints;
layout(location = 4) in vec4 a_w;
layout(location = 5) in vec4 a_col;
uniform mat4 u_bones[23];
out vec3 v_wpos;
out vec3 v_nrm;
out vec2 v_uv;
out vec4 v_col;
out float v_depth;
void main(){
  mat4 sk = u_bones[a_joints.x] * a_w.x + u_bones[a_joints.y] * a_w.y + u_bones[a_joints.z] * a_w.z + u_bones[a_joints.w] * a_w.w;
  vec4 wp = sk * vec4(a_pos, 1.0);
  v_wpos = wp.xyz;
  v_nrm = mat3(sk) * a_nrm;
  v_uv = a_uv;
  v_col = a_col;
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
const char* MH_FS = R"(
in vec3 v_wpos;
in vec3 v_nrm;
in vec2 v_uv;
in vec4 v_col;
in float v_depth;
out vec4 fragColor;
uniform sampler2D u_mhTex;    // skin / eye / brow texture for this part
uniform sampler2D u_mhMask;   // clothing mask: r tunic, g trousers, b shoes, a sash
uniform sampler2D u_mhAcc;    // painted accessories for this kind (shawl / necklace / bandoliers), alpha = coverage
uniform int u_part;           // 0 body (+ rigid garments), 1 eyes, 2 brows
uniform vec3 u_tunic, u_trou, u_shoe, u_sash, u_cap;
uniform float u_skinK;
void main(){
  g_hwVel = vec4(0.0, 0.0, 2.0, 1.0);  // moving thing without its own motion vector: temporal upscaling trusts this frame more
#ifdef SHADOW_PASS
  fragColor = vec4(1.0);
#else
  vec3 alb;
  float fab = 1.0;
  if (v_uv.x < -0.5) {
    // rigid garment: vertex colour, or a per-villager colour picked by the alpha code
    int code = int(v_col.a * 255.0 + 0.5);
    fab = 0.9 + 0.2 * texture(u_noiseTex, v_wpos.xz * 3.1 + v_wpos.y * 2.3).r;
    alb = code == 1 ? u_tunic : code == 2 ? u_cap : code == 3 ? u_trou : code == 4 ? u_sash : srgbToLinear(v_col.rgb);
    alb *= fab;
  } else if (u_part == 1) {
    // eyes: bright, lit without the socket's self-shadow, with a catchlight so they read clearly
    vec3 e = srgbToLinear(texture(u_mhTex, v_uv).rgb) * 1.25;
    vec3 ne = normalize(v_nrm);
    vec3 Ve = normalize(u_camPos - v_wpos);
    vec3 le = e * (u_hemiSky.rgb * 0.9 + u_sunCol.rgb * 0.45 * max(dot(ne, u_sunDir.xyz), 0.25));
    le += vec3(0.9) * pow(max(dot(reflect(-normalize(u_sunDir.xyz + vec3(0.0, 0.3, 0.0)), ne), Ve), 0.0), 60.0) * (0.4 + 0.6 * u_daylight);
    vec3 ce = outColor(le);
    fragColor = vec4(mix(ce, fogTint(v_wpos), fogObject(v_wpos, v_depth)), 1.0);
    return;
  } else if (u_part == 2) {
    if (texture(u_mhTex, v_uv).a < 0.45) discard;
    alb = vec3(0.025, 0.018, 0.012);
  } else {
    alb = srgbToLinear(texture(u_mhTex, v_uv).rgb) * u_skinK;
    vec4 m = texture(u_mhMask, v_uv);
    // woven homespun: fine grain from the noise texture, slightly darker in the creases (low skin-texture luminance)
    float grain = 0.86 + 0.28 * texture(u_noiseTex, v_uv * 37.0).r;
    float crease = clamp(dot(alb, vec3(0.3, 0.55, 0.15)) * 3.2, 0.7, 1.08);
    alb = mix(alb, u_trou * grain * crease, m.g);
    alb = mix(alb, u_tunic * grain * crease, m.r);
    alb = mix(alb, u_sash * grain * crease, m.a);
    alb = mix(alb, u_shoe * grain, m.b);
    vec4 acc = texture(u_mhAcc, v_uv);
    alb = mix(alb, srgbToLinear(acc.rgb) * grain, acc.a);
  }
  vec3 n = normalize(v_nrm);
  if (!gl_FrontFacing) n = -n;
  float sh = shadowAt(v_wpos + n * 0.05, n);  // offset off the low-poly surface: no self-shadow speckle
  vec3 lit = lambert(alb, n, sh);
  vec3 V = normalize(u_camPos - v_wpos);
  float rim = pow(1.0 - max(dot(n, V), 0.0), 3.0);
  lit += rim * 0.12 * alb * (u_hemiSky.rgb + u_sunCol.rgb * 0.25);
  vec3 c = outColor(lit);
  c = mix(c, fogTint(v_wpos), fogObject(v_wpos, v_depth));
  fragColor = vec4(c, 1.0);
#endif
}
)";

#pragma pack(push, 1)
struct Vtx { float p[3], n[3], uv[2]; uint8_t j[4], w[4], c[4]; };
#pragma pack(pop)
struct Part { GLuint vao = 0, vbo = 0, ebo = 0; int n = 0; };

bool g_tried = false, g_ok = false;
int g_nJ = 0;
int g_par[NJ_MAX];
Vec3 g_piv[NJ_MAX];
std::string g_jname[NJ_MAX];
std::vector<Vtx> g_bodyV;
std::vector<uint32_t> g_bodyI;
std::vector<Vtx> g_eyeV, g_browV;
std::vector<uint32_t> g_eyeI, g_browI;
Part g_body[4], g_eyes, g_brows;  // body + garments per kind
Texture g_skin[3], g_eyeTex, g_browTex, g_mask, g_acc[3];  // acc: man/elder, woman, bandit
float g_height = 1.657f;
Shader* g_cur = nullptr;
bool g_shadow = false;

Part upload(const std::vector<Vtx>& V, const std::vector<uint32_t>& I) {
  Part p;
  glGenVertexArrays(1, &p.vao);
  glBindVertexArray(p.vao);
  glGenBuffers(1, &p.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, p.vbo);
  glBufferData(GL_ARRAY_BUFFER, V.size() * sizeof(Vtx), V.data(), GL_STATIC_DRAW);
  glGenBuffers(1, &p.ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, I.size() * 4, I.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, p));
  glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, n));
  glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, uv));
  glEnableVertexAttribArray(3); glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, sizeof(Vtx), (void*)offsetof(Vtx, j));
  glEnableVertexAttribArray(4); glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vtx), (void*)offsetof(Vtx, w));
  glEnableVertexAttribArray(5); glVertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vtx), (void*)offsetof(Vtx, c));
  glBindVertexArray(0);
  gfx::addBufferBytes((long long)(V.size() * sizeof(Vtx) + I.size() * 4));
  p.n = (int)I.size();
  return p;
}

int jointIndex(const char* n) {
  for (int j = 0; j < g_nJ; j++) if (g_jname[j] == n) return j;
  return -1;
}

// rigid garments as skinned vertices bound to one joint; alpha = colour code (255 fixed, 1 tunic, 2 cap, 3 trousers, 4 sash)
void addShape(std::vector<Vtx>& V, std::vector<uint32_t>& I, const Shape& s, int joint, uint8_t code) {
  for (auto& v : s.verts) {
    Vtx o{};
    o.p[0] = v.p.x; o.p[1] = v.p.y; o.p[2] = v.p.z;
    o.n[0] = v.n.x; o.n[1] = v.n.y; o.n[2] = v.n.z;
    o.uv[0] = -1; o.uv[1] = -1;
    o.j[0] = (uint8_t)joint; o.w[0] = 255;
    o.c[0] = v.c & 255; o.c[1] = (v.c >> 8) & 255; o.c[2] = (v.c >> 16) & 255; o.c[3] = code;
    I.push_back((uint32_t)V.size());
    V.push_back(o);
  }
}
// skirts and coat flares: skinned like cloth instead of riding the pelvis rigidly. The waistband blends into the
// lower spine (so the chest can't twist away from it at the belt) and the hem increasingly follows the thigh on
// its side (front and back split between both), so legs swing the cloth instead of stepping out from under a cone.
void addSkirt(std::vector<Vtx>& V, std::vector<uint32_t>& I, const Shape& s, uint8_t code, float yTop, float yHem) {
  int pel = jointIndex("pelvis"), sp1 = jointIndex("spine_01"), tl = jointIndex("thigh_l"), tr = jointIndex("thigh_r");
  if (pel < 0 || sp1 < 0 || tl < 0 || tr < 0) { addShape(V, I, s, std::max(0, pel), code); return; }
  for (auto& v : s.verts) {
    Vtx o{};
    o.p[0] = v.p.x; o.p[1] = v.p.y; o.p[2] = v.p.z;
    o.n[0] = v.n.x; o.n[1] = v.n.y; o.n[2] = v.n.z;
    o.uv[0] = -1; o.uv[1] = -1;
    float low = clampf((yTop - 0.04f - v.p.y) / std::max(0.05f, yTop - yHem), 0, 1);   // 0 at the waist, 1 at the hem
    // the garment rings were centred on a round waist; the body's real waist is ~0.26 m wide and sits forward (back
    // at z -0.04, front +0.135): pull the upper part onto it so there is no 6-7 cm ledge across the back
    float fit = 1.0f - low / 0.3f; fit = fit <= 0 ? 0 : fit >= 1 ? 1 : fit * fit * (3 - 2 * fit);
    o.p[2] = o.p[2] + fit * ((0.045f + (o.p[2] - 0.025f) * 0.7f) - o.p[2]);
    o.p[0] = o.p[0] * (1.0f - 0.1f * fit);
    float top = clampf((v.p.y - (yTop - 0.12f)) / 0.12f, 0, 1);                       // 1 at the waistband
    float a = clampf(v.p.x / 0.16f, -1, 1);                                             // character's left is +x
    float leg = low * low * 0.6f;
    float wl = leg * 0.5f * (1 + a), wr = leg * 0.5f * (1 - a), ws = top * 0.35f;
    float wp = std::max(0.0f, 1 - wl - wr - ws);
    float W[4] = {wp, ws, wl, wr};
    int J[4] = {pel, sp1, tl, tr};
    int sum = 0;
    for (int i = 0; i < 4; i++) { o.j[i] = (uint8_t)J[i]; o.w[i] = (uint8_t)std::lround(W[i] * 255); sum += o.w[i]; }
    o.w[0] = (uint8_t)std::max(0, std::min(255, o.w[0] + 255 - sum));
    o.c[0] = v.c & 255; o.c[1] = (v.c >> 8) & 255; o.c[2] = (v.c >> 16) & 255; o.c[3] = code;
    I.push_back((uint32_t)V.size());
    V.push_back(o);
  }
}
Shape openCyl(const char* hex, float y0, float r0, float y1, float r1, float zc, float zs, int seg = 14) {
  Shape s;
  Geo g = cylinderGeo(r1, r0, y1 - y0, seg, 2, true);
  s.add(g, hex, {0, (y0 + y1) * 0.5f, zc}, {0, 0, 0}, {1, 1, zs});
  return s;
}

// a friendlier, more appealing face, done on the mesh: bigger eyes (eyeballs, lids and the skin around them scaled
// up about each eye, brows lifted with them) and mouth corners raised into a soft smile
void appealFace() {
  if (g_eyeV.empty()) return;
  Vec3 c[2] = {{0, 0, 0}, {0, 0, 0}};
  int n[2] = {0, 0};
  for (auto& v : g_eyeV) { int s = v.p[0] > 0 ? 0 : 1; c[s] = c[s] + Vec3{v.p[0], v.p[1], v.p[2]}; n[s]++; }
  for (int s = 0; s < 2; s++) if (n[s]) c[s] = c[s] * (1.0f / n[s]);
  const float EYE = 1.5f, R = 0.04f;
  for (auto& v : g_eyeV) {
    const Vec3& e = c[v.p[0] > 0 ? 0 : 1];
    for (int a = 0; a < 3; a++) v.p[a] = (&e.x)[a] + (v.p[a] - (&e.x)[a]) * EYE;
  }
  auto around = [&](Vtx& v, float liftZ) {
    for (int s = 0; s < 2; s++) {
      const Vec3& e = c[s];
      float dx = v.p[0] - e.x, dy = v.p[1] - e.y, d = std::sqrt(dx * dx + dy * dy);
      if (d >= R * 1.3f) continue;
      float t = 1 - d / (R * 1.3f), k = 1 + (EYE - 1) * t * t * (3 - 2 * t);
      v.p[0] = e.x + dx * k;
      v.p[1] = e.y + dy * k;
      v.p[2] += liftZ * t;
    }
  };
  for (auto& v : g_browV) around(v, 0.004f);  // brows ride up with the bigger eyes
  for (auto& v : g_bodyV) {
    if (v.p[1] < 1.44f || v.p[2] < 0.04f) continue;
    for (int s = 0; s < 2; s++) {
      const Vec3& e = c[s];
      float dx = v.p[0] - e.x, dy = v.p[1] - e.y, d = std::sqrt(dx * dx + dy * dy);
      if (d >= R) continue;
      float t = 1 - d / R, k = 1 + (EYE - 1) * t * t * (3 - 2 * t);
      v.p[0] = e.x + dx * k;
      v.p[1] = e.y + dy * k;
      v.p[2] += (EYE - 1) * 0.01f * t;  // lids follow the larger eyeball
    }
    // smile: the lip line (y ~1.487) curves up toward the corners (x ~ +-2.5 cm); the painted lips follow the vertices
    float ax = std::fabs(v.p[0]), y = v.p[1];
    if (v.p[2] > 0.12f && ax < 0.036f && y > 1.472f && y < 1.5f) {
      float u = std::min(1.0f, ax / 0.026f);
      float fy = 1.0f - std::min(1.0f, std::fabs(y - 1.487f) / 0.014f);
      v.p[1] += 0.0085f * u * u * fy;
      v.p[0] *= 1.0f + 0.05f * u * fy;
      v.p[2] -= 0.002f * u * fy;
    }
    // cheeks lifted a touch by the smile
    if (v.p[2] > 0.09f && ax > 0.026f && ax < 0.06f && y > 1.488f && y < 1.53f) v.p[1] += 0.003f;
  }
}

// ring of small embroidered chips (Kullu-pattu style) around an open cylinder of radius r at height y
void patternBand(Shape& s, float y, float r, float zc, float zs, int n, float w, float h) {
  static const char* COL[5] = {"#b91c1c", "#facc15", "#15803d", "#1d4ed8", "#f5f5f4"};
  for (int i = 0; i < n; i++) {
    float a = (float)i / n * TAU;
    s.box(COL[i % 5], {std::sin(a) * (r + 0.004f), y, zc + std::cos(a) * (r + 0.004f) * zs}, {w, h, 0.006f}, {0, a, (i & 1) ? 0.785f : 0.0f});
  }
}

void buildKinds() {
  int pelvis = jointIndex("pelvis"), spine = jointIndex("spine_03"), neckJ = jointIndex("neck_01");
  int head = jointIndex("head");
  std::vector<uint32_t> keep = g_bodyI;
  for (int k = 0; k < 4; k++) {
    std::vector<Vtx> V = g_bodyV;
    std::vector<uint32_t> I = keep;
    Shape fixedCol;  // accessories in fixed colours
    if (k == WOMAN) {
      addSkirt(V, I, openCyl("#ffffff", 0.12f, 0.33f, 1.0f, 0.15f, 0.025f, 0.85f, 16), 3, 1.0f, 0.12f);  // long ghagra skirt
      Shape sc;  // headscarf over the crown and back, falling to the shoulders
      sc.ball("#ffffff", {0, 1.592f, 0.018f}, {0.1f, 0.1f, 0.104f}, {0, 0, 0}, 2);
      sc.box("#ffffff", {0, 1.43f, -0.075f}, {0.2f, 0.26f, 0.03f}, {0.12f, 0, 0});
      addShape(V, I, sc, head, 2);
      Shape hair; hair.ball("#1c120c", {0, 1.618f, 0.06f}, {0.086f, 0.036f, 0.07f}, {0, 0, 0}, 1);
      fixedCol.ball("#d4af37", {0.083f, 1.515f, 0.03f}, {0.008f, 0.012f, 0.008f}, {0, 0, 0}, 0).ball("#d4af37", {-0.083f, 1.515f, 0.03f}, {0.008f, 0.012f, 0.008f}, {0, 0, 0}, 0);  // earrings
      addShape(V, I, hair, head, 255);
      Shape bd; bd.tube("#ffffff", {0, 1.595f, 0.02f}, 0.103f, 0.103f, 0.018f, 16, {1.25f, 0, 0}, {1, 1, 0.95f});
      addShape(V, I, bd, head, 4);
      addShape(V, I, fixedCol, head, 255);
      fixedCol = Shape();
      Shape hem; hem.add(cylinderGeo(0.334f, 0.334f, 0.035f, 16, 1, true), "#ffffff", {0, 0.16f, 0.025f}, {0, 0, 0}, {1, 1, 0.85f});
      addSkirt(V, I, hem, 4, 1.0f, 0.12f);  // contrasting hem band
    } else if (k == BANDIT) {
      addSkirt(V, I, openCyl("#ffffff", 0.6f, 0.23f, 1.0f, 0.15f, 0.025f, 0.85f), 1, 1.0f, 0.6f);
      Shape q;  // quiver on the back, crossed bandolier
      q.tube("#3b2418", {0.08f, 1.15f, -0.17f}, 0.055f, 0.05f, 0.5f, 9, {0.25f, 0, -0.35f}).cone("#57534e", {0.16f, 1.44f, -0.22f}, 0.035f, 0.1f, 4, {0.25f, 0, -0.35f});
      addShape(V, I, q, spine, 255);
      Shape hood;
      hood.ball("#1a1714", {0, 1.585f, -0.005f}, {0.104f, 0.104f, 0.094f}, {0, 0, 0}, 2).ball("#1a1714", {0, 1.47f, -0.06f}, {0.1f, 0.08f, 0.06f}, {0, 0, 0}, 1);
      hood.ball("#7f1d1d", {0, 1.425f, 0.03f}, {0.088f, 0.045f, 0.092f}, {0, 0, 0}, 2);   // red neckerchief: the bandit mark, face left bare
      hood.cone("#7f1d1d", {0, 1.395f, 0.1f}, 0.045f, 0.06f, 3, {PI, 0, 0});
      addShape(V, I, hood, head, 255);
      Shape fb;  // heavy drooping moustache and a short rough beard
      for (int side = -1; side <= 1; side += 2) {
        fb.rod("#120c08", {side * 0.003f, 1.4995f, 0.1565f}, {side * 0.022f, 1.4955f, 0.153f}, 0.0085f, 0.009f, 5);
        fb.rod("#120c08", {side * 0.022f, 1.4955f, 0.153f}, {side * 0.031f, 1.48f, 0.145f}, 0.009f, 0.007f, 5);
        fb.rod("#120c08", {side * 0.031f, 1.48f, 0.145f}, {side * 0.033f, 1.462f, 0.137f}, 0.007f, 0.003f, 4);
      }
      fb.ball("#1a120c", {0, 1.452f, 0.118f}, {0.05f, 0.03f, 0.04f}, {0, 0, 0}, 1);
      addShape(V, I, fb, head, 255);
    } else {
      addSkirt(V, I, openCyl("#ffffff", 0.62f, 0.22f, 1.0f, 0.15f, 0.025f, 0.85f), 1, 1.0f, 0.62f);  // chola flare to mid-thigh
      Shape topi;  // Himachali topi: wool body, coloured velvet band in front
      topi.tube(k == ELDER ? "#8a8278" : "#d6d0c4", {0, 1.628f, 0.042f}, 0.093f, 0.095f, 0.072f, 18, {0, 0, 0}, {1, 1, 1.12f});
      addShape(V, I, topi, head, 255);
      Shape band; band.tube("#ffffff", {0, 1.6f, 0.042f}, 0.097f, 0.097f, 0.026f, 18, {0, 0, 0}, {1, 1, 1.13f});
      addShape(V, I, band, head, 2);
      const char* hc = k == ELDER ? "#d6d3cd" : "#1c120c";
      Shape fh;  // curled Pahari moustache; elders a full white beard
      for (int side = -1; side <= 1; side += 2) {
        fh.rod(hc, {side * 0.003f, 1.4985f, 0.1555f}, {side * 0.02f, 1.4955f, 0.152f}, 0.0062f, 0.0068f, 5);
        fh.rod(hc, {side * 0.02f, 1.4955f, 0.152f}, {side * 0.033f, 1.4905f, 0.142f}, 0.0068f, 0.0045f, 5);
        fh.rod(hc, {side * 0.033f, 1.4905f, 0.142f}, {side * 0.041f, 1.4985f, 0.13f}, 0.0045f, 0.0018f, 4);
      }
      if (k == ELDER) {
        fh.ball(hc, {0, 1.438f, 0.1f}, {0.064f, 0.05f, 0.062f}, {0, 0, 0}, 1);
        fh.cone(hc, {0, 1.4f, 0.112f}, 0.04f, 0.07f, 6, {PI, 0, 0});
        for (int side = -1; side <= 1; side += 2) fh.ball(hc, {side * 0.058f, 1.475f, 0.075f}, {0.018f, 0.04f, 0.035f}, {0, 0, 0}, 0);
      }
      addShape(V, I, fh, head, 255);
      Shape hem; hem.add(cylinderGeo(0.224f, 0.224f, 0.03f, 16, 1, true), "#ffffff", {0, 0.635f, 0.025f}, {0, 0, 0}, {1, 1, 0.85f});
      addSkirt(V, I, hem, 4, 1.0f, 0.62f);  // contrasting hem band on the chola
    }
    Shape belt; belt.tube("#ffffff", {0, 0.975f, 0.02f}, 0.148f, 0.152f, 0.06f, 16, {0, 0, 0}, {1, 1, 0.86f});
    addSkirt(V, I, belt, 4, 1.0f, 0.62f);
    g_body[k] = upload(V, I);
  }
  (void)neckJ;
}

Texture loadTex(const char* rel, bool alpha) {
  int w = 0, h = 0;
  std::vector<uint8_t> px = assets::loadImageRGBA(rel, w, h);
  if (px.empty()) return {};
  return uploadTextureRGBA(px.data(), w, h, true, !alpha, false, false);
}

bool load() {
  std::vector<uint8_t> b;
  if (!platform::readFile(platform::assetPath("people/person.hwp"), b) || b.size() < 16 || std::memcmp(b.data(), "HWP1", 4) != 0) return false;
  size_t o = 4;
  auto rd = [&](void* dst, size_t n) { if (o + n > b.size()) return false; std::memcpy(dst, b.data() + o, n); o += n; return true; };
  uint32_t nJ;
  rd(&nJ, 4);
  if (nJ > NJ_MAX) return false;
  g_nJ = (int)nJ;
  for (int j = 0; j < g_nJ; j++) {
    char nm[33] = {};
    rd(nm, 32);
    g_jname[j] = nm;
    rd(&g_par[j], 4);
    float m[16];
    rd(m, 64);
    g_piv[j] = {m[12], m[13], m[14]};
  }
  uint32_t nm;
  rd(&nm, 4);
  for (uint32_t k = 0; k < nm; k++) {
    char name[33] = {};
    rd(name, 32);
    uint32_t kind, nv, ni;
    rd(&kind, 4); rd(&nv, 4); rd(&ni, 4);
    std::vector<Vtx> V(nv);
    std::vector<uint32_t> I(ni);
    if (!rd(V.data(), nv * sizeof(Vtx)) || !rd(I.data(), ni * 4)) return false;
    if (kind == 0) {
      g_bodyV = V; g_bodyI = I;
      float top = 0;
      for (auto& v : V) top = std::max(top, v.p[1]);
      g_height = top;
    } else if (kind == 1) { g_eyeV = V; g_eyeI = I; }
    else { g_browV = V; g_browI = I; }
  }
  if (g_bodyV.empty()) return false;
  appealFace();
  if (!g_eyeV.empty()) g_eyes = upload(g_eyeV, g_eyeI);
  if (!g_browV.empty()) g_brows = upload(g_browV, g_browI);
  buildKinds();
  g_skin[0] = loadTex("people/mh_skin_m1.png", false);
  g_skin[1] = loadTex("people/mh_skin_f.png", false);
  g_skin[2] = loadTex("people/mh_skin_m2.png", false);
  g_eyeTex = loadTex("people/mh_eye.png", false);
  g_browTex = loadTex("people/mh_brow.png", true);
  {
    int w = 0, h = 0;
    std::vector<uint8_t> px = assets::loadImageRGBA("people/mh_cloth.png", w, h);
    if (!px.empty()) g_mask = uploadTextureRGBA(px.data(), w, h, true, false, false, false);
  }
  {
    const char* an[3] = {"people/mh_acc_m.png", "people/mh_acc_f.png", "people/mh_acc_b.png"};
    for (int i = 0; i < 3; i++) {
      int w = 0, h = 0;
      std::vector<uint8_t> px = assets::loadImageRGBA(an[i], w, h);
      if (!px.empty()) g_acc[i] = uploadTextureRGBA(px.data(), w, h, true, false, false, false);
    }
  }
  if (!g_skin[0].id || !g_mask.id) return false;
  std::printf("MakeHuman villagers: %d joints, body %zu tris\n", g_nJ, g_bodyI.size() / 3);
  return true;
}

// pose -> per-joint model-space deformation (bind pose -> posed), parents first
void evaluate(const Pose& P, Mat4* D) {
  Quat R[NJ_MAX];
  for (int j = 0; j < g_nJ; j++) R[j] = Quat();
  auto X = [](float a) { return Quat::axisAngle({1, 0, 0}, a); };
  auto Y = [](float a) { return Quat::axisAngle({0, 1, 0}, a); };
  auto Z = [](float a) { return Quat::axisAngle({0, 0, 1}, a); };
  // stride: with cadence f (cycles/s) each step covers v / (2 f); the hip swing that gives that step with a
  // ~0.85 m leg is asin(step / (2 * 0.85)), so the planted foot stays put on the ground
  float amp = 0.42f * P.walk;
  if (P.speed >= 0) {
    float f = 0.85f + 0.3f * P.speed, step = P.speed / (2.0f * f);
    amp = P.speed > 0.05f ? std::asin(std::min(0.6f, step / 1.8f)) : 0.0f;
  }
  float wk = std::min(1.0f, amp / 0.25f);
  float sw = std::sin(P.walkPhase) * amp;
  auto set = [&](const char* n, const Quat& q) { int j = jointIndex(n); if (j >= 0) R[j] = q; };
  // arms: out of MakeHuman's A-pose down to the sides, then swing / raise forward and bend the elbows
  set("upperarm_l", X(-(-sw * 1.15f * P.armSwing + P.armL)) * Z(-0.74f + P.armOutL));
  set("upperarm_r", X(-(sw * 1.15f * P.armSwing + P.armR)) * Z(0.74f - P.armOutR));
  // MakeHuman binds the forearm ~50 deg forward of the upper arm: relax it to a natural ~25 deg at rest
  // the elbow hinges about its own axis (perpendicular to the A-posed upper arm), so a deep bend folds the
  // forearm up in front of the chest instead of swinging the hand out sideways once the arm is lowered
  auto hinge = [&](const char* up, const char* lo) {
    int a = jointIndex(up), b = jointIndex(lo);
    if (a < 0 || b < 0) return Vec3{1, 0, 0};
    Vec3 d = g_piv[b] - g_piv[a];
    Vec3 h{-d.y, d.x, 0};
    return h.length() > 1e-5f ? h.normalized() : Vec3{1, 0, 0};
  };
  set("lowerarm_l", Quat::axisAngle(hinge("upperarm_l", "lowerarm_l"), -(-0.45f + 0.25f * P.walk + P.elbowL)));
  set("lowerarm_r", Z(P.foldR) * Quat::axisAngle(hinge("upperarm_r", "lowerarm_r"), -(-0.45f + 0.25f * P.walk + P.elbowR)));  // fold about the bind-space Z: once the arm is raised forward that swings the forearm in toward the face
  set("clavicle_l", Z(-0.05f));
  set("clavicle_r", Z(0.05f));
  // legs: hip swing, knee bends through the recovery half of each stride
  // knees: straight-ish while planted, bent ~50 deg as the leg swings through (lifts the foot clear)
  // knee flexes most just after toe-off (early swing, thigh still behind), ~45 deg, then straightens for heel strike
  float kl = (0.08f + 0.7f * std::pow(std::max(0.0f, std::sin(P.walkPhase + 1.75f)), 2.0f)) * wk;
  float kr = (0.08f + 0.7f * std::pow(std::max(0.0f, std::sin(P.walkPhase + 1.75f + PI)), 2.0f)) * wk;
  // MakeHuman binds the feet 36 cm apart (A-stance): bring them in under the hips, more so in stride (feet ~ on one line)
  float add = 0.09f + 0.05f * wk;
  // seen from the front a real walk is a side-to-side weight shift: the hips slide over the planted foot and tip
  // down on the swinging side, the chest leans back the other way and the head stays level. Left leg swings
  // around phase 0 (right foot planted), right leg around phase pi.
  float cph = std::cos(P.walkPhase) * wk;
  float tilt = 0.05f * cph;  // pelvis obliquity, ~3 deg
  float slide = 0.032f * cph / 0.85f;  // legs angle back under the shifted hips so the planted foot doesn't slide
  set("thigh_l", Z(-add + tilt + slide) * X(-sw - 0.05f * wk));  // hips flex further forward than they extend back
  set("thigh_r", Z(add + tilt + slide) * X(sw - 0.05f * wk));     // (thighs undo the pelvis tilt: legs stay plumb)
  set("calf_l", X(kl));
  set("calf_r", X(kr));
  set("foot_l", Z(add) * X(-kl * 0.45f + std::cos(P.walkPhase) * 0.18f * wk));  // heel strike / toe off
  set("foot_r", Z(-add) * X(-kr * 0.45f - std::cos(P.walkPhase) * 0.18f * wk));
  // the hip turn and tilt are undone gradually over the three spine joints (never more than ~3-4 deg at one joint):
  // a single sharp counter-twist at the waist made the coat and the robe below it look like two separate pieces
  float hipYaw = -std::sin(P.walkPhase) * 0.07f * wk;
  set("pelvis", Y(hipYaw) * Z(-tilt));  // hips turn with the stride and drop on the swing side
  set("spine_01", Y(-hipYaw * 0.45f) * Z(tilt * 0.45f) * X(P.lean * 0.5f));
  set("spine_03", Y(-hipYaw * 0.45f) * Z(tilt * 0.45f));
  set("spine_02", Y(-hipYaw * 0.55f) * Z(tilt * 0.4f) * X(P.lean * 0.5f + 0.04f * wk));  // shoulders end up gently counter-rotated
  set("neck_01", Z(-tilt * 0.3f) * Y(P.headYaw * 0.4f));  // head stays level
  set("head", Y(P.headYaw * 0.6f) * X(P.headPitch));
  for (int j = 0; j < g_nJ; j++) {
    const Vec3& pv = g_piv[j];
    Mat4 L = Mat4::translate(pv) * Mat4::compose({0, 0, 0}, R[j], {1, 1, 1}) * Mat4::translate({-pv.x, -pv.y, -pv.z});
    D[j] = g_par[j] >= 0 ? D[g_par[j]] * L : L;
  }
  // body rises over the planted leg and dips at each heel strike (twice per cycle)
  float bob = (std::fabs(std::cos(P.walkPhase)) - 0.64f) * 0.025f * wk;
  float sway = -0.032f * cph;  // body over the planted foot (character's right is -X)
  if (bob != 0 || sway != 0) { Mat4 T = Mat4::translate({sway, bob, 0}); for (int j = 0; j < g_nJ; j++) D[j] = T * D[j]; }
}
}  // namespace

bool ready() {
  if (!g_tried) {
    g_tried = true;
    g_ok = load();
    if (!g_ok) std::fprintf(stderr, "MakeHuman villagers unavailable (assets/people/*) - using the built-in models\n");
  }
  return g_ok;
}
float height() { return g_height; }

void begin(bool shadow) {
  g_shadow = shadow;
  g_cur = shadow ? &render::program("mh_shadow", MH_VS, MH_FS, "#define SHADOW_PASS\n") : &render::program("mh", MH_VS, MH_FS);
  g_cur->use();
  if (!shadow) {
    g_cur->set1i("u_mhTex", 22);
    g_cur->set1i("u_mhMask", 23);
    g_cur->set1i("u_mhAcc", 24);
    glActiveTexture(GL_TEXTURE23);
    glBindTexture(GL_TEXTURE_2D, g_mask.id);
    glActiveTexture(GL_TEXTURE0);
  }
  glDisable(GL_CULL_FACE);  // open garments are seen from inside too
}

void draw(const Mat4& base, const Look& look, const Pose& pose, float dist, bool shadow) {
  if (!g_ok || !g_cur) return;
  Mat4 D[NJ_MAX], B[NJ_MAX];
  evaluate(pose, D);
  for (int j = 0; j < g_nJ; j++) B[j] = base * D[j];
  glUniformMatrix4fv(g_cur->loc("u_bones"), g_nJ, GL_FALSE, B[0].m);
  int k = (int)look.kind;
  if (!shadow) {
    g_cur->set3f("u_tunic", look.tunic.x, look.tunic.y, look.tunic.z);
    g_cur->set3f("u_trou", look.trousers.x, look.trousers.y, look.trousers.z);
    g_cur->set3f("u_shoe", look.shoes.x, look.shoes.y, look.shoes.z);
    g_cur->set3f("u_sash", look.sash.x, look.sash.y, look.sash.z);
    g_cur->set3f("u_cap", look.cap.x, look.cap.y, look.cap.z);
    g_cur->set1i("u_part", 0);
    g_cur->set1f("u_skinK", look.skin);
    glActiveTexture(GL_TEXTURE22);
    glBindTexture(GL_TEXTURE_2D, g_skin[k == WOMAN ? 1 : k == ELDER ? 2 : 0].id);
    glActiveTexture(GL_TEXTURE24);
    glBindTexture(GL_TEXTURE_2D, g_acc[k == WOMAN ? 1 : k == BANDIT ? 2 : 0].id);
    glActiveTexture(GL_TEXTURE0);
  }
  glBindVertexArray(g_body[k].vao);
  glDrawElements(GL_TRIANGLES, g_body[k].n, GL_UNSIGNED_INT, nullptr);
  gfx::drawCalls++;
  gfx::triangles += g_body[k].n / 3;
  if (!shadow && dist < 22 && g_eyes.n) {
    g_cur->set1i("u_part", 1);
    glActiveTexture(GL_TEXTURE22);
    glBindTexture(GL_TEXTURE_2D, g_eyeTex.id);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(g_eyes.vao);
    glDrawElements(GL_TRIANGLES, g_eyes.n, GL_UNSIGNED_INT, nullptr);
    gfx::drawCalls++;
    if (g_brows.n && g_browTex.id && k != BANDIT) {
      g_cur->set1i("u_part", 2);
      glActiveTexture(GL_TEXTURE22);
      glBindTexture(GL_TEXTURE_2D, g_browTex.id);
      glActiveTexture(GL_TEXTURE0);
      glBindVertexArray(g_brows.vao);
      glDrawElements(GL_TRIANGLES, g_brows.n, GL_UNSIGNED_INT, nullptr);
      gfx::drawCalls++;
    }
  }
  glBindVertexArray(0);
}

Mat4 headFrame(const Mat4& base, const Pose& pose) {
  if (!g_ok) return base;
  Mat4 D[NJ_MAX];
  evaluate(pose, D);
  int j = jointIndex("head");
  // the game's stylised heads have their origin at the top of the neck: put it just under the MakeHuman head pivot
  const Vec3 o{g_piv[j].x, g_piv[j].y - 0.085f, g_piv[j].z - 0.03f};
  return base * D[j] * Mat4::translate(o) * Mat4::scale({HEAD_SCALE, HEAD_SCALE, HEAD_SCALE});
}

Vec3 jointWorld(const Mat4& base, const Pose& pose, const char* joint) {
  if (!g_ok) return base.transformPoint({0, 1, 0});
  Mat4 D[NJ_MAX];
  evaluate(pose, D);
  int j = jointIndex(joint);
  if (j < 0) return base.transformPoint({0, 1, 0});
  return (base * D[j]).transformPoint(g_piv[j]);
}

void end() {
  glEnable(GL_CULL_FACE);
  g_cur = nullptr;
}

void shutdown() {
  for (auto* p : {&g_body[0], &g_body[1], &g_body[2], &g_body[3], &g_eyes, &g_brows}) {
    if (p->vao) glDeleteVertexArrays(1, &p->vao);
    if (p->vbo) glDeleteBuffers(1, &p->vbo);
    if (p->ebo) glDeleteBuffers(1, &p->ebo);
    *p = Part{};
  }
  for (auto& t : g_skin) t.destroy();
  g_eyeTex.destroy(); g_browTex.destroy(); g_mask.destroy();
  for (auto& t : g_acc) t.destroy();
  g_tried = false; g_ok = false;
}
}  // namespace mh
