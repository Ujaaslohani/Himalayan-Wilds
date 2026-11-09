#include "skinned.h"
#include "render.h"
#include "../gfx/gfx.h"
#include "../platform/platform.h"
#include <cstring>
#include <cstdio>
#include <map>
#include <memory>
#include <algorithm>

namespace skinned {
namespace {
const char* SKIN_VS = R"(
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
layout(location = 2) in vec4 a_col;
layout(location = 3) in uvec4 a_joints;
layout(location = 4) in vec4 a_w;
uniform mat4 u_bones[52];
out vec3 v_wpos;
out vec3 v_nrm;
out vec3 v_col;
out vec3 v_bpos;
out float v_depth;
void main(){
  v_bpos = a_pos;
  mat4 sk = u_bones[a_joints.x] * a_w.x + u_bones[a_joints.y] * a_w.y + u_bones[a_joints.z] * a_w.z + u_bones[a_joints.w] * a_w.w;
  vec4 wp = sk * vec4(a_pos, 1.0);
  v_wpos = wp.xyz;
  v_nrm = mat3(sk) * a_nrm;
  v_col = a_col.rgb;
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
const char* SKIN_FS = R"(
in vec3 v_wpos;
in vec3 v_nrm;
in vec3 v_col;
in vec3 v_bpos;
in float v_depth;
uniform float u_spots;  // >0: leopard rosettes, cells per model unit
out vec4 fragColor;
vec3 hash3(vec3 p) {
  p = vec3(dot(p, vec3(127.1, 311.7, 74.7)), dot(p, vec3(269.5, 183.3, 246.1)), dot(p, vec3(113.5, 271.9, 124.6)));
  return fract(sin(p) * 43758.5453);
}
// rosettes: broken dark rings round a slightly deeper tan centre, laid out on a jittered 3D cell grid in the
// bind pose so they ride along with the skin
float rosette(vec3 p) {
  vec3 i = floor(p), f = fract(p);
  float d1 = 9.0; vec3 h1 = vec3(0.0);
  for (int z = -1; z <= 1; z++) for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
    vec3 g = vec3(x, y, z), h = hash3(i + g), r = g + 0.15 + 0.7 * h - f;
    float d = dot(r, r);
    if (d < d1) { d1 = d; h1 = h; }
  }
  d1 = sqrt(d1);
  float rad = 0.22 + 0.08 * h1.x, w = 0.07;
  float ring = smoothstep(rad - w, rad - w * 0.4, d1) * (1.0 - smoothstep(rad + w * 0.4, rad + w, d1));
  float a = atan(f.y - 0.5, f.x - 0.5) * 3.0 + h1.y * 6.283;
  ring *= smoothstep(-0.3, 0.2, sin(a));             // break the ring into 2-4 blotches
  float solid = 1.0 - smoothstep(0.08, 0.13, d1);    // a few small solid spots
  return clamp(max(ring, solid * step(0.7, h1.z)), 0.0, 1.0) + 0.25 * (1.0 - smoothstep(0.0, rad, d1)) * (1.0 - step(0.7, h1.z));
}
void main(){
  g_hwVel = vec4(0.0, 0.0, 2.0, 1.0);  // moving thing without its own motion vector: temporal upscaling trusts this frame more
#ifdef SHADOW_PASS
  fragColor = vec4(1.0);
#else
  vec3 n = normalize(v_nrm);
  if (!gl_FrontFacing) n = -n;
  vec3 alb = srgbToLinear(v_col);
  if (u_spots > 0.0) {
    float sat = max(max(v_col.r, v_col.g), v_col.b) - min(min(v_col.r, v_col.g), v_col.b);
    float m = smoothstep(0.08, 0.2, sat);           // tan coat only: the white belly and muzzle stay clean
    float r = rosette(v_bpos * u_spots);
    alb = mix(alb, alb * vec3(0.10, 0.08, 0.06), clamp(r, 0.0, 1.0) * m);
  }
  float sh = shadowAt(v_wpos, n);
  vec3 lit = lambert(alb, n, sh);
  // soft fur rim, like the hand-built animals
  vec3 V = normalize(u_camPos - v_wpos);
  float rim = pow(1.0 - max(dot(n, V), 0.0), 3.0);
  lit += rim * 0.18 * alb * (u_hemiSky.rgb + u_sunCol.rgb * 0.25);
  vec3 c = outColor(lit);
  c = mix(c, fogTint(v_wpos), fogObject(v_wpos, v_depth));
  fragColor = vec4(c, 1.0);
#endif
}
)";

struct Vtx { float p[3], n[3]; uint8_t c[4], j[4]; float w[4]; };
std::map<std::string, std::unique_ptr<Model>> g_models;

template <class T> bool rd(const std::vector<uint8_t>& b, size_t& o, T& v) {
  if (o + sizeof(T) > b.size()) return false;
  std::memcpy(&v, b.data() + o, sizeof(T));
  o += sizeof(T);
  return true;
}
}  // namespace

int Model::clip(const std::string& n) const {
  for (size_t i = 0; i < clips.size(); i++) if (clips[i].name == n) return (int)i;
  return -1;
}
int Model::joint(const std::string& n) const {
  for (size_t i = 0; i < jointName.size(); i++) if (jointName[i] == n) return (int)i;
  return -1;
}

static void sampleLocal(const Clip& c, int nJ, float t, float* out) {
  float f = t * c.fps;
  int n = c.frames;
  int f0, f1;
  float u;
  if (c.loop) {
    f = std::fmod(f, (float)n);
    if (f < 0) f += n;
    f0 = (int)f; f1 = (f0 + 1) % n; u = f - f0;
  } else {
    f = std::max(0.0f, std::min(f, (float)(n - 1)));
    f0 = (int)f; f1 = std::min(n - 1, f0 + 1); u = f - f0;
  }
  const float* a = &c.keys[(size_t)f0 * nJ * 10];
  const float* b = &c.keys[(size_t)f1 * nJ * 10];
  for (int j = 0; j < nJ; j++) {
    const float* ka = a + j * 10; const float* kb = b + j * 10;
    float* o = out + j * 10;
    for (int k = 0; k < 3; k++) o[k] = ka[k] + (kb[k] - ka[k]) * u;
    float d = ka[3] * kb[3] + ka[4] * kb[4] + ka[5] * kb[5] + ka[6] * kb[6], s = d < 0 ? -1.0f : 1.0f;
    for (int k = 3; k < 7; k++) o[k] = ka[k] + (kb[k] * s - ka[k]) * u;
    for (int k = 7; k < 10; k++) o[k] = ka[k] + (kb[k] - ka[k]) * u;
  }
}

void evaluate(const Model& m, int clipA, float tA, int clipB, float tB, float b, Pose& out) {
  static thread_local std::vector<float> A, B;
  int nJ = std::min(m.nJ, MAX_BONES);
  A.resize((size_t)m.nJ * 10);
  B.resize((size_t)m.nJ * 10);
  sampleLocal(m.clips[clipA], m.nJ, tA, A.data());
  if (b > 0.001f && clipB >= 0) {
    sampleLocal(m.clips[clipB], m.nJ, tB, B.data());
    for (int j = 0; j < m.nJ; j++) {
      float* a = &A[j * 10]; const float* c = &B[j * 10];
      float d = a[3] * c[3] + a[4] * c[4] + a[5] * c[5] + a[6] * c[6], s = d < 0 ? -1.0f : 1.0f;
      for (int k = 0; k < 10; k++) a[k] += ((k >= 3 && k < 7 ? c[k] * s : c[k]) - a[k]) * b;
    }
  }
  for (int j = 0; j < nJ; j++) {
    const float* k = &A[j * 10];
    Mat4 local = Mat4::compose({k[0], k[1], k[2]}, Quat(k[3], k[4], k[5], k[6]).normalized(), {k[7], k[8], k[9]});
    out.world[j] = m.parent[j] >= 0 ? out.world[m.parent[j]] * local : m.pre[j] * local;
    out.skin[j] = out.world[j] * m.ibm[j];
  }
}

const Model* load(const std::string& path) {
  auto it = g_models.find(path);
  if (it != g_models.end()) return it->second->ok ? it->second.get() : nullptr;
  auto M = std::make_unique<Model>();
  std::vector<uint8_t> b;
  Model* res = M.get();
  g_models[path] = std::move(M);
  if (!platform::readFile(platform::assetPath(path), b) || b.size() < 24 || std::memcmp(b.data(), "HWAN", 4) != 0) {
    std::fprintf(stderr, "animated model missing: %s\n", path.c_str());
    return nullptr;
  }
  size_t o = 4;
  uint32_t ver, nv, ni, nj, nc;
  rd(b, o, ver); rd(b, o, nv); rd(b, o, ni); rd(b, o, nj); rd(b, o, nc);
  if (ver != 2 || nj > MAX_BONES) { std::fprintf(stderr, "animated model %s: unsupported\n", path.c_str()); return nullptr; }
  std::vector<Vtx> V(nv);
  for (auto& v : V) {
    rd(b, o, v.p); rd(b, o, v.n); rd(b, o, v.c); rd(b, o, v.j); rd(b, o, v.w);
  }
  std::vector<uint32_t> I(ni);
  for (auto& i : I) rd(b, o, i);
  res->nJ = (int)nj;
  res->parent.resize(nj); res->ibm.resize(nj); res->pre.resize(nj); res->jointName.resize(nj);
  for (uint32_t j = 0; j < nj; j++) {
    rd(b, o, res->parent[j]);
    rd(b, o, res->ibm[j].m);
    rd(b, o, res->pre[j].m);
  }
  for (uint32_t j = 0; j < nj; j++) {
    char nm[33] = {};
    if (o + 32 <= b.size()) std::memcpy(nm, b.data() + o, 32);
    o += 32;
    res->jointName[j] = nm;
  }
  for (uint32_t c = 0; c < nc; c++) {
    Clip C;
    char nm[33] = {};
    if (o + 32 > b.size()) return nullptr;
    std::memcpy(nm, b.data() + o, 32);
    o += 32;
    C.name = nm;
    uint32_t nf, loop;
    rd(b, o, C.fps); rd(b, o, nf); rd(b, o, loop);
    C.frames = (int)nf;
    C.loop = loop != 0;
    size_t n = (size_t)nf * nj * 10;
    if (o + n * 4 > b.size()) return nullptr;
    C.keys.resize(n);
    std::memcpy(C.keys.data(), b.data() + o, n * 4);
    o += n * 4;
    res->clips.push_back(std::move(C));
  }
  if (res->clips.empty()) return nullptr;
  // height of the rest pose (first frame of the first clip) for scaling to real-world size
  {
    Pose P;
    res->ok = true;
    evaluate(*res, 0, 0, -1, 0, 0, P);
    float top = 0;
    for (auto& v : V) {
      float y = 0;
      for (int k = 0; k < 4; k++) {
        if (v.w[k] <= 0) continue;
        const Mat4& S = P.skin[v.j[k]];
        y += v.w[k] * (S.m[1] * v.p[0] + S.m[5] * v.p[1] + S.m[9] * v.p[2] + S.m[13]);
      }
      top = std::max(top, y);
    }
    res->height = std::max(0.1f, top);
  }
  glGenVertexArrays(1, &res->vao);
  glBindVertexArray(res->vao);
  glGenBuffers(1, &res->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, res->vbo);
  glBufferData(GL_ARRAY_BUFFER, V.size() * sizeof(Vtx), V.data(), GL_STATIC_DRAW);
  glGenBuffers(1, &res->ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res->ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, I.size() * 4, I.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, p));
  glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, n));
  glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vtx), (void*)offsetof(Vtx, c));
  glEnableVertexAttribArray(3); glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, sizeof(Vtx), (void*)offsetof(Vtx, j));
  glEnableVertexAttribArray(4); glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, w));
  glBindVertexArray(0);
  gfx::addBufferBytes((long long)(V.size() * sizeof(Vtx) + I.size() * 4));
  res->nIdx = (int)I.size();
  std::printf("animated model %s: %u verts, %u joints, %u clips\n", path.c_str(), nv, nj, nc);
  return res;
}

static Shader* g_cur = nullptr;
void beginDraw(bool shadow) {
  g_cur = shadow ? &render::program("skin_shadow", SKIN_VS, SKIN_FS, "#define SHADOW_PASS\n") : &render::program("skin", SKIN_VS, SKIN_FS);
  g_cur->use();
}

void draw(const Model& m, const Pose& p, const Mat4& xf, bool) {
  if (!g_cur || !m.ok) return;
  static Mat4 bones[MAX_BONES];
  int n = std::min(m.nJ, MAX_BONES);
  for (int j = 0; j < n; j++) bones[j] = xf * p.skin[j];
  glUniformMatrix4fv(g_cur->loc("u_bones"), n, GL_FALSE, bones[0].m);
  glUniform1f(g_cur->loc("u_spots"), m.spots);
  glBindVertexArray(m.vao);
  glDrawElements(GL_TRIANGLES, m.nIdx, GL_UNSIGNED_INT, nullptr);
  gfx::drawCalls++;
  gfx::triangles += m.nIdx / 3;
  glBindVertexArray(0);
}
}  // namespace skinned
