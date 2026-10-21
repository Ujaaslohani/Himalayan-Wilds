#include "people.h"
#include <map>
#include <memory>
#include <functional>

const Vec3 PIV_HEAD{0, 1.44f + PEOPLE_LIFT, 0};
const Vec3 PIV_ARM{0.235f, 1.3f + PEOPLE_LIFT, 0};
const Vec3 PIV_LEG{0.1f, 0.62f + PEOPLE_LIFT, 0};

namespace {

using V = Vec3;
const V Z0{0, 0, 0};

struct Outfit {
  const char *coat, *vest, *trim, *sash, *shawl, *stripe, *cap;
  const char* band[3];
  const char *skin, *hair, *trousers, *shoe;
  bool female;
  const char* beard;
};

const Outfit OUT_HUNTER{"#1f4a2c", "#1b1a1c", "#efe6d2", "#c2410c", "#7c6a4f", "#b91c1c", "#2a2a2e", {"#15803d", "#facc15", "#b91c1c"}, "#c48a62", "#16100c", "#ece4d2", "#4a2e1c", false, "#16100c"};
const Outfit OUT_CARPENTER{"#e8dfcc", "#7a4b2a", "#f0e6d2", "#1e3a8a", "#d6c9ad", "#1e3a8a", "#6b6f76", {"#7f1d1d", "#f59e0b", "#1e40af"}, "#d1a07c", "#2b1d14", "#ebe2cf", "#4a3426", false, nullptr};
const Outfit OUT_HERBALIST{"#9f1239", "#14532d", "#f5e9d3", "#15803d", "#f1e4c8", "#9f1239", "#15803d", {"#facc15", "#9f1239", "#1d4ed8"}, "#d6a585", "#120c09", "#7c2d12", "#5a3d2b", true, nullptr};
const Outfit OUT_BUILDER{"#57534e", "#292524", "#e7e0d0", "#ca8a04", "#8a7f73", "#ca8a04", "#3f3f46", {"#1e3a8a", "#dc2626", "#facc15"}, "#b98262", "#c9c4bb", "#d8cfbf", "#2f241c", false, "#d6d3cd"};

const Outfit& outfitFor(const std::string& role) {
  if (role == "carpenter") return OUT_CARPENTER;
  if (role == "herbalist") return OUT_HERBALIST;
  if (role == "builder") return OUT_BUILDER;
  return OUT_HUNTER;
}

const char* GOLD = "#d4af37";
const char* EYE = "#f4f1ea";

float smf(float a, float b, float x) {
  float t = clampf((x - a) / (b - a), 0, 1);
  return t * t * (3 - 2 * t);
}

void chip(Shape& t, const char* color, V pos, float size, V rot) { t.add(planeGeo(size, size), color, pos, rot); }

void patternRing(Shape& t, float y, float r, int n, const std::vector<const char*>& cols, float size) {
  for (int i = 0; i < n; i++) {
    float a = ((float)i / n) * TAU;
    chip(t, cols[i % cols.size()], {std::sin(a) * (r + 0.004f), y, std::cos(a) * (r + 0.004f)}, size, {0, a, PI / 4});
  }
}

void torsoMale(Shape& t, const Outfit& o) {
  t.tube(o.coat, {0, 0.67f, 0}, 0.255f, 0.355f, 0.34f, 12)
      .tube(o.trim, {0, 0.505f, 0}, 0.358f, 0.358f, 0.025f, 12)
      .tube(o.stripe, {0, 0.535f, 0}, 0.352f, 0.352f, 0.018f, 12)
      .tube(o.coat, {0, 0.98f, 0}, 0.235f, 0.252f, 0.34f, 12)
      .ball(o.coat, {0, 1.13f, 0.0f}, {0.255f, 0.2f, 0.185f}, Z0, 1)
      .tube(o.vest, {0, 1.04f, -0.005f}, 0.248f, 0.262f, 0.4f, 12)
      .box(o.coat, {0, 1.05f, 0.236f}, {0.1f, 0.4f, 0.02f})
      .box(o.trim, {0.055f, 1.05f, 0.242f}, {0.014f, 0.4f, 0.012f})
      .box(o.trim, {-0.055f, 1.05f, 0.242f}, {0.014f, 0.4f, 0.012f})
      .tube(o.sash, {0, 0.83f, 0}, 0.262f, 0.268f, 0.085f, 12)
      .ball(o.sash, {0.13f, 0.82f, 0.23f}, {0.05f, 0.04f, 0.03f}, Z0, 0)
      .box(o.sash, {0.15f, 0.73f, 0.235f}, {0.05f, 0.15f, 0.02f}, {0, 0, 0.12f})
      .box(o.sash, {0.11f, 0.735f, 0.24f}, {0.045f, 0.13f, 0.02f}, {0, 0, -0.18f});
  for (int k = 0; k < 4; k++) t.ball(GOLD, {0.07f, 1.18f - k * 0.09f, 0.245f}, {0.014f, 0.014f, 0.01f}, Z0, 0);
  patternRing(t, 0.565f, 0.343f, 22, {o.band[0], o.band[1], o.band[2]}, 0.026f);
}

void torsoFemale(Shape& t, const Outfit& o, const char* hem) {
  t.tube(o.coat, {0, 0.47f, 0}, 0.255f, 0.43f, 0.66f, 14)
      .tube(hem, {0, 0.16f, 0}, 0.433f, 0.436f, 0.05f, 14)
      .tube(o.band[0], {0, 0.205f, 0}, 0.425f, 0.427f, 0.025f, 14)
      .tube(o.coat, {0, 1.0f, 0}, 0.215f, 0.245f, 0.42f, 12)
      .ball(o.coat, {0, 1.13f, 0.02f}, {0.225f, 0.17f, 0.18f}, Z0, 1)
      .tube(o.vest, {0, 1.03f, -0.004f}, 0.228f, 0.25f, 0.32f, 12)
      .box(o.coat, {0, 1.04f, 0.226f}, {0.08f, 0.32f, 0.02f})
      .tube(o.sash, {0, 0.8f, 0}, 0.252f, 0.258f, 0.08f, 12)
      .box(o.sash, {0.12f, 0.66f, 0.26f}, {0.06f, 0.24f, 0.025f}, {0.12f, 0, 0.1f})
      .box(o.band[1], {0.12f, 0.55f, 0.275f}, {0.065f, 0.03f, 0.026f}, {0.12f, 0, 0.1f});
  patternRing(t, 0.245f, 0.418f, 26, {o.band[1], o.band[2], hem}, 0.03f);
  patternRing(t, 0.115f, 0.43f, 26, {o.band[2], o.band[1]}, 0.022f);
}

void shawl(Shape& t, const Outfit& o) {
  t.tube(o.shawl, {0, 1.26f, 0}, 0.19f, 0.272f, 0.15f, 12)
      .tube(o.stripe, {0, 1.185f, 0}, 0.276f, 0.28f, 0.022f, 12)
      .tube(o.band[1], {0, 1.212f, 0}, 0.27f, 0.272f, 0.01f, 12)
      .box(o.shawl, {-0.03f, 1.05f, 0.205f}, {0.13f, 0.44f, 0.035f}, {0, 0, -0.55f})
      .box(o.stripe, {-0.115f, 0.875f, 0.22f}, {0.13f, 0.025f, 0.035f}, {0, 0, -0.55f})
      .ball(o.shawl, {0.2f, 1.28f, 0}, {0.105f, 0.075f, 0.115f}, Z0, 1)
      .ball(o.shawl, {-0.2f, 1.28f, 0}, {0.105f, 0.075f, 0.115f}, Z0, 1);
  for (int k = 0; k < 5; k++) chip(t, k % 2 ? o.band[0] : o.band[2], {-0.035f - k * 0.022f, 1.2f - k * 0.07f, 0.226f}, 0.026f, {0, 0, PI / 4});
  for (int k = 0; k < 7; k++) t.rod(o.stripe, {-0.13f + k * 0.014f, 0.86f - k * 0.008f, 0.22f}, {-0.135f + k * 0.014f, 0.8f - k * 0.008f, 0.222f}, 0.005f, 0.004f, 3);
}

void neck(Shape& t, const Outfit& o) {
  t.tube(o.trim, {0, 1.335f, 0}, 0.1f, 0.14f, 0.05f, 12).tube(o.skin, {0, 1.4f, 0}, 0.052f, 0.058f, 0.11f, 9);
}

void necklace(Shape& t) {
  for (int i = 0; i < 11; i++) {
    float a = -1.2f + (i / 10.0f) * 2.4f;
    t.ball(i % 3 == 1 ? "#b91c1c" : GOLD, {std::sin(a) * 0.105f, 1.33f - std::cos(a) * 0.035f - (1 - std::fabs(a) / 1.2f) * 0.04f, std::cos(a) * 0.105f + 0.02f}, {0.016f, 0.016f, 0.016f}, Z0, 0);
  }
  t.ball(GOLD, {0, 1.25f, 0.14f}, {0.03f, 0.035f, 0.012f}, Z0, 0);
}

Geo headGeometry(bool female) {
  Geo g = sphereGeo(1, 34, 26);
  const float CX = 0.118f, CY = 0.146f, CZ = 0.128f, OY = 0.145f, OZ = 0.008f;
  for (auto& P : g.pos) {
    float ux = P.x, uy = P.y, uz = P.z;
    float s = std::max(0.0f, -uy);
    float front = std::max(0.0f, uz);
    float narrow = (female ? 0.2f : 0.14f) + (female ? 0.36f : 0.3f) * front;
    float wx = 1 - narrow * smf(0.22f, 1.0f, s);
    wx *= 1 + 0.045f * std::exp(-((uy + 0.05f) * (uy + 0.05f)) / 0.02f) * front;
    wx *= 1 - 0.05f * std::pow(std::fabs(ux), 6.0f) * smf(-0.2f, 0.4f, uy);
    float zf = 1;
    if (uz < 0) zf = 1 - 0.22f * smf(0.35f, 1, s);
    else zf = 1 + (female ? 0.03f : 0.06f) * smf(0.55f, 0.95f, s) * smf(0.6f, 1, front);
    float yy = uy;
    if (uy < 0) yy = uy * (1 - 0.1f * smf(0.6f, 1, s)) - (female ? 0.03f : 0.045f) * smf(0.5f, 1, s) * front;
    float x = ux * CX * wx, y = OY + yy * CY, z = OZ + uz * CZ * zf;
    if (uy < -0.2f && uz < 0.35f) z += 0.012f * smf(0.3f, 0.9f, s);
    float ax = std::fabs(ux) - 0.55f, ay = uy + 0.18f;
    float cheek = std::exp(-((ax * ax) / 0.02f + (ay * ay) / 0.012f)) * front;
    z += cheek * 0.008f;
    P = {x, y, z};
  }
  computeSmoothNormals(g);
  return g;
}

void face(Shape& h, const Outfit& o, bool female) {
  const char* lid = o.skin;
  const char* lip = female ? "#b55a4f" : "#9c5a48";
  Geo head = headGeometry(female);
  auto surf = [&head](float x, float y) {
    float best = -1, bd = 1e9f;
    for (auto& p : head.pos) {
      if (p.z < 0) continue;
      float d = (p.x - x) * (p.x - x) + (p.y - y) * (p.y - y);
      if (d < bd) { bd = d; best = p.z; }
    }
    return best;
  };
  h.add(head, o.skin);
  h.ball(o.skin, {0.116f, 0.14f, -0.006f}, {0.016f, 0.036f, 0.026f}, {0, 0.3f, 0}, 2)
      .ball(o.skin, {-0.116f, 0.14f, -0.006f}, {0.016f, 0.036f, 0.026f}, {0, -0.3f, 0}, 2)
      .rod(o.skin, {0, 0.168f, 0.12f}, {0, 0.122f, 0.137f}, 0.01f, 0.016f, 8)
      .ball(o.skin, {0, 0.117f, 0.135f}, {0.018f, 0.015f, 0.014f}, Z0, 2)
      .ball(o.skin, {0.014f, 0.113f, 0.127f}, {0.01f, 0.009f, 0.01f}, Z0, 2)
      .ball(o.skin, {-0.014f, 0.113f, 0.127f}, {0.01f, 0.009f, 0.01f}, Z0, 2)
      .ball("#4a2a20", {0.009f, 0.108f, 0.13f}, {0.004f, 0.003f, 0.004f}, Z0, 2)
      .ball("#4a2a20", {-0.009f, 0.108f, 0.13f}, {0.004f, 0.003f, 0.004f}, Z0, 2);
  const char* irisCol = female ? "#4a2c18" : "#3b2414";
  for (float sx : {-1.0f, 1.0f}) {
    float x = sx * 0.043f;
    h.ball("#d9a283", {x, 0.159f, 0.115f}, {0.028f, 0.022f, 0.012f}, Z0, 2)
        .ball(EYE, {x, 0.158f, 0.122f}, {0.024f, 0.0165f, 0.012f}, Z0, 2)
        .ball(irisCol, {x, 0.158f, 0.1315f}, {0.0115f, 0.0115f, 0.004f}, Z0, 2)
        .ball("#7a5232", {x, 0.157f, 0.1335f}, {0.0075f, 0.0075f, 0.003f}, Z0, 2)
        .ball("#0b0705", {x, 0.158f, 0.135f}, {0.0045f, 0.0045f, 0.002f}, Z0, 2)
        .ball("#ffffff", {x + sx * 0.0035f, 0.1625f, 0.1365f}, {0.0024f, 0.0024f, 0.001f}, Z0, 1)
        .ball(lid, {x, 0.1745f, 0.122f}, {0.026f, 0.0068f, 0.012f}, Z0, 2)
        .ball(lid, {x, 0.1455f, 0.1235f}, {0.024f, 0.0055f, 0.0105f}, Z0, 2)
        .box("#1a100b", {x, 0.1685f, 0.1335f}, {0.044f, 0.0022f, 0.003f}, {0, 0, sx * 0.05f});
    float bx0 = sx * 0.02f, bx1 = sx * 0.046f, bx2 = sx * 0.068f;
    float by0 = 0.1855f, by1 = 0.1935f, by2 = 0.188f;
    h.rod(o.hair, {bx0, by0, surf(bx0, by0) + 0.003f}, {bx1, by1, surf(bx1, by1) + 0.003f}, 0.0042f, 0.0038f, 5)
        .rod(o.hair, {bx1, by1, surf(bx1, by1) + 0.003f}, {bx2, by2, surf(bx2, by2) + 0.002f}, 0.0038f, 0.0022f, 5);
    if (female) h.box("#0b0705", {x + sx * 0.024f, 0.1705f, 0.13f}, {0.012f, 0.0022f, 0.003f}, {0, 0, sx * 0.5f});
    float cx = sx * 0.06f, cy = 0.114f;
    if (female) h.add(circleGeo(0.016f, 14), "#d99482", {cx, cy, surf(cx, cy) + 0.0012f}, {0, sx * 0.45f, 0});
  }
  const int N = 8;
  const float W = female ? 0.025f : 0.027f, LIFT = 0.009f;
  auto at = [&](float t, float base, float curve) {
    float x = (t * 2 - 1) * W;
    float y = base + curve * (x / W) * (x / W);
    return V{x, y, surf(x, y) + 0.0015f};
  };
  auto thick = [](float t) { return 0.0028f + 0.0032f * std::sin(t * PI); };
  for (int k = 0; k < N; k++) {
    float t0 = (float)k / N, t1 = (float)(k + 1) / N;
    h.rod(lip, at(t0, 0.0865f, LIFT * 0.85f), at(t1, 0.0865f, LIFT * 0.85f), thick(t0), thick(t1), 5);
    h.rod(lip, at(t0, 0.0775f, LIFT * 1.25f), at(t1, 0.0775f, LIFT * 1.25f), thick(t0) * 1.15f, thick(t1) * 1.15f, 5);
    V a = at(t0, 0.0825f, LIFT), b = at(t1, 0.0825f, LIFT);
    h.rod("#5a2b22", {a.x, a.y, a.z + 0.0022f}, {b.x, b.y, b.z + 0.0022f}, 0.0012f, 0.0012f, 4);
  }
  for (float sx : {-1.0f, 1.0f}) {
    V c = at(sx > 0 ? 1.0f : 0.0f, 0.0825f, LIFT);
    h.ball("#a8705a", {c.x + sx * 0.002f, c.y + 0.001f, c.z - 0.002f}, {0.003f, 0.0035f, 0.002f}, Z0, 1);
  }
}

void topi(Shape& h, const Outfit& o) {
  h.ball(o.hair, {0, 0.165f, -0.035f}, {0.124f, 0.118f, 0.115f}, Z0, 1)
      .box(o.hair, {0.112f, 0.125f, -0.012f}, {0.012f, 0.05f, 0.026f})
      .box(o.hair, {-0.112f, 0.125f, -0.012f}, {0.012f, 0.05f, 0.026f})
      .tube(o.cap, {0, 0.27f, -0.005f}, 0.128f, 0.135f, 0.105f, 14)
      .tube(o.cap, {0, 0.328f, -0.005f}, 0.11f, 0.128f, 0.02f, 14)
      .tube(o.band[0], {0, 0.236f, 0.0f}, 0.14f, 0.14f, 0.046f, 14);
  for (int i = 0; i < 9; i++) {
    float a = -1.0f + i * 0.25f;
    chip(h, o.band[1 + (i % 2)], {std::sin(a) * 0.1415f, 0.237f, std::cos(a) * 0.1415f}, 0.028f, {0, a, PI / 4});
  }
  h.box(o.band[1], {0, 0.236f, 0.142f}, {0.07f, 0.05f, 0.012f});
}

void scarf(Shape& h, const Outfit& o) {
  h.ball(o.cap, {0, 0.185f, -0.02f}, {0.14f, 0.148f, 0.142f}, Z0, 1)
      .tube(o.band[0], {0, 0.248f, 0.012f}, 0.145f, 0.145f, 0.036f, 14, {0.25f, 0, 0})
      .ball(o.cap, {0, 0.21f, -0.152f}, {0.06f, 0.05f, 0.045f}, Z0, 0)
      .box(o.cap, {0.02f, 0.06f, -0.165f}, {0.08f, 0.26f, 0.03f}, {0.15f, 0, 0.1f})
      .box(o.band[1], {0.024f, -0.06f, -0.18f}, {0.082f, 0.03f, 0.032f}, {0.15f, 0, 0.1f})
      .ball(o.hair, {0.112f, 0.17f, -0.03f}, {0.022f, 0.045f, 0.03f}, Z0, 0)
      .ball(o.hair, {-0.112f, 0.17f, -0.03f}, {0.022f, 0.045f, 0.03f}, Z0, 0);
  for (int k = 0; k < 6; k++) h.ball(o.hair, {0, 0.02f - k * 0.055f, -0.13f - k * 0.006f}, {0.032f - k * 0.002f, 0.034f, 0.03f}, Z0, 0);
  h.ball(o.band[2], {0, -0.32f, -0.165f}, {0.025f, 0.03f, 0.025f}, Z0, 0);
  for (int i = 0; i < 9; i++) {
    float a = -1.0f + i * 0.25f;
    chip(h, o.band[1 + (i % 2)], {std::sin(a) * 0.147f, 0.25f + std::cos(a) * 0.036f, std::cos(a) * 0.147f + 0.01f}, 0.024f, {0.25f, a, PI / 4});
  }
  for (float sx : {-1.0f, 1.0f}) {
    h.ball(GOLD, {sx * 0.125f, 0.105f, 0.0f}, {0.016f, 0.024f, 0.016f}, Z0, 0).ball(GOLD, {sx * 0.125f, 0.07f, 0.0f}, {0.012f, 0.012f, 0.012f}, Z0, 0);
  }
  h.add(torusGeo(0.016f, 0.0035f, 4, 10), GOLD, {0.022f, 0.104f, 0.142f}, {0, 0.6f, 0});
  h.ball("#b91c1c", {0, 0.19f, 0.122f}, {0.009f, 0.009f, 0.004f}, Z0, 0);
}

void whiskers(Shape& h, const char* color, bool full) {
  h.box(color, {0.026f, 0.0935f, 0.129f}, {0.048f, 0.013f, 0.016f}, {0, 0, 0.2f}).box(color, {-0.026f, 0.0935f, 0.129f}, {0.048f, 0.013f, 0.016f}, {0, 0, -0.2f});
  if (full) h.ball(color, {0, 0.045f, 0.075f}, {0.1f, 0.09f, 0.09f}, Z0, 1).cone(color, {0, -0.03f, 0.09f}, 0.05f, 0.08f, 6, {PI, 0, 0});
}

void hand(Shape& s, const char* skin, float y, float z) {
  s.ball(skin, {0, y, z}, {0.036f, 0.048f, 0.024f}, Z0, 0);
  for (int f = 0; f < 4; f++) {
    float x = -0.024f + f * 0.016f;
    s.rod(skin, {x, y - 0.035f, z + 0.004f}, {x, y - 0.078f + std::fabs(f - 1.5f) * 0.008f, z + 0.012f}, 0.0075f, 0.0065f, 4);
  }
  s.rod(skin, {0.03f, y - 0.005f, z + 0.01f}, {0.042f, y - 0.045f, z + 0.03f}, 0.009f, 0.007f, 4);
}

Shape sleeve(const char* main, const char* cuff, const char* skin, const Outfit* o = nullptr) {
  Shape s;
  s.ball(main, {0, -0.01f, 0}, {0.078f, 0.078f, 0.078f}, Z0, 0)
      .rod(main, {0, 0.02f, 0}, {0, -0.29f, 0.015f}, 0.068f, 0.058f, 9)
      .ball(main, {0, -0.29f, 0.015f}, {0.058f, 0.058f, 0.058f}, Z0, 0)
      .rod(main, {0, -0.29f, 0.015f}, {0, -0.49f, 0.06f}, 0.057f, 0.05f, 9)
      .tube(cuff, {0, -0.495f, 0.06f}, 0.053f, 0.053f, 0.04f, 9);
  if (o) {
    s.tube(o->stripe, {0, -0.455f, 0.052f}, 0.056f, 0.056f, 0.03f, 12);
    for (int i = 0; i < 12; i++) {
      float a = i / 12.0f * TAU;
      chip(s, o->band[i % 3], {std::sin(a) * 0.058f, -0.455f, 0.052f + std::cos(a) * 0.058f}, 0.018f, {0, a, PI / 4});
    }
  }
  hand(s, skin, -0.555f, 0.065f);
  return s;
}

Shape legGeo(const char* trousers, const char* shoe, const char* wrap, bool tall = false, const Outfit* o = nullptr) {
  Shape s;
  s.rod(trousers, {0, 0.02f, 0}, {0, -0.29f, 0.01f}, 0.085f, 0.064f, 9)
      .ball(trousers, {0, -0.29f, 0.01f}, {0.064f, 0.064f, 0.064f}, Z0, 0)
      .rod(trousers, {0, -0.29f, 0.01f}, {0, -0.48f, 0}, 0.062f, 0.047f, 9);
  for (int k = 0; k < 3; k++) s.tube(trousers, {0, -0.36f - k * 0.04f, 0.004f}, 0.06f - k * 0.004f, 0.062f - k * 0.004f, 0.012f, 9);
  s.tube(wrap, {0, -0.47f, 0}, 0.052f, 0.052f, 0.035f, 9)
      .rod(shoe, {0, -0.475f, 0}, {0, -0.575f, 0.012f}, 0.054f, 0.06f, 9)
      .ball(shoe, {0, -0.588f, 0.05f}, {0.062f, 0.034f, 0.112f}, Z0, 1)
      .cone(shoe, {0, -0.572f, 0.168f}, 0.022f, 0.07f, 6, {PI / 2 - 0.75f, 0, 0})
      .tube("#c9a96e", {0, -0.6f, 0.05f}, 0.06f, 0.064f, 0.012f, 9);
  if (tall) {
    s.rod(shoe, {0, -0.27f, 0.006f}, {0, -0.5f, 0.004f}, 0.066f, 0.058f, 12)
        .tube(shoe, {0, -0.27f, 0.006f}, 0.068f, 0.068f, 0.03f, 12);
    if (o) {
      s.tube(o->stripe, {0, -0.255f, 0.006f}, 0.069f, 0.069f, 0.022f, 12);
      for (int i = 0; i < 12; i++) {
        float a = i / 12.0f * TAU;
        chip(s, o->band[i % 3], {std::sin(a) * 0.071f, -0.255f, 0.006f + std::cos(a) * 0.071f}, 0.016f, {0, a, PI / 4});
      }
    }
    for (int k = 0; k < 3; k++) s.tube("#2a1a10", {0, -0.33f - k * 0.065f, 0.006f}, 0.067f, 0.067f, 0.012f, 12, {0.0f, 0, (k % 2 ? 0.18f : -0.18f)});
  }
  return s;
}

void roleProps(Shape& t, const std::string& role) {
  if (role == "hunter") {
    t.tube("#6b4423", {0.12f, 1.08f, -0.24f}, 0.065f, 0.055f, 0.55f, 9, {0.25f, 0, -0.35f})
        .tube("#4a2f1a", {0.06f, 1.33f, -0.21f}, 0.068f, 0.068f, 0.03f, 9, {0.25f, 0, -0.35f})
        .cone("#f3f0e6", {0.21f, 1.42f, -0.29f}, 0.04f, 0.12f, 4, {0.25f, 0, -0.35f})
        .cone("#b91c1c", {0.17f, 1.4f, -0.26f}, 0.03f, 0.1f, 4, {0.25f, 0, -0.35f})
        .cone("#f3f0e6", {0.25f, 1.38f, -0.3f}, 0.035f, 0.11f, 4, {0.25f, 0, -0.35f})
        .add(torusGeo(0.48f, 0.016f, 5, 18, PI * 0.85f), "#7a4b2a", {-0.02f, 1.0f, -0.26f}, {0, 0, 1.85f})
        .box("#6b4423", {-0.22f, 0.76f, 0.12f}, {0.1f, 0.13f, 0.06f})
        .box("#4a2f1a", {-0.22f, 0.81f, 0.15f}, {0.1f, 0.04f, 0.012f})
        .rod("#d6d3d1", {0.25f, 0.8f, 0.13f}, {0.26f, 0.62f, 0.15f}, 0.012f, 0.006f, 4)
        .box("#4a2f1a", {0.25f, 0.8f, 0.13f}, {0.035f, 0.07f, 0.03f})
        .rod("#4a2f1a", {0.17f, 1.3f, 0.2f}, {-0.2f, 0.82f, 0.24f}, 0.016f, 0.016f, 6)
        .rod("#4a2f1a", {0.17f, 1.3f, -0.2f}, {-0.2f, 0.82f, -0.24f}, 0.016f, 0.016f, 6)
        .ball(GOLD, {0.02f, 1.07f, 0.245f}, {0.018f, 0.018f, 0.01f}, Z0, 0)
        .box("#b91c1c", {0.085f, 1.05f, 0.25f}, {0.008f, 0.4f, 0.008f})
        .box("#b91c1c", {-0.085f, 1.05f, 0.25f}, {0.008f, 0.4f, 0.008f})
        .box("#e9a23b", {-0.18f, 0.66f, 0.2f}, {0.05f, 0.2f, 0.015f}, {0.1f, 0, 0.08f});
  } else if (role == "carpenter") {
    t.rod("#8b6a45", {0.28f, 0.94f, 0.08f}, {0.32f, 0.55f, 0.12f}, 0.018f, 0.018f, 6)
        .box("#9ca3af", {0.32f, 0.6f, 0.17f}, {0.03f, 0.11f, 0.13f})
        .box("#c2a27a", {-0.24f, 0.8f, 0.0f}, {0.04f, 0.16f, 0.3f})
        .box("#5b3a1e", {-0.22f, 0.86f, 0.14f}, {0.1f, 0.14f, 0.06f})
        .box("#a8763e", {0, 0.72f, 0.31f}, {0.34f, 0.4f, 0.02f})
        .box("#8a5a2b", {0, 0.92f, 0.315f}, {0.34f, 0.03f, 0.02f})
        .rod("#d97706", {0.08f, 0.78f, 0.33f}, {0.08f, 0.66f, 0.33f}, 0.007f, 0.007f, 4);
  } else if (role == "herbalist") {
    t.tube("#b08850", {0, 1.06f, -0.31f}, 0.22f, 0.12f, 0.56f, 12)
        .tube("#8a6a3d", {0, 1.3f, -0.31f}, 0.225f, 0.225f, 0.04f, 12)
        .tube("#8a6a3d", {0, 1.1f, -0.31f}, 0.2f, 0.2f, 0.02f, 12)
        .tube("#8a6a3d", {0, 0.95f, -0.31f}, 0.17f, 0.17f, 0.03f, 12)
        .ball("#4d7c2f", {0, 1.36f, -0.31f}, {0.17f, 0.08f, 0.17f}, Z0, 1)
        .ball("#7c3aed", {0.07f, 1.42f, -0.27f}, {0.035f, 0.035f, 0.035f}, Z0, 0)
        .ball("#e11d48", {-0.05f, 1.43f, -0.3f}, {0.035f, 0.035f, 0.035f}, Z0, 0)
        .ball("#facc15", {-0.06f, 1.41f, -0.35f}, {0.035f, 0.035f, 0.035f}, Z0, 0)
        .rod("#3f6212", {0.05f, 1.38f, -0.33f}, {0.09f, 1.52f, -0.35f}, 0.006f, 0.004f, 3)
        .rod("#3f6212", {-0.04f, 1.38f, -0.28f}, {-0.08f, 1.5f, -0.26f}, 0.006f, 0.004f, 3)
        .box("#8a6a3d", {0.12f, 1.12f, 0.08f}, {0.03f, 0.5f, 0.02f}, {0, 0, 0.5f});
    necklace(t);
  } else {
    t.rod("#8b6a45", {-0.28f, 0.94f, 0.08f}, {-0.32f, 0.6f, 0.1f}, 0.02f, 0.02f, 6)
        .box("#6b7280", {-0.32f, 0.62f, 0.1f}, {0.16f, 0.08f, 0.08f})
        .box("#6b4423", {0.23f, 0.84f, -0.14f}, {0.12f, 0.16f, 0.1f})
        .box("#e7e0d0", {0, 0.7f, 0.3f}, {0.32f, 0.36f, 0.02f})
        .box("#78716c", {0, 0.6f, 0.31f}, {0.28f, 0.012f, 0.01f})
        .box("#78716c", {0, 0.72f, 0.31f}, {0.28f, 0.012f, 0.01f});
  }
}

float sm(float a, float b, float x) { return smf(a, b, x); }

void shapeTorso(Shape& s) {
  s.reshapeF([](Vec3& p) {
    float x = p.x, y = p.y, z = p.z;
    float chest = sm(0.82f, 1.28f, y);
    float sx = y > 0.82f ? 0.9f + 0.17f * chest : 0.86f + 0.06f * sm(0.3f, 0.82f, y);
    float sz = (y > 0.82f ? 0.84f - 0.04f * chest : 0.82f) * (z > 0 && y > 0.95f && y < 1.3f ? 1.06f : 1);
    p = {x * sx, y + PEOPLE_LIFT, z * sz};
  });
}

void shapeHead(Shape& s) {
  s.reshapeF([](Vec3& p) { p = {p.x * 1.12f, p.y * 1.14f, p.z * 1.1f}; });
}

void shapeArm(Shape& s) {
  s.reshapeF([](Vec3& p) {
    bool hnd = p.y < -0.52f;
    float k = hnd ? 1.22f * -0.52f + (p.y + 0.52f) : p.y * 1.22f;
    p = {p.x * 0.94f, k, p.z * 0.94f + (hnd ? 0.01f : 0)};
  });
}

void shapeLeg(Shape& s) {
  s.reshapeF([](Vec3& p) {
    bool foot = p.y < -0.46f;
    p = {p.x * (foot ? 1 : 0.92f), foot ? -0.46f * 1.48f + (p.y + 0.46f) : p.y * 1.48f, p.z * (foot ? 1 : 0.92f)};
  });
}

Mesh finish(Shape& s, void (*fn)(Shape&)) {
  fn(s);
  return uploadMesh(s.build(true));
}

std::map<std::string, std::unique_ptr<PersonModel>> g_people;
std::unique_ptr<ResidentKit> g_kit;

}  // namespace

namespace people {

const PersonModel& person(const std::string& role) {
  auto it = g_people.find(role);
  if (it != g_people.end()) return *it->second;
  const Outfit& o = outfitFor(role);
  Shape t;
  if (o.female) torsoFemale(t, o, o.stripe);
  else torsoMale(t, o);
  if (role != "hunter") shawl(t, o);
  neck(t, o);
  roleProps(t, role);
  Shape h;
  face(h, o, o.female);
  if (o.female) scarf(h, o);
  else topi(h, o);
  if (o.beard) whiskers(h, o.beard, role == "builder");
  if (role == "hunter") {
    h.ball("#b91c1c", {0.13f, 0.27f, 0.05f}, {0.028f, 0.035f, 0.02f}, Z0, 1)
        .ball("#7f1d1d", {0.135f, 0.29f, 0.045f}, {0.018f, 0.02f, 0.014f}, Z0, 0)
        .rod("#14532d", {0.13f, 0.25f, 0.05f}, {0.15f, 0.33f, 0.02f}, 0.006f, 0.003f, 4);
  }
  auto p = std::make_unique<PersonModel>();
  p->torso = finish(t, shapeTorso);
  p->head = finish(h, shapeHead);
  bool hunter = role == "hunter";
  Shape arm = sleeve(o.coat, o.trim, o.skin, hunter ? &o : nullptr);
  p->arm = finish(arm, shapeArm);
  Shape leg = legGeo(o.trousers, o.shoe, o.sash, hunter, hunter ? &o : nullptr);
  p->leg = finish(leg, shapeLeg);
  PersonModel& r = *p;
  g_people[role] = std::move(p);
  return r;
}

const ResidentKit& residentKit() {
  if (g_kit) return *g_kit;
  const char* W = "#ffffff";
  Outfit man{W, "#3f2a1c", "#efe6d4", "#7f1d1d", "#d8ccb4", "#9a3412", "#5b5f66", {"#15803d", "#facc15", "#b91c1c"}, "#c98f68", "#1c120c", "#e3d8c2", "#3b2a20", false, nullptr};
  Outfit woman{W, "#1e3a5f", "#f5e9d3", "#a16207", "#efe3cb", "#be123c", "#b91c1c", {"#facc15", "#1d4ed8", "#15803d"}, "#d6a585", "#120c09", "#7c2d12", "#5a3d2b", true, nullptr};
  Outfit elder{W, "#44403c", "#e7e0d0", "#57534e", "#8a7f73", "#1e3a8a", "#3f3f46", {"#7f1d1d", "#f59e0b", "#1e40af"}, "#b98262", "#d6d3cd", "#d8cfbf", "#2f241c", false, "#e7e5e0"};
  Shape t0; torsoMale(t0, man); shawl(t0, man); neck(t0, man);
  Shape t1; torsoFemale(t1, woman, "#be123c"); shawl(t1, woman); neck(t1, woman); necklace(t1);
  t1.box("#b08850", {0.27f, 0.72f, 0.1f}, {0.12f, 0.14f, 0.16f}).ball("#e11d48", {0.27f, 0.8f, 0.1f}, {0.05f, 0.03f, 0.06f}, Z0, 0);
  Shape t2; torsoMale(t2, elder); shawl(t2, elder); neck(t2, elder);
  t2.rod("#6b4423", {0.36f, 1.05f, 0.18f}, {0.4f, 0.02f, 0.24f}, 0.022f, 0.02f, 6).ball("#5a3a20", {0.36f, 1.07f, 0.18f}, {0.03f, 0.03f, 0.03f}, Z0, 0);
  Shape h0; face(h0, man, false); topi(h0, man); whiskers(h0, man.hair, false);
  Shape h1; face(h1, woman, true); scarf(h1, woman);
  Shape h2; face(h2, elder, false); topi(h2, elder); whiskers(h2, "#e7e5e0", true);
  g_kit = std::make_unique<ResidentKit>();
  g_kit->torsos[0] = finish(t0, shapeTorso);
  g_kit->torsos[1] = finish(t1, shapeTorso);
  g_kit->torsos[2] = finish(t2, shapeTorso);
  g_kit->heads[0] = finish(h0, shapeHead);
  g_kit->heads[1] = finish(h1, shapeHead);
  g_kit->heads[2] = finish(h2, shapeHead);
  Shape arm = sleeve(W, "#efe6d4", "#c99a74");
  g_kit->arm = finish(arm, shapeArm);
  Shape leg = legGeo("#e3d8c2", "#3b2a20", "#7f1d1d");
  g_kit->leg = finish(leg, shapeLeg);
  return *g_kit;
}

void shutdown() {
  for (auto& p : g_people) { p.second->torso.destroy(); p.second->head.destroy(); p.second->arm.destroy(); p.second->leg.destroy(); }
  g_people.clear();
  if (g_kit) {
    for (int i = 0; i < 3; i++) { g_kit->torsos[i].destroy(); g_kit->heads[i].destroy(); }
    g_kit->arm.destroy();
    g_kit->leg.destroy();
    g_kit.reset();
  }
}

}  // namespace people
