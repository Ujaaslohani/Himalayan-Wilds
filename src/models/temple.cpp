#include "temple.h"

namespace {

using V = Vec3;
const V Z0{0, 0, 0};
const char *STONE = "#8f8a80", *STONE2 = "#77726a", *STONE3 = "#a8a296", *PAVE = "#9b958a", *PAVE2 = "#8a847a";
const char *WOOD = "#5a3820", *WOOD2 = "#3f2716", *CARVE = "#7a4b2a";
const char *SLATE = "#4b5563", *SLATE2 = "#374151", *SLATE3 = "#5b6573";
const char *GOLD = "#d4a72c", *BRASS = "#b8892a", *RED = "#9f1d1d", *SAFFRON = "#e07b1a", *LAMP = "#ffd27a", *WHITE = "#e9e4d8";
const char* WIN = "#ffb04a";
const float SANCTUM_HALF = 4.2f, WALL_H = 0.9f, GATE = 2.6f, PAVE_TOP = 0.15f;

Shape* glowShape = nullptr;

void frustum(Shape& b, const char* color, float y, float bottomHalf, float topHalf, float h) {
  b.add(cylinderGeo(topHalf * std::sqrt(2.0f), bottomHalf * std::sqrt(2.0f), h, 4, 1), color, {0, y + h / 2, 0}, {0, PI / 4, 0});
}

void eaveFringe(Shape& b, float half, float y, int n) {
  for (int side = 0; side < 4; side++) {
    float a = (side * PI) / 2;
    float c = std::cos(a), s = std::sin(a);
    for (int i = 0; i < n; i++) {
      float t = -half + ((i + 0.5f) / n) * half * 2;
      float x = c * t + s * half, z = -s * t + c * half;
      b.box(i % 2 ? CARVE : WOOD2, {x, y - 0.18f, z}, {0.09f, 0.36f, 0.05f}, {0, a, 0});
    }
    for (int k = -1; k <= 1; k += 2) {
      float x = c * k * half + s * half, z = -s * k * half + c * half;
      b.box(WOOD2, {x * 1.04f, y + 0.05f, z * 1.04f}, {0.2f, 0.2f, 0.9f}, {0.5f, a + k * PI / 4, 0});
      b.cone(BRASS, {x, y - 0.48f, z}, 0.11f, 0.24f, 6).ball(GOLD, {x, y - 0.64f, z}, {0.035f, 0.035f, 0.035f}, Z0, 0);
    }
  }
}

float kathKuniWalls(Shape& b, float half, float y0, int courses) {
  float y = y0;
  for (int i = 0; i < courses; i++) {
    b.box(WOOD, {0, y + 0.07f, 0}, {half * 2 + 0.14f, 0.14f, half * 2 + 0.14f});
    b.box(i % 2 ? STONE : STONE2, {0, y + 0.29f, 0}, {half * 2 - 0.04f, 0.3f, half * 2 - 0.04f});
    y += 0.44f;
  }
  for (int sx = -1; sx <= 1; sx += 2)
    for (int sz = -1; sz <= 1; sz += 2) b.box(WOOD2, {sx * half, y0 + (y - y0) / 2, sz * half}, {0.26f, y - y0, 0.26f});
  return y;
}

void carvedWindow(Shape& b, float x, float y, float z, float rot, float w, float h) {
  float c = std::cos(rot), s = std::sin(rot);
  b.box(WOOD2, {x, y, z}, {w + 0.24f, h + 0.24f, 0.1f}, {0, rot, 0});
  glowShape->box(WIN, {x + s * 0.07f, y, z + c * 0.07f}, {w, h, 0.02f}, {0, rot, 0});
  for (int k = -1; k <= 1; k++) b.box(CARVE, {x + c * k * w * 0.3f + s * 0.11f, y, z - s * k * w * 0.3f + c * 0.11f}, {0.05f, h, 0.04f}, {0, rot, 0});
  b.box(CARVE, {x + s * 0.11f, y, z + c * 0.11f}, {w, 0.05f, 0.04f}, {0, rot, 0});
  b.box(CARVE, {x + s * 0.06f, y + h / 2 + 0.22f, z + c * 0.06f}, {w + 0.5f, 0.16f, 0.12f}, {0, rot, 0});
}

void chorten(Shape& b, float x, float z, float s) {
  b.box(STONE2, {x, 0.35f * s, z}, {2.2f * s, 0.7f * s, 2.2f * s});
  b.box(WHITE, {x, 0.95f * s, z}, {1.8f * s, 0.5f * s, 1.8f * s});
  b.box(WHITE, {x, 1.35f * s, z}, {1.45f * s, 0.3f * s, 1.45f * s});
  b.ball(WHITE, {x, 2.05f * s, z}, {0.85f * s, 0.75f * s, 0.85f * s});
  b.box(RED, {x, 2.8f * s, z}, {0.7f * s, 0.25f * s, 0.7f * s});
  b.cone(GOLD, {x, 3.6f * s, z}, 0.32f * s, 1.4f * s, 8);
  b.ball(GOLD, {x, 4.4f * s, z}, {0.14f * s, 0.14f * s, 0.14f * s}, Z0, 0);
}

void lion(Shape& b, float x, float z, float ry) {
  b.box(STONE2, {x, PAVE_TOP + 0.3f, z}, {0.9f, 0.6f, 1.3f});
  b.ball(STONE3, {x, PAVE_TOP + 0.95f, z - 0.15f}, {0.38f, 0.45f, 0.55f}, {0, ry, 0});
  b.ball(STONE3, {x, PAVE_TOP + 1.45f, z + 0.25f}, {0.32f, 0.32f, 0.3f}, {0, ry, 0});
  b.ball(STONE, {x, PAVE_TOP + 1.42f, z + 0.42f}, {0.42f, 0.38f, 0.18f}, {0, ry, 0});
  b.ball(STONE2, {x, PAVE_TOP + 1.36f, z + 0.55f}, {0.13f, 0.1f, 0.12f}, {0, ry, 0}, 0);
  for (int sx = -1; sx <= 1; sx += 2) b.tube(STONE3, {x + sx * 0.22f, PAVE_TOP + 0.75f, z + 0.32f}, 0.09f, 0.1f, 0.5f, 6);
}

bool g_lodPass = false;
void flagString(Shape& b, V a, V c, int n, float sag, int seed) {
  if (g_lodPass) n = std::max(2, n / 3);
  const char* cols[5] = {RED, SAFFRON, WHITE, "#1d4ed8", "#15803d"};
  float ry = std::atan2(c.x - a.x, c.z - a.z);
  for (int k = 1; k < n; k++) {
    float t = (float)k / n;
    float x = a.x + (c.x - a.x) * t, z = a.z + (c.z - a.z) * t;
    float y = a.y + (c.y - a.y) * t - std::sin(t * PI) * sag;
    b.cloth(planeGeo(0.34f, 0.34f), cols[(k + seed) % 5], Pivot::Top, {x, y - 0.2f, z}, {0, ry + PI / 2, 0});
    b.cloth(planeGeo(0.34f, 0.34f), cols[(k + seed) % 5], Pivot::Top, {x, y - 0.2f, z}, {0, ry - PI / 2, 0});
  }
}

bool g_built = false;
Mesh g_mesh, g_glow, g_lodMesh;
std::vector<V> g_lights;

void buildPass();
void build() {
  if (g_built) return;
  g_built = true;
  buildPass();
  std::vector<V> keep = g_lights;
  g_lodPass = true;
  g_geoLod = 1;
  buildPass();
  g_geoLod = 0;
  g_lodPass = false;
  g_lights = keep;
}
void buildPass() {
  Shape b, glow;
  glowShape = &glow;
  std::vector<V>& lights = g_lights;
  const int tiles = 6;
  float tw = (COURT * 2) / tiles;
  for (int i = 0; i < tiles; i++)
    for (int j = 0; j < tiles; j++) b.box((i + j) % 2 ? PAVE : PAVE2, {-COURT + tw * (i + 0.5f), PAVE_TOP - 0.4f, -COURT + tw * (j + 0.5f)}, {tw - 0.06f, 0.8f, tw - 0.06f});
  b.box(STONE2, {0, PAVE_TOP - 0.95f, 0}, {COURT * 2 + 0.4f, 0.5f, COURT * 2 + 0.4f});
  for (int k = 0; k < 7; k++) b.box(STONE3, {0, PAVE_TOP + 0.005f, COURT - 1 - k * 1.25f}, {3.2f, 0.02f, 0.9f});

  for (int side = 0; side < 4; side++) {
    float a = (side * PI) / 2;
    float c = std::cos(a), s = std::sin(a);
    std::vector<std::pair<float, float>> segs;
    if (side == 0) segs = {{-COURT, -GATE}, {GATE, COURT}};
    else segs = {{-COURT, COURT}};
    for (auto& sg : segs) {
      float t0 = sg.first, t1 = sg.second;
      float mid = (t0 + t1) / 2, len = t1 - t0;
      float x = c * mid + s * COURT, z = -s * mid + c * COURT;
      b.box(STONE2, {x, WALL_H / 2, z}, {len + 0.6f, WALL_H, 0.6f}, {0, a, 0});
      b.box(STONE3, {x, WALL_H + 0.06f, z}, {len + 0.7f, 0.12f, 0.75f}, {0, a, 0});
      int posts = std::max(1, (int)std::floor(len / 4));
      for (int k = 0; k <= posts; k++) {
        float t = t0 + (len * k) / posts;
        float px = c * t + s * COURT, pz = -s * t + c * COURT;
        b.tube(STONE, {px, WALL_H + 0.3f, pz}, 0.16f, 0.2f, 0.5f, 6).cone(STONE3, {px, WALL_H + 0.7f, pz}, 0.2f, 0.3f, 6);
      }
    }
  }

  for (int sx = -1; sx <= 1; sx += 2) {
    float x = sx * (GATE + 0.45f);
    b.box(STONE2, {x, 0.4f, COURT}, {1.0f, 0.8f, 1.0f});
    b.box(STONE, {x, 2.8f, COURT}, {0.7f, 4.2f, 0.7f});
    b.box(STONE3, {x, 4.95f, COURT}, {0.95f, 0.2f, 0.95f});
    for (int k = 0; k < 3; k++) b.box(CARVE, {x, 1.4f + k * 1.2f, COURT + 0.36f}, {0.5f, 0.08f, 0.04f});
  }
  b.box(WOOD, {0, 5.25f, COURT}, {GATE * 2 + 2.4f, 0.45f, 0.8f});
  b.box(CARVE, {0, 4.85f, COURT + 0.05f}, {GATE * 2 + 0.6f, 0.3f, 0.6f});
  b.box(RED, {0, 5.25f, COURT + 0.42f}, {2.2f, 0.32f, 0.04f});
  for (int sz = -1; sz <= 1; sz += 2) b.box(SLATE, {0, 5.78f, COURT + sz * 0.42f}, {GATE * 2 + 2.8f, 0.1f, 1.0f}, {sz * 0.5f, 0, 0});
  b.box(WOOD2, {0, 6.0f, COURT}, {GATE * 2 + 2.9f, 0.12f, 0.14f});
  b.cone(GOLD, {0, 6.35f, COURT}, 0.12f, 0.5f, 8);
  for (int k = -1; k <= 1; k++) {
    b.rod(BRASS, {k * 1.4f, 4.7f, COURT}, {k * 1.4f, 4.25f, COURT}, 0.015f, 0.015f, 4);
    b.cone(BRASS, {k * 1.4f, 4.05f, COURT}, 0.22f, 0.45f, 10).ball(GOLD, {k * 1.4f, 3.8f, COURT}, {0.06f, 0.06f, 0.06f}, Z0, 0);
  }

  for (int sx = -1; sx <= 1; sx += 2)
    for (int sz = -1; sz <= 1; sz += 2) chorten(b, sx * (COURT - 2.2f), sz * (COURT - 2.2f), 1);

  b.tube(STONE2, {6.5f, PAVE_TOP + 0.4f, 9.6f}, 0.7f, 0.8f, 0.8f, 8);
  b.tube(WOOD2, {6.5f, PAVE_TOP + 4.8f, 9.6f}, 0.09f, 0.14f, 8.4f, 6);
  b.box(GOLD, {6.5f, PAVE_TOP + 9.1f, 9.6f}, {0.9f, 0.08f, 0.08f});
  for (int k = -1; k <= 1; k++) b.cone(GOLD, {6.5f + k * 0.42f, PAVE_TOP + 9.45f + (k == 0 ? 0.15f : 0), 9.6f}, 0.07f, 0.6f, 5);
  b.cloth(boxGeo(0.04f, 1.3f, 1.4f, 1, 3, 6), SAFFRON, Pivot::ZNeg, {6.5f, PAVE_TOP + 7.9f, 10.35f});
  b.cloth(boxGeo(0.05f, 0.12f, 1.4f, 1, 1, 6), RED, Pivot::ZNeg, {6.5f, PAVE_TOP + 7.2f, 10.35f});

  for (int sx = -1; sx <= 1; sx += 2)
    for (float zz : {5.2f, 9.2f}) {
      float x = sx * 3.3f;
      b.tube(STONE3, {x, PAVE_TOP + 0.7f, zz + 2}, 0.13f, 0.2f, 1.4f, 6).box(STONE, {x, PAVE_TOP + 1.5f, zz + 2}, {0.5f, 0.25f, 0.5f});
      glow.ball(LAMP, {x, PAVE_TOP + 1.78f, zz + 2}, {0.12f, 0.17f, 0.12f}, Z0, 0);
      b.cone(STONE2, {x, PAVE_TOP + 2.08f, zz + 2}, 0.34f, 0.32f, 4);
      lights.push_back({x, PAVE_TOP + 1.8f, zz + 2});
    }

  b.box(STONE2, {0, PAVE_TOP + 0.33f, 0}, {PLINTH_HALF * 2 + 1.0f, 0.66f, PLINTH_HALF * 2 + 1.0f});
  b.box(STONE, {0, PAVE_TOP + 0.9f, 0}, {PLINTH_HALF * 2, 0.6f, PLINTH_HALF * 2});
  b.box(STONE3, {0, PAVE_TOP + PLINTH_H - 0.03f, 0}, {PLINTH_HALF * 2 + 0.14f, 0.08f, PLINTH_HALF * 2 + 0.14f});
  for (int side = 0; side < 4; side++) {
    float a = (side * PI) / 2;
    float c = std::cos(a), s = std::sin(a);
    for (int k = -3; k <= 3; k++) {
      float x = c * k * 2 + s * (PLINTH_HALF + 0.52f), z = -s * k * 2 + c * (PLINTH_HALF + 0.52f);
      if (side == 0 && std::abs(k) <= 1) continue;
      b.box(CARVE, {x, PAVE_TOP + 0.33f, z}, {0.9f, 0.36f, 0.06f}, {0, a, 0});
    }
  }
  float PT = PAVE_TOP + PLINTH_H;
  for (int i = 0; i < 3; i++) b.box(i % 2 ? STONE : STONE3, {0, PAVE_TOP + (i + 1) * (PLINTH_H / 3) / 2, PLINTH_HALF + 0.45f + (2 - i) * 0.5f}, {4.4f, (i + 1) * (PLINTH_H / 3), 0.5f});
  for (int sx = -1; sx <= 1; sx += 2) {
    b.box(STONE2, {sx * 2.5f, PAVE_TOP + 0.75f, PLINTH_HALF + 1.0f}, {0.5f, 1.5f, 1.6f});
    lion(b, sx * 3.7f, PLINTH_HALF + 1.6f, 0);
  }

  float top = kathKuniWalls(b, SANCTUM_HALF, PT, 9);
  float vh = top - PT + 0.6f;
  for (int i = 0; i < 4; i++) {
    float a = (i * PI) / 2;
    float c = std::cos(a), s = std::sin(a);
    const float R = 5.9f;
    for (float t : {-5.4f, -2.7f, 0.0f, 2.7f, 5.4f}) {
      if (i == 0 && t == 0) continue;
      float x = c * t + s * R, z = -s * t + c * R;
      b.tube(WOOD, {x, PT + vh / 2, z}, 0.14f, 0.17f, vh, 8);
      b.box(CARVE, {x, PT + vh - 0.3f, z}, {0.5f, 0.22f, 0.5f});
      b.box(STONE3, {x, PT + 0.12f, z}, {0.44f, 0.24f, 0.44f});
      b.rod(CARVE, {x, PT + vh - 0.5f, z}, {x * 1.22f, PT + vh + 0.55f, z * 1.22f}, 0.06f, 0.05f, 4);
    }
    b.box(CARVE, {s * R, PT + vh, c * R}, {c * 12.2f + s * 0.36f, 0.26f, s * 12.2f + c * 0.36f});
    b.box(WOOD2, {s * R, PT + 0.8f, c * R}, {c * 11.6f + s * 0.08f, 0.08f, s * 11.6f + c * 0.08f});
    if (i != 0)
      for (int k = 0; k < 13; k++) {
        float t = -5.4f + k * 0.9f;
        b.box(WOOD, {c * t + s * R, PT + 0.42f, -s * t + c * R}, {0.07f, 0.78f, 0.07f});
      }
    if (i != 0) carvedWindow(b, s * (SANCTUM_HALF + 0.06f), PT + 2.3f, c * (SANCTUM_HALF + 0.06f), a, 1.3f, 1.2f);
    for (float t : {-2.7f, 2.7f}) {
      float x = c * t + s * R, z = -s * t + c * R;
      b.rod(BRASS, {x, PT + vh - 0.15f, z}, {x, PT + vh - 0.7f, z}, 0.015f, 0.015f, 4);
      glow.box(LAMP, {x, PT + vh - 0.95f, z}, {0.26f, 0.38f, 0.26f});
      b.box(WOOD2, {x, PT + vh - 0.72f, z}, {0.34f, 0.06f, 0.34f});
      lights.push_back({x, PT + vh - 0.95f, z});
    }
  }
  b.box(WOOD2, {0, PT + 1.4f, SANCTUM_HALF + 0.03f}, {2.0f, 2.8f, 0.12f});
  b.box(CARVE, {0, PT + 1.35f, SANCTUM_HALF + 0.08f}, {1.6f, 2.6f, 0.06f});
  for (int k = 0; k < 5; k++) b.box(GOLD, {0, PT + 0.45f + k * 0.5f, SANCTUM_HALF + 0.12f}, {1.35f, 0.05f, 0.03f});
  b.box(WOOD, {0, PT + 3.0f, SANCTUM_HALF + 0.12f}, {3.0f, 0.4f, 0.22f});
  b.box(CARVE, {0, PT + 3.45f, SANCTUM_HALF + 0.12f}, {2.2f, 0.5f, 0.18f});
  for (int k = -4; k <= 4; k++) b.ball(k % 2 ? SAFFRON : RED, {k * 0.32f, PT + 2.72f, SANCTUM_HALF + 0.3f}, {0.08f, 0.12f, 0.05f}, Z0, 0);
  b.rod(BRASS, {0, PT + vh - 0.1f, 5.2f}, {0, PT + 3.5f, 5.2f}, 0.02f, 0.02f, 4);
  b.cone(BRASS, {0, PT + 3.25f, 5.2f}, 0.28f, 0.55f, 10).ball(GOLD, {0, PT + 2.95f, 5.2f}, {0.07f, 0.07f, 0.07f}, Z0, 0);

  float e1 = PT + vh + 0.12f;
  b.box(WOOD2, {0, e1 - 0.02f, 0}, {13.2f, 0.22f, 13.2f});
  frustum(b, SLATE, e1, 7.7f, 4.6f, 1.9f);
  frustum(b, SLATE2, e1 - 0.06f, 7.75f, 7.45f, 0.14f);
  for (int k = 1; k < 4; k++) frustum(b, k % 2 ? SLATE3 : SLATE2, e1 + k * 0.47f - 0.02f, 7.7f - k * 0.775f + 0.04f, 7.7f - k * 0.775f - 0.02f, 0.05f);
  eaveFringe(b, 7.6f, e1 + 0.06f, 20);

  float d2y = e1 + 1.9f;
  b.box(WOOD, {0, d2y + 0.9f, 0}, {6.4f, 1.8f, 6.4f});
  for (int k = 0; k < 3; k++) b.box(k % 2 ? CARVE : WOOD2, {0, d2y + 0.25f + k * 0.65f, 0}, {6.55f, 0.1f, 6.55f});
  b.box(WOOD2, {0, d2y + 0.05f, 0}, {8.0f, 0.12f, 8.0f});
  for (int i = 0; i < 4; i++) {
    float a = (i * PI) / 2;
    float c = std::cos(a), s = std::sin(a);
    for (int k = 0; k < 15; k++) {
      float t = -3.85f + k * 0.55f;
      b.box(CARVE, {c * t + s * 3.95f, d2y + 0.4f, -s * t + c * 3.95f}, {0.06f, 0.65f, 0.06f});
    }
    b.box(WOOD, {s * 3.95f, d2y + 0.75f, c * 3.95f}, {c * 8.0f + s * 0.1f, 0.1f, s * 8.0f + c * 0.1f});
    for (float t : {-1.8f, 0.0f, 1.8f}) carvedWindow(b, c * t + s * 3.22f, d2y + 1.0f, -s * t + c * 3.22f, a, 0.8f, 0.9f);
    lights.push_back({s * 3.3f, d2y + 1.0f, c * 3.3f});
  }

  float e2 = d2y + 1.8f;
  b.box(WOOD2, {0, e2, 0}, {8.4f, 0.16f, 8.4f});
  frustum(b, SLATE, e2, 5.1f, 3.0f, 1.4f);
  frustum(b, SLATE2, e2 - 0.05f, 5.15f, 4.9f, 0.12f);
  eaveFringe(b, 5.0f, e2 + 0.05f, 14);

  float d3y = e2 + 1.4f;
  b.box(WOOD, {0, d3y + 0.65f, 0}, {4.0f, 1.3f, 4.0f});
  b.box(CARVE, {0, d3y + 1.1f, 0}, {4.1f, 0.1f, 4.1f});
  for (int i = 0; i < 4; i++) {
    float a = (i * PI) / 2;
    carvedWindow(b, std::sin(a) * 2.02f, d3y + 0.6f, std::cos(a) * 2.02f, a, 0.7f, 0.6f);
  }
  float e3 = d3y + 1.3f;
  b.box(WOOD2, {0, e3, 0}, {5.6f, 0.14f, 5.6f});
  frustum(b, SLATE, e3, 3.4f, 2.0f, 1.05f);
  eaveFringe(b, 3.35f, e3 + 0.05f, 10);

  float d4y = e3 + 1.05f;
  b.box(WOOD, {0, d4y + 0.45f, 0}, {2.4f, 0.9f, 2.4f});
  b.box(CARVE, {0, d4y + 0.8f, 0}, {2.5f, 0.08f, 2.5f});
  float e4 = d4y + 0.9f;
  b.box(WOOD2, {0, e4, 0}, {3.6f, 0.12f, 3.6f});
  b.add(coneGeo(2.6f, 3.0f, 4), SLATE, {0, e4 + 1.5f, 0}, {0, PI / 4, 0});
  eaveFringe(b, 1.85f, e4 + 0.05f, 6);

  float f = e4 + 2.9f;
  b.tube(GOLD, {0, f + 0.1f, 0}, 0.22f, 0.34f, 0.2f, 10)
      .ball(GOLD, {0, f + 0.45f, 0}, {0.38f, 0.32f, 0.38f})
      .tube(GOLD, {0, f + 0.8f, 0}, 0.1f, 0.2f, 0.24f, 10)
      .ball(GOLD, {0, f + 1.06f, 0}, {0.24f, 0.26f, 0.24f})
      .tube(GOLD, {0, f + 1.36f, 0}, 0.08f, 0.14f, 0.2f, 8)
      .ball(GOLD, {0, f + 1.58f, 0}, {0.16f, 0.18f, 0.16f})
      .cone(GOLD, {0, f + 2.0f, 0}, 0.09f, 0.7f, 8)
      .ball(GOLD, {0, f + 2.4f, 0}, {0.07f, 0.07f, 0.07f}, Z0, 0);
  b.rod(WOOD2, {0.6f, f + 0.2f, 0}, {0.6f, f + 3.2f, 0}, 0.04f, 0.035f, 5);
  b.cloth(boxGeo(1.1f, 0.6f, 0.03f, 6, 3, 1), SAFFRON, Pivot::XNeg, {1.15f, f + 2.8f, 0});
  b.cloth(boxGeo(1.1f, 0.07f, 0.035f, 6, 1, 1), RED, Pivot::XNeg, {1.15f, f + 2.46f, 0});

  for (int sx = -1; sx <= 1; sx += 2)
    for (int sz = -1; sz <= 1; sz += 2) {
      float x = sx * (PLINTH_HALF - 0.5f), z = sz * (PLINTH_HALF - 0.5f);
      b.tube(STONE3, {x, PT + 0.5f, z}, 0.16f, 0.22f, 1.0f, 6).tube(STONE, {x, PT + 1.05f, z}, 0.28f, 0.18f, 0.14f, 8);
      glow.ball(LAMP, {x, PT + 1.2f, z}, {0.09f, 0.13f, 0.09f}, Z0, 0);
      lights.push_back({x, PT + 1.25f, z});
    }

  for (int line = 0; line < 4; line++) {
    float a = PI / 4 + (line * PI) / 2;
    float cx = std::sin(a) * (COURT - 2.2f) * std::sqrt(2.0f), cz = std::cos(a) * (COURT - 2.2f) * std::sqrt(2.0f);
    flagString(b, {0, f + 0.6f, 0}, {cx, 4.5f, cz}, 18, 1.6f, line);
  }
  for (int side = 0; side < 4; side++) {
    float a = (side * PI) / 2;
    float c = std::cos(a), s = std::sin(a);
    float K = COURT - 2.2f;
    V p0{c * -K + s * K, 4.4f, -s * -K + c * K};
    V p1{c * K + s * K, 4.4f, -s * K + c * K};
    if (side == 0) {
      flagString(b, p0, {-GATE - 0.45f, 5.0f, COURT}, 8, 0.7f, side);
      flagString(b, {GATE + 0.45f, 5.0f, COURT}, p1, 8, 0.7f, side + 2);
    } else flagString(b, p0, p1, 16, 1.2f, side);
  }

  for (int i = 0; i < 4; i++) {
    float a = (i * PI) / 2;
    float c = std::cos(a), s = std::sin(a);
    for (float t : {-1.8f, 1.8f}) glow.box(WIN, {c * t + s * (SANCTUM_HALF + 0.12f), PAVE_TOP + PLINTH_H + 1.1f, -s * t + c * (SANCTUM_HALF + 0.12f)}, {0.5f, 0.7f, 0.02f}, {0, a, 0});
  }
  if (g_lodPass) g_lodMesh = uploadMesh(b.build(false));
  else {
    g_mesh = uploadMesh(b.build(false));
    g_glow = uploadMesh(glow.build(false));
  }
  glowShape = nullptr;
}

const float R2 = (COURT + 2) * (COURT + 2) * 2;

void toLocal(const Shrine& sh, float x, float z, float& lx, float& lz) {
  float c = std::cos(sh.yaw), s = std::sin(sh.yaw);
  float dx = x - sh.x, dz = z - sh.z;
  lx = dx * c - dz * s;
  lz = dx * s + dz * c;
}

const float POSTS[9][3] = {
    {-(COURT - 2.2f), -(COURT - 2.2f), 1.5f}, {COURT - 2.2f, -(COURT - 2.2f), 1.5f}, {-(COURT - 2.2f), COURT - 2.2f, 1.5f}, {COURT - 2.2f, COURT - 2.2f, 1.5f},
    {-(GATE + 0.45f), COURT, 0.75f}, {GATE + 0.45f, COURT, 0.75f}, {6.5f, 9.6f, 0.9f}, {-3.7f, PLINTH_HALF + 1.6f, 0.7f}, {3.7f, PLINTH_HALF + 1.6f, 0.7f},
};

}  // namespace

namespace temple {

const Mesh& model() { build(); return g_mesh; }
const Mesh& glow() { build(); return g_glow; }
const Mesh& modelLod() { build(); return g_lodMesh; }
const std::vector<Vec3>& lights() { build(); return g_lights; }

float ground(float x, float z, float h) {
  for (const Shrine& sh : world::SHRINES) {
    float dx = x - sh.x, dz = z - sh.z;
    if (dx * dx + dz * dz > R2) continue;
    float lx, lz;
    toLocal(sh, x, z, lx, lz);
    float ax = std::fabs(lx), az = std::fabs(lz);
    if (ax > COURT + 0.35f || az > COURT + 0.35f) continue;
    float y = PAVE_TOP;
    bool onWall = (ax > COURT - 0.35f || az > COURT - 0.35f) && !(lz > 0 && ax < GATE && az > COURT - 0.4f);
    if (onWall) y = WALL_H + 0.12f;
    else if (ax <= PLINTH_HALF && az <= PLINTH_HALF) y = PAVE_TOP + PLINTH_H;
    else if (ax <= 2.2f && lz > PLINTH_HALF && lz < PLINTH_HALF + 1.5f) y = PAVE_TOP + std::ceil((1 - (lz - PLINTH_HALF) / 1.5f) * 3) / 3 * PLINTH_H;
    h = std::max(h, sh.y + y);
  }
  return h;
}

void resolveCollision(float& px, float& pz, float radius) {
  for (const Shrine& sh : world::SHRINES) {
    float dx = px - sh.x, dz = pz - sh.z;
    if (dx * dx + dz * dz > R2) continue;
    float lx, lz;
    toLocal(sh, px, pz, lx, lz);
    float c = std::cos(sh.yaw), s = std::sin(sh.yaw);
    float nx = lx, nz = lz;
    float m = SANCTUM_HALF + 0.2f + radius;
    if (std::fabs(lx) < m && std::fabs(lz) < m) {
      float ppx = m - std::fabs(lx), ppz = m - std::fabs(lz);
      if (ppx < ppz) nx = (lx < 0 ? -1.0f : 1.0f) * m;
      else nz = (lz < 0 ? -1.0f : 1.0f) * m;
    }
    for (auto& P : POSTS) {
      float ox = nx - P[0], oz = nz - P[1];
      float d = hypot2(ox, oz), rr = P[2] + radius;
      if (d < rr && d > 1e-4f) { nx = P[0] + (ox / d) * rr; nz = P[1] + (oz / d) * rr; }
    }
    if (nx != lx || nz != lz) {
      px = sh.x + nx * c + nz * s;
      pz = sh.z - nx * s + nz * c;
    }
  }
}

const Shrine* nearShrine(float x, float z, float r) {
  for (const Shrine& sh : world::SHRINES)
    if (hypot2(sh.x - x, sh.z - z) < r) return &sh;
  return nullptr;
}

std::vector<Vec3> lightSpots() {
  const auto& L = lights();
  std::vector<Vec3> out;
  for (const Shrine& sh : world::SHRINES) {
    float c = std::cos(sh.yaw), s = std::sin(sh.yaw);
    for (auto& l : L) out.push_back({sh.x + l.x * c + l.z * s, sh.y + l.y, sh.z - l.x * s + l.z * c});
  }
  return out;
}

void shutdown() {
  g_mesh.destroy();
  g_glow.destroy();
  g_lodMesh.destroy();
  g_built = false;
  g_lights.clear();
}

}  // namespace temple
