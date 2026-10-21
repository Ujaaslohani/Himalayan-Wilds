#include "houses.h"
#include <map>
#include <memory>
#include <algorithm>
#include <cstdio>

const KindInfo KINDS[SK_COUNT] = {
    {"cottage", "Small Cottage", KG_HOME, "Stone-and-timber home with a sloped roof and a small chimney. Houses 2.", {{"wood", 10}, {"stone", 6}}, 2, 1, 2.9f, {LO_ROOF, LO_WALL, LO_WOOD, LO_DOOR, LO_ACCENT, LO_DECO}},
    {"raised_house", "Raised Wooden House", KG_HOME, "Timber upper floor over a stone base, with a balcony and outside stairs. Houses 3.", {{"wood", 16}, {"stone", 8}, {"fiber", 4}}, 3, 2, 3.4f, {LO_ROOF, LO_WALL, LO_WOOD, LO_DOOR, LO_ACCENT, LO_DECO}},
    {"family_house", "Large Family House", KG_HOME, "A bigger two-storey home with a side wing and porch. Houses 5.", {{"wood", 24}, {"stone", 14}, {"fiber", 6}}, 5, 3, 4.6f, {LO_ROOF, LO_WALL, LO_WOOD, LO_DOOR, LO_ACCENT, LO_DECO}},
    {"communal_house", "Communal House", KG_HOME, "A long shared house for many residents. Houses 8.", {{"wood", 36}, {"stone", 22}, {"fiber", 10}}, 8, 3, 5.6f, {LO_ROOF, LO_WALL, LO_WOOD, LO_DOOR, LO_ACCENT, LO_DECO}},
    {"market_stall", "Market Stall", KG_VILLAGE, "Timber stall with a counter, baskets, pottery and hanging cloth.", {{"wood", 8}, {"fiber", 6}}, 0, 3, 2.3f, {LO_ROOF, LO_WOOD, LO_ACCENT}},
    {"animal_shelter", "Animal Shelter", KG_VILLAGE, "Roofed enclosure with rails, hay and storage.", {{"wood", 12}, {"fiber", 6}, {"stone", 2}}, 0, 2, 3.4f, {LO_ROOF, LO_WOOD}},
    {"community_hall", "Community Hall", KG_VILLAGE, "Large gathering hall with timber pillars, benches and carved beams.", {{"wood", 40}, {"stone", 30}, {"fiber", 12}}, 0, 8, 6.6f, {LO_ROOF, LO_WALL, LO_WOOD, LO_DOOR, LO_ACCENT}},
    {"food_store", "Food Store", KG_VILLAGE, "Raised timber granary on stone pillars. Holds 80 more food for winter.", {{"wood", 18}, {"stone", 12}}, 0, 1, 2.9f, {LO_ROOF, LO_WALL, LO_WOOD, LO_DOOR}},
    {"fence", "Fence", KG_DECOR, "A 3 m timber fence section.", {{"wood", 1}}, 0, 0, 0.6f, {LO_WOOD}},
    {"path", "Stone Path", KG_DECOR, "A 2 m square of flat stones.", {{"stone", 1}}, 0, 0, 0.6f, {}},
    {"lamp_post", "Lantern Post", KG_DECOR, "Lights the way at night.", {{"wood", 1}, {"fiber", 1}}, 0, 1, 0.6f, {LO_WOOD}},
    {"flagpole", "Village Flagpole", KG_DECOR, "A tall pole with a cloth banner.", {{"wood", 2}, {"fiber", 2}}, 0, 1, 0.8f, {LO_ACCENT}},
    {"bench", "Bench", KG_DECOR, "A place to sit by the path.", {{"wood", 2}}, 0, 0, 0.9f, {LO_WOOD}},
};

const char* const LOOK_LABELS[LO_COUNT][7] = {
    {"Slate", "Wood shingle", "Thatch"},
    {"Stone", "Plaster & beams", "Log timber"},
    {"Dark", "Honey", "Red-brown"},
    {"Plain", "Carved", "Arched"},
    {"None", "Blue", "Green", "Red", "Yellow"},
    {"Flower pots", "Lantern", "Firewood", "Bench", "Barrels", "Yard fence", "Hanging cloth"},
};
const int LOOK_COUNTS[LO_COUNT] = {3, 3, 3, 3, 5, 7};
const char* const LOOK_OPT_NAMES[LO_COUNT] = {"Roof", "Walls", "Wood", "Door", "Accent", "Decorations"};

StructureKind kindFromId(const std::string& id) {
  for (int i = 0; i < SK_COUNT; i++)
    if (id == KINDS[i].id) return (StructureKind)i;
  return SK_COUNT;
}

namespace {

using V = Vec3;
const V Z0{0, 0, 0};
const char* WOODC[3][2] = {{"#5a3820", "#3f2716"}, {"#a0703c", "#7a5229"}, {"#7c2d12", "#5c200c"}};
const char* ACCENTC[5] = {"#6b4a32", "#1d4ed8", "#15803d", "#b91c1c", "#ca8a04"};
const char* STONE[3] = {"#8b8478", "#6b665d", "#9a948a"};
const char* PLASTER = "#e8dcc4";
const char* GLASS = "#2a2018";
const char* LAMP = "#ffd27a";
enum { R_SLATE, R_SHINGLE, R_THATCH };
enum { W_STONE, W_PLASTER, W_TIMBER };
enum { D_PLAIN, D_CARVED, D_ARCHED };

void prism(Shape& b, const char* color, float base, float height, float depth, V pos, float rotY = 0) {
  b.add(prismGeo(base, height, depth), color, pos, {0, rotY, 0});
}

const char* wallColor(const Look& L) { return L.wall == W_PLASTER ? PLASTER : L.wall == W_TIMBER ? WOODC[L.wood][0] : STONE[0]; }

float roof(Shape& b, const Look& L, float w, float d, float y, float pitch, float ox = 0, float oz = 0) {
  float t = L.roof == R_THATCH ? 0.38f : L.roof == R_SHINGLE ? 0.16f : 0.12f;
  float o = L.roof == R_THATCH ? 0.6f : 0.4f;
  float tp = std::tan(pitch);
  float half = d / 2 + o;
  float len = half / std::cos(pitch);
  float ridge = y + (d / 2) * tp + t * 0.5f;
  const char* main = L.roof == R_THATCH ? "#c9a95c" : L.roof == R_SHINGLE ? "#8a5a34" : "#4b5563";
  const char* under = L.roof == R_THATCH ? "#a8873f" : L.roof == R_SHINGLE ? "#6b4426" : "#374151";
  for (int s = 1; s >= -1; s -= 2) {
    float c = half / 2;
    b.box(main, {ox, ridge - c * tp, oz + s * c}, {w + o * 2, t, len}, {s * pitch, 0, 0});
    if (L.roof == R_SHINGLE)
      for (int r = 1; r <= 3; r++) {
        float zz = (half * r) / 4;
        b.box(under, {ox, ridge - zz * tp + t * 0.55f, oz + s * zz}, {w + o * 2, 0.04f, 0.08f}, {s * pitch, 0, 0});
      }
  }
  if (L.roof == R_THATCH) b.rod(under, {ox - w / 2 - o, ridge + 0.05f, oz}, {ox + w / 2 + o, ridge + 0.05f, oz}, 0.22f, 0.22f, 7);
  else b.box(under, {ox, ridge + 0.04f, oz}, {w + o * 2, 0.12f, 0.26f});
  prism(b, wallColor(L), d, (d / 2) * tp, 0.16f, {ox + w / 2 - 0.08f, y, oz}, PI / 2);
  prism(b, wallColor(L), d, (d / 2) * tp, 0.16f, {ox - w / 2 + 0.08f, y, oz}, PI / 2);
  return ridge;
}

void walls(Shape& b, const Look& L, float w, float d, float y0, float h, float ox = 0, float oz = 0) {
  const char *w1 = WOODC[L.wood][0], *w2 = WOODC[L.wood][1];
  if (L.wall == W_STONE) {
    b.box(STONE[0], {ox, y0 + h / 2, oz}, {w, h, d});
    for (int k = 1; k < 3; k++) b.box(STONE[1], {ox, y0 + (h * k) / 3, oz}, {w + 0.04f, 0.12f, d + 0.04f});
  } else if (L.wall == W_PLASTER) {
    b.box(PLASTER, {ox, y0 + h / 2, oz}, {w, h, d});
    b.box(w1, {ox, y0 + h - 0.08f, oz}, {w + 0.1f, 0.16f, d + 0.1f});
    b.box(w1, {ox, y0 + h * 0.5f, oz}, {w + 0.06f, 0.1f, d + 0.06f});
  } else {
    int n = std::max(3, (int)std::lround(h / 0.28f));
    for (int i = 0; i < n; i++) b.box(i % 2 ? w1 : w2, {ox, y0 + (i + 0.5f) * (h / n), oz}, {w + (i % 2 ? 0.08f : 0), h / n, d + (i % 2 ? 0.08f : 0)});
  }
  for (int sx = -1; sx <= 1; sx += 2)
    for (int sz = -1; sz <= 1; sz += 2) b.box(w2, {ox + sx * w / 2, y0 + h / 2, oz + sz * d / 2}, {0.16f, h, 0.16f});
}

void windowAt(Shape& b, const Look& L, float x, float y, float z, std::vector<V>& lights, float face = 1) {
  const char* w1 = WOODC[L.wood][0];
  b.box(GLASS, {x, y, z + 0.03f * face}, {0.55f, 0.5f, 0.05f});
  b.box(w1, {x, y - 0.3f, z + 0.06f * face}, {0.7f, 0.07f, 0.1f});
  b.box(w1, {x, y + 0.28f, z + 0.05f * face}, {0.65f, 0.06f, 0.06f});
  if (L.accent != 0)
    for (int s = -1; s <= 1; s += 2) b.box(ACCENTC[L.accent], {x + s * 0.42f, y, z + 0.05f * face}, {0.24f, 0.55f, 0.04f});
  lights.push_back({x, y, z + 0.12f * face});
}

void doorAt(Shape& b, const Look& L, float x, float y0, float z, float h = 1.5f, float wd = 0.95f) {
  const char *w1 = WOODC[L.wood][0], *w2 = WOODC[L.wood][1];
  b.box(w2, {x, y0 + h / 2, z + 0.03f}, {wd, h, 0.06f});
  if (L.door == D_CARVED) {
    for (int s = -1; s <= 1; s += 2) b.box(w1, {x + s * wd * 0.22f, y0 + h * 0.55f, z + 0.07f}, {wd * 0.3f, h * 0.55f, 0.03f});
    for (int i = 0; i < 5; i++) b.box(w1, {x - wd / 2 + (i + 0.5f) * (wd / 5), y0 + h + 0.12f, z + 0.07f}, {0.12f, 0.12f, 0.04f}, {0, 0, PI / 4});
    b.box(w1, {x, y0 + h + 0.02f, z + 0.06f}, {wd + 0.3f, 0.1f, 0.08f});
  } else if (L.door == D_ARCHED) {
    b.add(cylinderGeo(wd / 2, wd / 2, 0.06f, 10, 1, false, 0, PI), w2, {x, y0 + h, z + 0.03f}, {PI / 2, PI / 2, 0});
    b.add(torusGeo(wd / 2 + 0.04f, 0.05f, 4, 10, PI), w1, {x, y0 + h, z + 0.06f});
  } else {
    b.box(w1, {x, y0 + h + 0.05f, z + 0.05f}, {wd + 0.2f, 0.1f, 0.08f});
  }
  b.ball("#d4a017", {x + wd * 0.32f, y0 + h * 0.5f, z + 0.09f}, {0.035f, 0.035f, 0.035f}, Z0, 0);
}

void decorate(Shape& b, const Look& L, float w, float d, float doorX, std::vector<V>& lights) {
  const char *w1 = WOODC[L.wood][0], *w2 = WOODC[L.wood][1];
  float fz = d / 2;
  if (L.has(DK_POTS))
    for (int s = -1; s <= 1; s += 2) {
      float x = doorX + s * 0.85f;
      b.tube("#b45f3a", {x, 0.2f, fz + 0.35f}, 0.16f, 0.12f, 0.34f, 8);
      b.ball(s > 0 ? "#e11d48" : "#facc15", {x, 0.46f, fz + 0.35f}, {0.2f, 0.14f, 0.2f});
      b.ball("#4d7c2f", {x, 0.4f, fz + 0.35f}, {0.22f, 0.1f, 0.22f});
    }
  if (L.has(DK_LANTERN)) {
    b.rod(w2, {doorX - 0.75f, 1.9f, fz + 0.05f}, {doorX - 0.75f, 1.9f, fz + 0.4f}, 0.025f, 0.025f, 4);
    b.box(w2, {doorX - 0.75f, 1.72f, fz + 0.4f}, {0.2f, 0.05f, 0.2f});
    b.box(LAMP, {doorX - 0.75f, 1.6f, fz + 0.4f}, {0.16f, 0.22f, 0.16f});
    lights.push_back({doorX - 0.75f, 1.6f, fz + 0.45f});
  }
  if (L.has(DK_FIREWOOD))
    for (int r = 0; r < 3; r++)
      for (int i = 0; i < 4 - r; i++) {
        float z = -d / 2 + 0.6f + (i + r * 0.5f) * 0.3f;
        b.rod("#6b4a32", {w / 2 + 0.25f, 0.14f + r * 0.25f, z}, {w / 2 + 1.1f, 0.14f + r * 0.25f, z}, 0.12f, 0.12f, 6);
      }
  if (L.has(DK_BENCH)) {
    float x = doorX + (L.has(DK_POTS) ? 1.9f : 1.2f);
    b.box(w1, {x, 0.45f, fz + 0.45f}, {1.3f, 0.08f, 0.36f});
    for (int s = -1; s <= 1; s += 2) b.box(w2, {x + s * 0.55f, 0.22f, fz + 0.45f}, {0.08f, 0.44f, 0.3f});
  }
  if (L.has(DK_BARRELS)) {
    float bs[2][2] = {{-w / 2 - 0.45f, fz - 0.4f}, {-w / 2 - 0.45f, fz - 1.1f}};
    for (auto& p : bs) {
      b.tube(w1, {p[0], 0.4f, p[1]}, 0.3f, 0.27f, 0.8f, 9);
      b.tube("#3f3f46", {p[0], 0.62f, p[1]}, 0.31f, 0.31f, 0.05f, 9);
      b.tube("#3f3f46", {p[0], 0.18f, p[1]}, 0.29f, 0.29f, 0.05f, 9);
    }
  }
  if (L.has(DK_FENCE)) {
    float yz = fz + 2.4f;
    float xs[2] = {-w / 2 - 0.2f, w / 2 + 0.2f};
    for (int i = 0; i <= 8; i++) {
      float x = xs[0] + (i / 8.0f) * (xs[1] - xs[0]);
      if (std::fabs(x - doorX) < 0.55f) continue;
      b.box(w2, {x, 0.45f, yz}, {0.08f, 0.9f, 0.08f});
    }
    b.box(w1, {doorX - (doorX - xs[0] + 0.6f) / 2 - 0.3f, 0.65f, yz}, {std::max(0.2f, doorX - xs[0] - 0.6f), 0.07f, 0.05f});
    b.box(w1, {doorX + (xs[1] - doorX + 0.6f) / 2 + 0.3f, 0.65f, yz}, {std::max(0.2f, xs[1] - doorX - 0.6f), 0.07f, 0.05f});
    for (float x : xs) {
      for (int i = 0; i <= 3; i++) b.box(w2, {x, 0.45f, fz + (i / 3.0f) * 2.4f}, {0.08f, 0.9f, 0.08f});
      b.box(w1, {x, 0.65f, fz + 1.2f}, {0.05f, 0.07f, 2.4f});
    }
  }
  if (L.has(DK_CLOTH)) {
    float x0 = -w / 2 - 0.9f, x1 = -w / 2 - 0.9f;
    b.rod(w2, {x0, 0, fz - 0.2f}, {x0, 2.0f, fz - 0.2f}, 0.04f, 0.04f, 5);
    b.rod(w2, {x1, 0, fz - 2.6f}, {x1, 2.0f, fz - 2.6f}, 0.04f, 0.04f, 5);
    b.rod("#e7e0d0", {x0, 1.95f, fz - 0.2f}, {x1, 1.95f, fz - 2.6f}, 0.01f, 0.01f, 3);
    const char* cols[3] = {ACCENTC[L.accent == 0 ? 3 : L.accent], "#f5f0e6", "#ca8a04"};
    for (int i = 0; i < 3; i++) b.box(cols[i], {x0, 1.6f, fz - 0.6f - i * 0.7f}, {0.03f, 0.65f, 0.5f});
  }
}

void plinth(Shape& b, float w, float d) { b.box(STONE[1], {0, 0.17f, 0}, {w + 0.3f, 0.34f, d + 0.3f}); }

struct Out {
  Shape s;
  std::vector<V> lights;
  V door;
};

Out cottage(const Look& L) {
  Out o;
  Shape& b = o.s;
  float w = 4.2f, d = 3.6f, h = 2.3f, y0 = 0.34f;
  plinth(b, w, d);
  walls(b, L, w, d, y0, h);
  doorAt(b, L, 0, y0, d / 2);
  windowAt(b, L, -1.35f, y0 + 1.35f, d / 2, o.lights);
  windowAt(b, L, 1.35f, y0 + 1.35f, d / 2, o.lights);
  windowAt(b, L, 0, y0 + 1.35f, -d / 2, o.lights, -1);
  float ridge = roof(b, L, w, d, y0 + h, 0.62f);
  b.box(STONE[0], {w * 0.28f, ridge - 0.1f, -d * 0.18f}, {0.5f, 1.2f, 0.5f});
  b.box("#2b2522", {w * 0.28f, ridge + 0.52f, -d * 0.18f}, {0.56f, 0.08f, 0.56f});
  decorate(b, L, w, d, 0, o.lights);
  o.door = {0, 0, d / 2 + 0.9f};
  return o;
}

Out raisedHouse(const Look& L) {
  Out o;
  Shape& b = o.s;
  float w = 4.4f, d = 3.8f;
  const char *w1 = WOODC[L.wood][0], *w2 = WOODC[L.wood][1];
  plinth(b, w, d);
  Look ls = L;
  ls.wall = W_STONE;
  walls(b, ls, w, d, 0.34f, 1.8f);
  doorAt(b, L, -1.1f, 0.34f, d / 2, 1.35f, 0.85f);
  windowAt(b, L, 1.2f, 1.2f, d / 2, o.lights);
  b.box(w2, {0, 2.24f, 0.3f}, {w + 0.6f, 0.2f, d + 0.6f});
  Look lt = L;
  if (L.wall == W_STONE) lt.wall = W_TIMBER;
  walls(b, lt, w + 0.4f, d + 0.4f, 2.34f, 1.8f);
  doorAt(b, L, 0, 2.34f, (d + 0.4f) / 2, 1.4f, 0.85f);
  windowAt(b, L, -1.4f, 3.3f, (d + 0.4f) / 2, o.lights);
  windowAt(b, L, 1.4f, 3.3f, (d + 0.4f) / 2, o.lights);
  float by = 2.34f, bz = (d + 0.4f) / 2 + 0.55f;
  b.box(w1, {0, by - 0.05f, bz}, {w + 0.4f, 0.12f, 1.1f});
  for (int i = 0; i <= 8; i++) b.box(w2, {-(w + 0.4f) / 2 + (i / 8.0f) * (w + 0.4f), by + 0.45f, bz + 0.5f}, {0.07f, 0.9f, 0.07f});
  b.box(w1, {0, by + 0.9f, bz + 0.5f}, {w + 0.5f, 0.08f, 0.1f});
  for (int s = -1; s <= 1; s += 2) b.box(w2, {s * (w + 0.4f) / 2, by / 2, bz + 0.45f}, {0.14f, by, 0.14f});
  float sx = (w + 0.4f) / 2 + 0.55f;
  for (int i = 0; i < 7; i++) {
    float t = (i + 1) / 7.0f;
    b.box(w1, {sx, t * by - 0.05f, bz - 0.4f - (1 - t) * 3.2f}, {0.9f, 0.1f, 0.4f});
  }
  b.rod(w2, {sx + 0.45f, 0, bz - 3.6f}, {sx + 0.45f, by + 0.9f, bz - 0.4f}, 0.04f, 0.04f, 4);
  float ridge = roof(b, L, w + 0.4f, d + 0.4f, 4.14f, 0.55f);
  b.box(STONE[0], {-(w * 0.25f), ridge - 0.2f, -d * 0.2f}, {0.45f, 1.1f, 0.45f});
  decorate(b, L, w, d, -1.1f, o.lights);
  o.door = {-1.1f, 0, d / 2 + 1.0f};
  return o;
}

Out familyHouse(const Look& L) {
  Out o;
  Shape& b = o.s;
  float w = 6, d = 4.6f, h = 3.9f, y0 = 0.34f;
  const char *w1 = WOODC[L.wood][0], *w2 = WOODC[L.wood][1];
  plinth(b, w, d);
  walls(b, L, w, d, y0, h);
  doorAt(b, L, 0, y0, d / 2, 1.6f, 1.1f);
  for (float x : {-1.9f, 1.9f}) windowAt(b, L, x, y0 + 1.2f, d / 2, o.lights);
  for (float x : {-1.9f, 0.0f, 1.9f}) windowAt(b, L, x, y0 + 3.0f, d / 2, o.lights);
  windowAt(b, L, -1.5f, y0 + 1.2f, -d / 2, o.lights, -1);
  float ridge = roof(b, L, w, d, y0 + h, 0.52f);
  float wx = w / 2 + 1.6f, wz = -0.5f, ww = 3.2f, wd = 3.6f, wh = 2.4f;
  b.box(STONE[1], {wx, 0.17f, wz}, {ww + 0.3f, 0.34f, wd + 0.3f});
  walls(b, L, ww, wd, y0, wh, wx, wz);
  windowAt(b, L, wx, y0 + 1.3f, wz + wd / 2, o.lights);
  roof(b, L, ww, wd, y0 + wh, 0.55f, wx, wz);
  for (int s = -1; s <= 1; s += 2) b.box(w2, {s * 0.95f, 1.2f, d / 2 + 1.1f}, {0.16f, 2.4f, 0.16f});
  b.box(w1, {0, 2.45f, d / 2 + 0.75f}, {2.4f, 0.1f, 1.6f}, {0.25f, 0, 0});
  for (float x : {-w * 0.3f, w * 0.3f}) {
    b.box(STONE[0], {x, ridge - 0.1f, -d * 0.15f}, {0.55f, 1.4f, 0.55f});
    b.box("#2b2522", {x, ridge + 0.62f, -d * 0.15f}, {0.6f, 0.08f, 0.6f});
  }
  decorate(b, L, w, d, 0, o.lights);
  o.door = {0, 0, d / 2 + 1.9f};
  return o;
}

Out communalHouse(const Look& L) {
  Out o;
  Shape& b = o.s;
  float w = 9, d = 5, h = 3.7f, y0 = 0.34f;
  const char* w1 = WOODC[L.wood][0];
  plinth(b, w, d);
  walls(b, L, w, d, y0, h);
  for (float x : {-2.2f, 2.2f}) doorAt(b, L, x, y0, d / 2, 1.55f, 1.0f);
  for (float x : {-3.6f, 0.0f, 3.6f}) windowAt(b, L, x, y0 + 1.2f, d / 2, o.lights);
  for (float x : {-3.6f, -1.2f, 1.2f, 3.6f}) windowAt(b, L, x, y0 + 2.9f, d / 2, o.lights);
  for (float x : {-2.4f, 2.4f}) windowAt(b, L, x, y0 + 1.2f, -d / 2, o.lights, -1);
  float ridge = roof(b, L, w, d, y0 + h, 0.5f);
  b.box(w1, {0, y0 + h + 0.05f, d / 2 + 0.2f}, {w + 0.4f, 0.14f, 0.12f});
  for (float x : {-w * 0.32f, 0.0f, w * 0.32f}) {
    b.box(STONE[0], {x, ridge - 0.1f, -d * 0.1f}, {0.5f, 1.3f, 0.5f});
    b.box("#2b2522", {x, ridge + 0.57f, -d * 0.1f}, {0.55f, 0.08f, 0.55f});
  }
  decorate(b, L, w, d, 0, o.lights);
  o.door = {0, 0, d / 2 + 1.0f};
  return o;
}

const char* roofCol(const Look& L) { return L.roof == R_THATCH ? "#c9a95c" : L.roof == R_SHINGLE ? "#8a5a34" : "#4b5563"; }

Out marketStall(const Look& L) {
  Out o;
  Shape& b = o.s;
  const char *w1 = WOODC[L.wood][0], *w2 = WOODC[L.wood][1];
  float W = 3.2f, D = 2.2f;
  float posts[4][3] = {{-W / 2, D / 2, 2.4f}, {W / 2, D / 2, 2.4f}, {-W / 2, -D / 2, 2.1f}, {W / 2, -D / 2, 2.1f}};
  for (auto& p : posts) b.rod(w2, {p[0], 0, p[1]}, {p[0], p[2], p[1]}, 0.07f, 0.07f, 6);
  b.box(roofCol(L), {0, 2.3f, 0}, {W + 0.7f, L.roof == R_THATCH ? 0.28f : 0.12f, D + 0.8f}, {-0.14f, 0, 0});
  b.box(w1, {0, 0.5f, D / 2 - 0.2f}, {W, 1.0f, 0.45f});
  b.box(w2, {0, 1.03f, D / 2 - 0.2f}, {W + 0.1f, 0.06f, 0.55f});
  const char* produce[4] = {"#dc2626", "#f97316", "#65a30d", "#facc15"};
  for (int i = 0; i < 4; i++) {
    float x = -W / 2 + 0.45f + i * 0.75f;
    b.tube("#b08850", {x, 1.16f, D / 2 - 0.2f}, 0.24f, 0.18f, 0.2f, 8);
    b.ball(produce[i], {x, 1.3f, D / 2 - 0.2f}, {0.2f, 0.1f, 0.2f});
  }
  for (int i = 0; i < 3; i++) b.tube("#b45f3a", {-W / 2 + 0.5f + i * 0.45f, 0.3f, -D / 2 + 0.3f}, 0.14f + i * 0.03f, 0.1f, 0.5f + i * 0.12f, 8);
  b.ball("#b45f3a", {W / 2 - 0.5f, 0.3f, -D / 2 + 0.4f}, {0.28f, 0.3f, 0.28f});
  const char* cloth = ACCENTC[L.accent == 0 ? 3 : L.accent];
  for (int i = 0; i < 6; i++) b.box(i % 2 ? "#f5f0e6" : cloth, {-W / 2 + 0.27f + i * 0.53f, 2.05f, D / 2 + 0.3f}, {0.5f, 0.35f, 0.03f});
  b.box(LAMP, {W / 2 - 0.2f, 1.85f, D / 2 + 0.15f}, {0.14f, 0.2f, 0.14f});
  o.lights.push_back({W / 2 - 0.2f, 1.85f, D / 2 + 0.2f});
  o.door = {0, 0, D / 2 + 1.2f};
  return o;
}

Out animalShelter(const Look& L) {
  Out o;
  Shape& b = o.s;
  const char *w1 = WOODC[L.wood][0], *w2 = WOODC[L.wood][1];
  float W = 5, D = 3.5f;
  float posts[6][3] = {{-W / 2, D / 2, 2.4f}, {0, D / 2, 2.4f}, {W / 2, D / 2, 2.4f}, {-W / 2, -D / 2, 2.0f}, {0, -D / 2, 2.0f}, {W / 2, -D / 2, 2.0f}};
  for (auto& p : posts) b.rod(w2, {p[0], 0, p[1]}, {p[0], p[2], p[1]}, 0.09f, 0.09f, 6);
  b.box(roofCol(L), {0, 2.3f, 0}, {W + 0.6f, L.roof == R_THATCH ? 0.3f : 0.14f, D + 0.7f}, {-0.12f, 0, 0});
  for (int i = 0; i < 6; i++) b.box(i % 2 ? w1 : w2, {0, 0.2f + i * 0.32f, -D / 2}, {W, 0.3f, 0.08f});
  for (float y : {0.45f, 0.95f}) {
    b.box(w1, {0, y, D / 2}, {W, 0.08f, 0.08f});
    for (int s = -1; s <= 1; s += 2) b.box(w1, {s * W / 2, y, 0}, {0.08f, 0.08f, D});
  }
  float hay[3][2] = {{-1.5f, -0.8f}, {-0.6f, -1.0f}, {-1.1f, -0.2f}};
  for (auto& p : hay) b.box("#d6b85a", {p[0], 0.3f, p[1]}, {0.9f, 0.55f, 0.55f}, {0, p[0], 0});
  b.box("#d6b85a", {-1.1f, 0.85f, -0.7f}, {0.9f, 0.55f, 0.55f}, {0, 0.3f, 0});
  float crates[2][2] = {{1.6f, -1.0f}, {1.6f, -0.3f}};
  for (auto& p : crates) {
    b.box(w1, {p[0], 0.3f, p[1]}, {0.6f, 0.6f, 0.6f});
    b.box(w2, {p[0], 0.3f, p[1] + 0.31f}, {0.62f, 0.08f, 0.02f});
  }
  b.tube("#6b7280", {0.6f, 0.2f, 0.8f}, 0.45f, 0.4f, 0.35f, 10);
  o.door = {0, 0, D / 2 + 1.2f};
  return o;
}

Out foodStore(const Look& L) {
  Out o;
  Shape& b = o.s;
  const char *w1 = WOODC[L.wood][0], *w2 = WOODC[L.wood][1];
  float w = 3.6f, d = 3.0f, y0 = 1.13f;
  for (int sx = -1; sx <= 1; sx += 2)
    for (int sz = -1; sz <= 1; sz += 2) {
      b.tube(STONE[1], {sx * 1.4f, 0.45f, sz * 1.1f}, 0.22f, 0.27f, 0.9f, 6);
      b.tube(STONE[2], {sx * 1.4f, 0.95f, sz * 1.1f}, 0.38f, 0.3f, 0.1f, 8);
    }
  b.box(w2, {0, 1.05f, 0}, {w + 0.3f, 0.16f, d + 0.3f});
  walls(b, L, w, d, y0, 2.0f);
  roof(b, L, w, d, y0 + 2.0f, 0.6f);
  doorAt(b, L, 0, y0, d / 2, 1.4f, 0.8f);
  for (int s = -1; s <= 1; s += 2) b.rod(w1, {s * 0.32f, 0, d / 2 + 1.0f}, {s * 0.32f, 1.2f, d / 2 + 0.18f}, 0.04f, 0.04f, 5);
  for (int i = 1; i < 5; i++) {
    float t = i / 5.0f;
    b.box(w1, {0, t * 1.2f, d / 2 + 1.0f - t * 0.82f}, {0.66f, 0.05f, 0.07f});
  }
  float sacks[3][2] = {{-2.2f, 0.9f}, {-2.2f, 0.2f}, {-2.3f, 1.5f}};
  for (auto& p : sacks) b.ball("#cbb48a", {p[0], 0.3f, p[1]}, {0.28f, 0.32f, 0.25f});
  b.ball("#b89c6c", {-2.2f, 0.75f, 0.6f}, {0.26f, 0.28f, 0.22f});
  b.tube("#a0703c", {2.25f, 0.26f, 1.0f}, 0.3f, 0.24f, 0.52f, 8).tube("#d6a33a", {2.25f, 0.54f, 1.0f}, 0.27f, 0.27f, 0.06f, 8);
  b.tube("#a0703c", {2.3f, 0.22f, 0.25f}, 0.26f, 0.2f, 0.44f, 8).tube("#8a3b1c", {2.3f, 0.46f, 0.25f}, 0.23f, 0.23f, 0.05f, 8);
  b.box(LAMP, {0.75f, y0 + 1.45f, d / 2 + 0.12f}, {0.14f, 0.2f, 0.14f});
  o.lights.push_back({0.75f, y0 + 1.45f, d / 2 + 0.2f});
  o.door = {0, 0, d / 2 + 1.7f};
  return o;
}

Out communityHall(const Look& L) {
  Out o;
  Shape& b = o.s;
  float w = 10, d = 7, h = 4.2f, y0 = 0.5f;
  const char *w1 = WOODC[L.wood][0], *w2 = WOODC[L.wood][1];
  b.box(STONE[1], {0, 0.25f, 0.8f}, {w + 1.2f, 0.5f, d + 2.4f});
  walls(b, L, w, d, y0, h);
  doorAt(b, L, -0.6f, y0, d / 2, 2.2f, 1.1f);
  doorAt(b, L, 0.6f, y0, d / 2, 2.2f, 1.1f);
  for (float x : {-3.8f, -2.2f, 2.2f, 3.8f}) windowAt(b, L, x, y0 + 1.8f, d / 2, o.lights);
  for (float x : {-3.0f, 0.0f, 3.0f}) windowAt(b, L, x, y0 + 1.8f, -d / 2, o.lights, -1);
  for (int i = 0; i < 6; i++) {
    float x = -w / 2 + 0.3f + (i * (w - 0.6f)) / 5;
    b.tube(w2, {x, y0 + h / 2, d / 2 + 1.6f}, 0.17f, 0.2f, h, 8);
    b.box(w1, {x, y0 + h - 0.15f, d / 2 + 1.6f}, {0.5f, 0.25f, 0.5f});
  }
  b.box(w1, {0, y0 + h, d / 2 + 1.6f}, {w + 0.4f, 0.22f, 0.3f});
  b.box(w2, {0, y0 + h + 0.15f, d / 2 + 0.8f}, {w + 0.4f, 0.12f, 1.9f});
  float ridge = roof(b, L, w, d, y0 + h, 0.5f);
  for (int s = 1; s >= -1; s -= 2)
    for (int i = 0; i < 18; i++) {
      float x = -w / 2 - 0.2f + (i + 0.5f) * ((w + 0.4f) / 18);
      b.box(i % 2 ? w1 : w2, {x, y0 + h - 0.1f, s * (d / 2 + 0.42f)}, {0.22f, 0.22f, 0.06f}, {0, 0, PI / 4});
    }
  b.box(ACCENTC[L.accent == 0 ? 4 : L.accent], {0, ridge + 0.35f, 0}, {0.5f, 0.5f, 0.08f}, {0, 0, PI / 4});
  for (int s = -1; s <= 1; s += 2) {
    b.box(w1, {s * 2.8f, 0.95f, d / 2 + 0.9f}, {2.4f, 0.1f, 0.45f});
    for (int k = -1; k <= 1; k += 2) b.box(w2, {s * 2.8f + k * 1.0f, 0.72f, d / 2 + 0.9f}, {0.1f, 0.45f, 0.4f});
  }
  o.lights.push_back({-1.6f, 2.6f, d / 2 + 1.7f});
  o.lights.push_back({1.6f, 2.6f, d / 2 + 1.7f});
  b.box(LAMP, {-1.6f, 2.6f, d / 2 + 1.7f}, {0.18f, 0.26f, 0.18f});
  b.box(LAMP, {1.6f, 2.6f, d / 2 + 1.7f}, {0.18f, 0.26f, 0.18f});
  o.door = {0, 0, d / 2 + 2.8f};
  return o;
}

Out decor(StructureKind kind, const Look& L) {
  Out o;
  Shape& b = o.s;
  const char *w1 = WOODC[L.wood][0], *w2 = WOODC[L.wood][1];
  if (kind == SK_FENCE) {
    for (int i = 0; i <= 3; i++) b.box(w2, {-1.5f + i, 0.5f, 0}, {0.1f, 1.0f, 0.1f});
    for (float y : {0.4f, 0.8f}) b.box(w1, {0, y, 0}, {3.0f, 0.08f, 0.06f});
  } else if (kind == SK_PATH) {
    float tiles[4][4] = {{-0.5f, -0.5f, 0.9f, 0.85f}, {0.48f, -0.52f, 0.85f, 0.9f}, {-0.52f, 0.5f, 0.86f, 0.9f}, {0.5f, 0.48f, 0.9f, 0.86f}};
    for (int i = 0; i < 4; i++) b.box(STONE[i % 3], {tiles[i][0], 0.04f, tiles[i][1]}, {tiles[i][2], 0.08f, tiles[i][3]}, {0, i * 0.08f, 0});
  } else if (kind == SK_LAMP_POST) {
    b.rod(w2, {0, 0, 0}, {0, 2.6f, 0}, 0.07f, 0.05f, 6);
    b.rod(w2, {0, 2.5f, 0}, {0.45f, 2.5f, 0}, 0.035f, 0.035f, 4);
    b.box(w2, {0.45f, 2.38f, 0}, {0.24f, 0.05f, 0.24f});
    b.box(LAMP, {0.45f, 2.24f, 0}, {0.18f, 0.26f, 0.18f});
    o.lights.push_back({0.45f, 2.24f, 0});
  } else if (kind == SK_FLAGPOLE) {
    b.tube(STONE[1], {0, 0.2f, 0}, 0.4f, 0.5f, 0.4f, 8);
    b.rod("#7a4b2a", {0, 0.3f, 0}, {0, 6.2f, 0}, 0.07f, 0.04f, 6);
    b.cloth(boxGeo(1.2f, 0.8f, 0.03f, 6, 3, 1), ACCENTC[L.accent == 0 ? 4 : L.accent], Pivot::XNeg, {0.62f, 5.7f, 0});
    b.ball("#d4a017", {0, 6.25f, 0}, {0.09f, 0.09f, 0.09f}, Z0, 0);
  } else {
    b.box(w1, {0, 0.45f, 0}, {1.4f, 0.08f, 0.4f});
    b.box(w1, {0, 0.75f, -0.18f}, {1.4f, 0.3f, 0.06f});
    for (int s = -1; s <= 1; s += 2) b.box(w2, {s * 0.6f, 0.22f, 0}, {0.08f, 0.44f, 0.34f});
  }
  o.door = {0, 0, 1};
  return o;
}

struct Entry {
  Mesh mesh;
  BuiltModel model;
};

std::map<std::string, std::unique_ptr<Entry>> g_cache;

std::string lookKey(StructureKind k, const Look& L) {
  char buf[96];
  std::snprintf(buf, sizeof buf, "%d|%d|%d|%d|%d|%d|%d", (int)k, L.roof, L.wall, L.wood, L.door, L.accent, L.deco);
  return buf;
}

}  // namespace

const BuiltModel& structureModel(StructureKind kind, const Look& L) {
  std::string key = lookKey(kind, L);
  auto it = g_cache.find(key);
  if (it != g_cache.end()) return it->second->model;
  Out o = kind == SK_COTTAGE ? cottage(L)
        : kind == SK_RAISED_HOUSE ? raisedHouse(L)
        : kind == SK_FAMILY_HOUSE ? familyHouse(L)
        : kind == SK_COMMUNAL_HOUSE ? communalHouse(L)
        : kind == SK_MARKET_STALL ? marketStall(L)
        : kind == SK_ANIMAL_SHELTER ? animalShelter(L)
        : kind == SK_COMMUNITY_HALL ? communityHall(L)
        : kind == SK_FOOD_STORE ? foodStore(L)
        : decor(kind, L);
  auto e = std::make_unique<Entry>();
  e->mesh = uploadMesh(o.s.build(false));
  e->model.mesh = &e->mesh;
  e->model.lights = o.lights;
  e->model.door = o.door;
  BuiltModel& r = e->model;
  g_cache[key] = std::move(e);
  return r;
}

void pruneStructureModels(const std::vector<std::pair<StructureKind, Look>>& keep) {
  std::vector<std::string> keys;
  for (auto& k : keep) keys.push_back(lookKey(k.first, k.second));
  for (auto it = g_cache.begin(); it != g_cache.end();) {
    if (std::find(keys.begin(), keys.end(), it->first) == keys.end()) {
      it->second->mesh.destroy();
      it = g_cache.erase(it);
    } else ++it;
  }
}

void shutdownHouses() {
  for (auto& e : g_cache) e.second->mesh.destroy();
  g_cache.clear();
}
