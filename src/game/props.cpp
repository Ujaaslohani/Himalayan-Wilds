#include "props.h"
#include "engine.h"
#include "npcs.h"
#include "village.h"
#include "../world/terrain.h"
#include "../world/shadows.h"
#include "../models/models.h"
#include "../models/temple.h"
#include "../models/houses.h"
#include "../render/render.h"
#include "../render/terrain_render.h"
#include "../audio/audio.h"
#include "../ui/font.h"
#include <set>

namespace {

Mesh g_stairs;
CulledBatch g_posts, g_rails, g_marks, g_temples, g_templeGlow, g_clueCairns;
Mesh g_post, g_rail, g_signMesh;
Texture g_signTex;
std::vector<CastInstance> g_casts;
std::set<std::string> g_reached;

float snoise(float x, float y, float z) { return std::sin(x * 2.1f + std::sin(z * 1.7f + y * 0.9f) * 1.6f) * std::sin(z * 2.3f + std::sin(x * 1.3f - y * 1.1f) * 1.4f); }

MeshData buildStairs() {
  std::vector<VtxP3C4N4> verts;
  std::vector<uint32_t> idx;
  Color side = Color::hex("8f7048"), side2 = Color::hex("7b5f3d"), deep = Color::hex("5e5850");
  for (size_t i = 0; i < world::STEPS.size(); i++) {
    const Step& S = world::STEPS[i];
    float bottom = std::min(S.bottom, world::getTerrainHeight(S.x, S.z) - 0.5f);
    float hgt = S.top - bottom;
    Vec3 A0(S.hw, hgt / 2, S.hl);
    float r = std::min({0.42f, hgt / 2.2f, S.hl * 0.55f, S.hw * 0.3f});
    int sy = std::max(2, std::min(8, (int)std::ceil(hgt / 1.6f)));
    Geo box = boxGeo(1, 1, 1, 6, sy, 4);
    Quat q = Quat::axisAngle({0, 1, 0}, std::atan2(S.dx, S.dz));
    Vec3 c(S.x, bottom + hgt / 2, S.z);
    Color top = world::terrainColor(S.x, S.z, S.top, 1);
    uint32_t base = (uint32_t)verts.size();
    for (size_t k = 0; k < box.pos.size(); k++) {
      Vec3 p = box.pos[k].mul(A0) * 2.0f;
      Vec3 inner(clampf(p.x, -A0.x + r, A0.x - r), clampf(p.y, -A0.y + r, A0.y - r), clampf(p.z, -A0.z + r, A0.z - r));
      Vec3 d = p - inner;
      Vec3 n;
      if (d.lengthSq() > 1e-8f) { n = d.normalized(); p = inner + n * r; }
      else n = box.nrm[k];
      Vec3 w = q.rotate(p) + c;
      Vec3 wn = q.rotate(n);
      if (n.y < 0.6f) w += wn * (snoise(w.x, w.y, w.z) * 0.09f * (1 - std::max(0.0f, n.y)));
      float below = S.top - w.y;
      Color cc;
      if (wn.y > 0.6f && below < 0.5f) cc = top.offsetHSL(0, 0, snoise(w.x * 3, 0, w.z * 3) * 0.025f);
      else cc = (i % 2 ? side : side2).offsetHSL(0, 0, snoise(w.x, w.y * 2.5f, w.z) * 0.04f).lerp(deep, clampf((below - 1.5f) / 6, 0, 1)).lerp(top, std::max(0.0f, 1 - below * 2.2f) * 0.85f);
      VtxP3C4N4 v;
      v.p[0] = w.x; v.p[1] = w.y; v.p[2] = w.z;
      v.c = cc.packSrgb();
      v.n[0] = (int8_t)std::lround(wn.x * 127); v.n[1] = (int8_t)std::lround(wn.y * 127); v.n[2] = (int8_t)std::lround(wn.z * 127); v.n[3] = 0;
      verts.push_back(v);
    }
    for (uint32_t id : box.idx) idx.push_back(base + id);
  }
  MeshData d;
  d.fmt = VFmt::P3C4N4;
  d.vertexCount = (int)verts.size();
  d.verts.resize(verts.size() * sizeof(VtxP3C4N4));
  if (!verts.empty()) std::memcpy(d.verts.data(), verts.data(), d.verts.size());
  d.idx = idx;
  return d;
}

struct SignSpec { float x, y, z, yaw, scale; std::string lines[3]; };

void buildSigns(const std::vector<SignSpec>& list) {
  g_signMesh.destroy();
  g_signTex.destroy();
  const int COLS = 2, ROWS = 6, SW = 256, SH = 112;
  int n = std::min((int)list.size(), COLS * ROWS);
  if (!n) return;
  Canvas cv;
  cv.init(COLS * SW, ROWS * SH);
  FontFace* serif = fonts::serif();
  for (int i = 0; i < n; i++) {
    int ox = (i % COLS) * SW, oy = (i / COLS) * SH;
    cv.vgradient(ox, oy, SW, SH, rgbHex("#8a6038"), rgbHex("#6e4a2a"));
    for (int k = 0; k < 7; k++) {
      float yy = oy + 8 + k * 15 + std::sin(i * 3.0f + k) * 3;
      cv.bezier((float)ox + 4, yy, (float)ox + 80, yy + 4, (float)ox + 170, yy - 4, (float)ox + SW - 4, yy + 2, 1, (rgbHex("#28180c") & 0x00ffffff) | (89u << 24));
    }
    cv.strokeRect((float)ox + 3, (float)oy + 3, SW - 6, SH - 6, 6, rgbHex("#3b2414"));
    std::string a = list[i].lines[0];
    for (auto& ch : a) ch = (char)std::toupper((unsigned char)ch);
    cv.text(serif, a, 15, ox + SW / 2.0f, oy + 24.0f, rgbHex("#f6e7c4"));
    float size = 30;
    while (fonts::measure(serif, list[i].lines[1], size) > SW - 24 && size > 14) size -= 2;
    cv.text(serif, list[i].lines[1], size, ox + SW / 2.0f, oy + 56.0f, rgbHex("#fff4d6"));
    cv.text(serif, list[i].lines[2], 15, ox + SW / 2.0f, oy + 88.0f, rgbHex("#ead6a8"));
  }
  TexLevels L = prepareTexture(cv.px.data(), cv.w, cv.h, true, false, false);
  g_signTex = uploadPrepared(L, false);
  std::vector<VtxGlb> verts;
  std::vector<uint32_t> idx;
  float corners[4][4] = {{-0.43f, -0.235f, 0, 0}, {0.43f, -0.235f, 1, 0}, {0.43f, 0.235f, 1, 1}, {-0.43f, 0.235f, 0, 1}};
  for (int i = 0; i < n; i++) {
    const SignSpec& S = list[i];
    Quat q = Quat::axisAngle({0, 1, 0}, S.yaw);
    Mat4 m = Mat4::compose({S.x, S.y, S.z}, q, {S.scale, S.scale, S.scale});
    Vec3 nn = q.rotate({0, 0, 1});
    float u0 = (float)(i % COLS) / COLS, v0 = (float)(i / COLS) / ROWS;
    for (int side = 0; side < 2; side++) {
      float sg = side ? -1.0f : 1.0f;
      uint32_t b = (uint32_t)verts.size();
      for (int k = 0; k < 4; k++) {
        Vec3 v = m.transformPoint({0.7f + corners[k][0] * sg, 1.52f + corners[k][1], side ? 0.222f : 0.288f});
        VtxGlb o;
        o.p[0] = v.x; o.p[1] = v.y; o.p[2] = v.z;
        o.n[0] = (int8_t)std::lround(nn.x * sg * 127); o.n[1] = (int8_t)std::lround(nn.y * sg * 127); o.n[2] = (int8_t)std::lround(nn.z * sg * 127); o.n[3] = 0;
        o.uv[0] = floatToHalf(u0 + corners[k][2] / COLS);
        o.uv[1] = floatToHalf(v0 + (1 - corners[k][3]) / ROWS);
        o.c = 0xffffffffu;
        verts.push_back(o);
      }
      idx.insert(idx.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    }
  }
  MeshData d;
  d.fmt = VFmt::GLB;
  d.vertexCount = (int)verts.size();
  d.verts.resize(verts.size() * sizeof(VtxGlb));
  std::memcpy(d.verts.data(), verts.data(), d.verts.size());
  d.idx = idx;
  g_signMesh = uploadMesh(d);
}

}  // namespace

namespace props {

void init() {
  g_stairs = uploadMesh(buildStairs());
  temple::model();
  g_temples.init(&temple::model(), &temple::modelLod());
  g_temples.mid = 60; g_temples.band = 12;
  g_templeGlow.init(&temple::glow());
  for (auto& sh : world::SHRINES) {
    Mat4 m = Mat4::compose({sh.x, sh.y - 0.05f, sh.z}, Quat::axisAngle({0, 1, 0}, sh.yaw), {1, 1, 1});
    g_temples.add(m, 16);
    g_templeGlow.add(m, 16);
  }
  Shape post, rail;
  post.add(translated(boxGeo(0.16f, 1.15f, 0.16f), {0, 0.5f, 0}), "#6b4c30");
  rail.add(boxGeo(0.08f, 0.12f, 1), "#6b4c30");
  g_post = uploadMesh(post.build(false));
  g_rail = uploadMesh(rail.build(false));
  g_temples.outer = 1200; g_templeGlow.outer = 600;
  g_posts.outer = 140; g_rails.outer = 140; g_marks.outer = 200; g_clueCairns.outer = 200;
  g_posts.init(&g_post);
  g_rails.init(&g_rail);
  for (auto& run : world::fenceLines()) {
    for (size_t i = 0; i < run.size(); i++) {
      Vec3 p = run[i];
      g_posts.add(Mat4::compose({p.x, p.y - 0.1f, p.z}, Quat::axisAngle({0, 1, 0}, i * 0.7f), {1, 1 + ((i * 7) % 3) * 0.06f, 1}), 1);
      if (i == 0) continue;
      Vec3 pp = run[i - 1];
      float len = hypot2(p.x - pp.x, p.z - pp.z);
      Quat q = Quat::axisAngle({0, 1, 0}, std::atan2(p.x - pp.x, p.z - pp.z));
      for (float hh : {0.45f, 0.88f}) g_rails.add(Mat4::compose({(p.x + pp.x) / 2, (p.y + pp.y) / 2 + hh, (p.z + pp.z) / 2}, q, {1, 1, len + 0.1f}), len * 0.5f + 0.5f);
    }
  }
  g_marks.init(&models::cairn());
  for (auto& r : world::ROUTES) {
    float x = r.pts[0][0], z = r.pts[0][1];
    g_marks.add(Mat4::compose({x, world::getTerrainHeight(x, z), z}, Quat::axisAngle({0, 1, 0}, std::atan2(-x, -z)), {0.8f, 0.8f, 0.8f}), 1.5f);
  }
  g_clueCairns.init(&models::cairn());
  registerMarkers("summits", [](std::vector<Marker>& out) {
    const Vec3& p = E.player.pos;
    for (auto& s : world::SUMMITS) {
      if (g_reached.count(s.name)) continue;
      if (hypot2(s.x - p.x, s.z - p.z) < 160) out.push_back({s.x, s.h + 3, s.z, MI_CLUE, s.name});
    }
  });
  rebuildLandmarks();
}

void rebuildLandmarks() {
  g_clueCairns.clear();
  std::vector<SignSpec> list;
  for (auto& r : world::ROUTES) {
    float x = r.pts[0][0], z = r.pts[0][1];
    list.push_back({x, world::getTerrainHeight(x, z), z, std::atan2(-x, -z), 0.8f, {"Temple trail", r.name, std::to_string((int)std::lround(r.len)) + " m to the summit"}});
  }
  for (auto& c : A.clues) {
    std::string kind = c.id.rfind("track", 0) == 0 ? "Hunting ground" : c.id.rfind("ruins", 0) == 0 ? "Old ruins" : c.id.rfind("falls", 0) == 0 ? "Waterfall" : "Viewpoint";
    float y = world::getTerrainHeight(c.x, c.z);
    list.push_back({c.x, y, c.z, c.x * 0.37f, 1, {kind, c.title, "Press E to inspect"}});
    g_clueCairns.add(Mat4::compose({c.x, y, c.z}, Quat::axisAngle({0, 1, 0}, c.x * 0.37f), {1, 1, 1}), 1.5f);
  }
  buildSigns(list);
}

void rebuildCasters() {
  g_casts.clear();
  auto put = [&](float x, float z, float r, float h, float y) {
    if (g_casts.size() < 256) g_casts.push_back(castInstance(x, y, z, r, h));
  };
  auto putT = [&](float x, float z, float r, float h) { put(x, z, r, h, world::getTerrainHeight(x, z)); };
  for (auto& sh : world::SHRINES) put(sh.x, sh.z, 10, 15, sh.y + 0.2f);
  for (auto& b : A.buildings)
    if (b.built && b.type != "bridge" && b.type != "campfire") putT(b.pos[0], b.pos[2], 3.8f, 5.5f);
  for (auto& st : A.structures) {
    if (st.kind == SK_PATH) continue;
    bool small = st.kind == SK_FENCE || st.kind == SK_LAMP_POST || st.kind == SK_BENCH || st.kind == SK_FLAGPOLE;
    putT(st.x, st.z, small ? 0.5f : structureRadius(st.kind) * 0.8f, small ? 1.6f : 5);
  }
  for (auto& c : npcs::CAMPS)
    if (std::string(c.id) != "hunter") putT(c.campX - 2.2f, c.campZ - 1.2f, 1.5f, 2.2f);
}

static void drawWorld(bool shadow) {
  if (!shadow) {
    glDisable(GL_CULL_FACE);
    terrain_render::drawGroundMesh(g_stairs);
    for (CulledBatch* b : {&g_temples, &g_templeGlow, &g_posts, &g_rails, &g_marks, &g_clueCairns}) b->cull();
  }
  glEnable(GL_CULL_FACE);
  Shader& s1 = render::object(VFmt::P3C4, IM_FULL, SeasonKind::Build, OF_WIND);
  for (CulledBatch* b : {&g_temples, &g_posts, &g_rails}) shadow ? b->drawShadow() : b->draw(s1);
  Shader& s2 = render::object(VFmt::P3C4, IM_FULL, SeasonKind::None, 0);
  for (CulledBatch* b : {&g_marks, &g_clueCairns}) shadow ? b->drawShadow() : b->draw(s2);
  if (!shadow && g_signMesh.valid()) {
    AssetMaterial m;
    m.tex = &g_signTex;
    m.kind = SeasonKind::None;
    Shader& s = render::object(VFmt::GLB, IM_NONE, SeasonKind::None, 0);
    render::bindMaterial(s, m);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1, -1);
    g_signMesh.draw();
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
}

void draw() { drawWorld(false); }
void drawShadow() { drawWorld(true); }

void drawGlow() {
  float night = clampf((0.5f - E.daylight) / 0.4f, 0, 1);
  float k = 0.4f + 0.75f * night;
  Shader& s = render::object(VFmt::P3C4, IM_FULL, SeasonKind::None, 0);
  (void)s;
  glEnable(GL_CULL_FACE);
  Shader& b = render::program("templeglow", R"(
in vec3 a_pos;
in vec4 a_col;
in vec4 a_i0;
in vec4 a_i1;
in vec4 a_i2;
out vec3 v_col;
void main(){
  mat4 M = mat4(vec4(a_i0.x, a_i1.x, a_i2.x, 0.0), vec4(a_i0.y, a_i1.y, a_i2.y, 0.0), vec4(a_i0.z, a_i1.z, a_i2.z, 0.0), vec4(a_i0.w, a_i1.w, a_i2.w, 1.0));
  v_col = srgbToLinear(a_col.rgb);
  gl_Position = u_viewProj * (M * vec4(a_pos, 1.0));
}
)", R"(
in vec3 v_col;
uniform float u_k;
out vec4 fragColor;
void main(){ fragColor = vec4(outColor(v_col * u_k), 1.0); }
)");
  b.set1f("u_k", k);
  g_templeGlow.nearB.draw();
}

void drawCasters() { drawCasts(g_casts); }

void checkSummits() {
  const Vec3& p = E.player.pos;
  for (auto& s : world::SUMMITS) {
    if (g_reached.count(s.name)) continue;
    if (hypot2(s.x - p.x, s.z - p.z) < 7 && p.y > s.h - 2) {
      g_reached.insert(s.name);
      actions::addScore(400);
      popup(s.name + " · " + std::to_string((int)std::lround(s.h * 38 + 2400)) + " m", rgbHex("#e0f2fe"));
      actions::notify("You reached the summit of " + s.name + "! The whole valley lies below you. +400 renown.");
      audio::victoryFanfare();
    }
  }
}

bool summitReached(const char* name) { return g_reached.count(name) > 0; }
void markSummit(const char* name) { g_reached.insert(name); }
void resetSummits() { g_reached.clear(); }

void shutdown() {
  g_stairs.destroy();
  g_post.destroy();
  g_rail.destroy();
  g_signMesh.destroy();
  g_signTex.destroy();
}

}  // namespace props
