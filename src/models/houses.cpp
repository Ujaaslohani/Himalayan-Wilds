#include "houses.h"
#include <map>
#include <memory>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "../platform/platform.h"

const KindInfo KINDS[SK_COUNT] = {
    {"cottage", "Small Cottage", KG_HOME, "Stone-and-timber home with a sloped roof and a small chimney. Houses 2.", {{"wood", 10}, {"stone", 6}}, 2, 1, 2.9f, {LO_ROOF, LO_WALL, LO_WOOD, LO_DOOR, LO_ACCENT, LO_DECO}},
    {"raised_house", "Raised Wooden House", KG_HOME, "Timber upper floor over a stone base, with a balcony and outside stairs. Houses 3.", {{"wood", 24}, {"stone", 12}, {"fiber", 6}}, 3, 2, 3.4f, {LO_ROOF, LO_WALL, LO_WOOD, LO_DOOR, LO_ACCENT, LO_DECO}},
    {"family_house", "Large Family House", KG_HOME, "A bigger two-storey home with a side wing and porch. Houses 5.", {{"wood", 45}, {"stone", 28}, {"fiber", 12}}, 5, 3, 4.6f, {LO_ROOF, LO_WALL, LO_WOOD, LO_DOOR, LO_ACCENT, LO_DECO}},
    {"communal_house", "Communal House", KG_HOME, "A long shared house for many residents. Houses 8.", {{"wood", 80}, {"stone", 50}, {"fiber", 22}}, 8, 3, 5.6f, {LO_ROOF, LO_WALL, LO_WOOD, LO_DOOR, LO_ACCENT, LO_DECO}},
    {"market_stall", "Market Stall", KG_VILLAGE, "Timber stall with a counter, baskets, pottery and hanging cloth.", {{"wood", 8}, {"fiber", 6}}, 0, 3, 2.3f, {LO_ROOF, LO_WOOD, LO_ACCENT}},
    {"animal_shelter", "Animal Shelter", KG_VILLAGE, "Roofed enclosure with rails, hay and storage.", {{"wood", 16}, {"fiber", 8}, {"stone", 4}}, 0, 2, 3.4f, {LO_ROOF, LO_WOOD}},
    {"community_hall", "Community Hall", KG_VILLAGE, "Large gathering hall with timber pillars, benches and carved beams.", {{"wood", 110}, {"stone", 80}, {"fiber", 30}}, 0, 8, 6.6f, {LO_ROOF, LO_WALL, LO_WOOD, LO_DOOR, LO_ACCENT}},
    {"food_store", "Food Store", KG_VILLAGE, "Raised timber granary on stone pillars. Holds 80 more food for winter.", {{"wood", 28}, {"stone", 20}}, 0, 1, 2.9f, {LO_ROOF, LO_WALL, LO_WOOD, LO_DOOR}},
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

const char* roofCol(const Look& L);
float roof(Shape& b, const Look& L, float w, float d, float y, float pitch, float ox = 0, float oz = 0) {
  // a real roof: overlapping courses of slates / shingles / thatch bundles, fascia and barge boards, exposed rafter
  // tails under the eaves, ridge cap, and the gable walls closing the ends
  float t = L.roof == R_THATCH ? 0.34f : L.roof == R_SHINGLE ? 0.07f : 0.06f;
  float o = L.roof == R_THATCH ? 0.6f : 0.45f;
  float tp = std::tan(pitch), cp = std::cos(pitch), sp = std::sin(pitch);
  float half = d / 2 + o;
  float len = half / cp;
  float ridge = y + (d / 2) * tp + t * 0.5f + 0.05f;
  const char* main = L.roof == R_THATCH ? "#c9a95c" : L.roof == R_SHINGLE ? "#8a5a34" : "#4b5563";
  const char* alt = L.roof == R_THATCH ? "#b8964c" : L.roof == R_SHINGLE ? "#7a4e2c" : "#5a636e";
  const char* under = L.roof == R_THATCH ? "#a8873f" : L.roof == R_SHINGLE ? "#6b4426" : "#374151";
  const char* w1 = WOODC[L.wood][0];
  const char* w2 = WOODC[L.wood][1];
  float W = w + o * 2;
  for (int s = 1; s >= -1; s -= 2) {
    // deck under the courses
    b.box(under, {ox, ridge - (half / 2) * tp - 0.04f, oz + s * half / 2}, {W, 0.05f, len}, {s * pitch, 0, 0});
    // courses from the eave up to the ridge, each overlapping the one below with a visible lip
    float course = L.roof == R_THATCH ? 0.42f : 0.26f;
    int n = std::max(3, (int)std::ceil(len / course));
    for (int i = 0; i < n; i++) {
      float u = (i + 0.5f) * len / n;  // distance down the slope from the ridge
      float zz = u * cp, yy = ridge - u * sp;
      const char* c = (i & 1) ? alt : main;
      b.box(c, {ox, yy + t * 0.5f, oz + s * zz}, {W + (L.roof == R_THATCH ? 0.05f : 0), t, len / n + 0.06f}, {s * pitch, 0, 0});
      if (L.roof == R_SLATE)  // staggered joints between slates
        for (float x = -W / 2 + ((i & 1) ? 0.2f : 0.4f); x < W / 2; x += 0.4f)
          b.box("#2d333b", {ox + x, yy + t + 0.004f, oz + s * zz}, {0.015f, 0.01f, len / n * 0.9f}, {s * pitch, 0, 0});
    }
    // fascia board along the eave and rafter tails under it
    float ez = oz + s * half, ey = ridge - half * tp;
    b.box(w1, {ox, ey - 0.05f, ez}, {W + 0.04f, 0.16f, 0.05f}, {s * pitch * 0.2f, 0, 0});
    for (float x = -W / 2 + 0.25f; x < W / 2; x += 0.5f) b.box(w2, {ox + x, ey - 0.06f, ez - s * 0.18f}, {0.08f, 0.1f, 0.36f}, {s * pitch, 0, 0});
  }
  // ridge cap and barge boards on the gable ends
  if (L.roof == R_THATCH) b.rod(under, {ox - W / 2, ridge + 0.12f, oz}, {ox + W / 2, ridge + 0.12f, oz}, 0.2f, 0.2f, 7);
  else {
    b.box(L.roof == R_SLATE ? "#374151" : w1, {ox, ridge + 0.08f, oz}, {W + 0.04f, 0.1f, 0.24f});
    for (float x = -W / 2; x <= W / 2 + 0.01f; x += W / 8) b.box(L.roof == R_SLATE ? "#2d333b" : w2, {ox + x, ridge + 0.14f, oz}, {0.1f, 0.05f, 0.16f}, {0, 0.785f, 0});
  }
  for (int e = -1; e <= 1; e += 2)
    for (int s = -1; s <= 1; s += 2)
      b.box(w1, {ox + e * (W / 2 + 0.02f), ridge - (half / 2) * tp + 0.02f, oz + s * half / 2}, {0.06f, 0.2f, len + 0.05f}, {s * pitch, 0, 0});
  // gable walls with a small vent and a carved finial at the peak
  prism(b, wallColor(L), d, (d / 2) * tp, 0.16f, {ox + w / 2 - 0.08f, y, oz}, PI / 2);
  prism(b, wallColor(L), d, (d / 2) * tp, 0.16f, {ox - w / 2 + 0.08f, y, oz}, PI / 2);
  for (int e = -1; e <= 1; e += 2) {
    b.box(GLASS, {ox + e * (w / 2 + 0.005f), y + (d / 2) * tp * 0.4f, oz}, {0.02f, 0.25f, 0.3f});
    b.box(w1, {ox + e * (w / 2 + 0.02f), y + (d / 2) * tp * 0.4f, oz}, {0.03f, 0.3f, 0.05f});
    b.cone(w1, {ox + e * (W / 2 + 0.03f), ridge + 0.05f, oz}, 0.06f, 0.3f, 5);
  }
  return ridge;
}

void walls(Shape& b, const Look& L, float w, float d, float y0, float h, float ox = 0, float oz = 0) {
  const char *w1 = WOODC[L.wood][0], *w2 = WOODC[L.wood][1];
  if (L.wall == W_STONE) {
    // Kath-Kuni: dry stone laced with timber courses every ~0.6 m, darker stone at the corners
    b.box(STONE[0], {ox, y0 + h / 2, oz}, {w, h, d});
    for (float yy = y0 + 0.55f; yy < y0 + h - 0.2f; yy += 0.6f) b.box(w1, {ox, yy, oz}, {w + 0.05f, 0.11f, d + 0.05f});
    for (int sx = -1; sx <= 1; sx += 2)
      for (int sz = -1; sz <= 1; sz += 2)
        for (float yy = y0 + 0.15f; yy < y0 + h; yy += 0.3f)
          b.box(STONE[((int)(yy * 7)) % 2 ? 1 : 2], {ox + sx * (w / 2 - 0.1f), yy, oz + sz * (d / 2 - 0.1f)}, {0.26f, 0.26f, 0.26f});
  } else if (L.wall == W_PLASTER) {
    b.box(PLASTER, {ox, y0 + h / 2, oz}, {w, h, d});
    b.box(w1, {ox, y0 + h - 0.08f, oz}, {w + 0.1f, 0.16f, d + 0.1f});
    b.box(w1, {ox, y0 + h * 0.5f, oz}, {w + 0.06f, 0.1f, d + 0.06f});
    b.box(STONE[1], {ox, y0 + 0.2f, oz}, {w + 0.04f, 0.4f, d + 0.04f});  // stone dado at the foot of the plaster
    for (int sz = -1; sz <= 1; sz += 2)  // half-timbering on the long walls
      for (float x = -w / 2 + 0.9f; x < w / 2 - 0.5f; x += 1.2f) {
        b.box(w1, {ox + x, y0 + h * 0.25f + 0.1f, oz + sz * (d / 2 + 0.02f)}, {0.08f, h * 0.5f - 0.2f, 0.04f}, {0, 0, 0.5f});
        b.box(w1, {ox + x, y0 + h * 0.75f - 0.05f, oz + sz * (d / 2 + 0.02f)}, {0.08f, h * 0.5f - 0.2f, 0.04f}, {0, 0, -0.5f});
      }
  } else {
    int n = std::max(3, (int)std::lround(h / 0.28f));
    for (int i = 0; i < n; i++) b.box(i % 2 ? w1 : w2, {ox, y0 + (i + 0.5f) * (h / n), oz}, {w + (i % 2 ? 0.08f : 0), h / n, d + (i % 2 ? 0.08f : 0)});
    for (int sx = -1; sx <= 1; sx += 2)  // notched log ends sticking out at the corners
      for (int sz = -1; sz <= 1; sz += 2)
        for (int i = 0; i < n; i += 2) b.box(w2, {ox + sx * (w / 2 + 0.12f), y0 + (i + 0.5f) * (h / n), oz + sz * (d / 2 - 0.05f)}, {0.24f, h / n * 0.9f, 0.2f});
  }
  for (int sx = -1; sx <= 1; sx += 2)
    for (int sz = -1; sz <= 1; sz += 2) b.box(w2, {ox + sx * w / 2, y0 + h / 2, oz + sz * d / 2}, {0.16f, h, 0.16f});
}

const char* trimColor(const Look& L) {
  // frames contrast with the wall: limed pale wood on dark timber / stone, dark carved wood on light plaster
  return L.wall == W_PLASTER ? WOODC[L.wood][0] : (L.wood == 1 ? "#5a3820" : "#c8b48c");
}

void windowAt(Shape& b, const Look& L, float x, float y, float z, std::vector<V>& lights, float face = 1) {
  // a Himachali window bay: deep carved frame, 2x3 panes behind a wooden lattice, arched carved pediment,
  // projecting sill on brackets with a flower box, painted shutters folded open
  const char* tr = trimColor(L);
  const char* w2 = WOODC[L.wood][1];
  const char* sh = L.accent != 0 ? ACCENTC[L.accent] : "#7a5229";
  float f = face;
  const float W = 0.62f, H = 0.74f;
  b.box("#4a6178", {x, y, z + 0.015f * f}, {W, H, 0.03f});                    // glass (sky-tinted)
  b.box("#2b3540", {x, y, z + 0.005f * f}, {W - 0.04f, H - 0.04f, 0.02f});    // room behind
  for (int i = 1; i < 3; i++) b.box(tr, {x, y - H / 2 + i * H / 3, z + 0.04f * f}, {W, 0.03f, 0.03f});  // glazing bars
  b.box(tr, {x, y, z + 0.04f * f}, {0.03f, H, 0.03f});
  for (int s = -1; s <= 1; s += 2) b.box(tr, {x + s * (W / 2 + 0.05f), y, z + 0.07f * f}, {0.1f, H + 0.2f, 0.12f});  // jambs
  b.box(tr, {x, y + H / 2 + 0.06f, z + 0.07f * f}, {W + 0.3f, 0.12f, 0.12f});  // head
  // arched pediment with a carved rosette
  b.add(cylinderGeo(W / 2 + 0.08f, W / 2 + 0.08f, 0.08f, 12, 1, false, 0, PI), tr, {x, y + H / 2 + 0.12f, z + 0.06f * f}, {PI / 2, PI / 2, 0});
  b.add(cylinderGeo(W / 2 - 0.04f, W / 2 - 0.04f, 0.09f, 12, 1, false, 0, PI), w2, {x, y + H / 2 + 0.12f, z + 0.065f * f}, {PI / 2, PI / 2, 0});
  b.ball(sh, {x, y + H / 2 + 0.22f, z + 0.12f * f}, {0.06f, 0.06f, 0.02f}, Z0, 1);
  for (int k = 0; k < 7; k++) {
    float a = PI * (k + 0.5f) / 7;
    b.box(tr, {x + std::cos(a) * (W / 2 - 0.02f), y + H / 2 + 0.12f + std::sin(a) * (W / 2 - 0.02f), z + 0.11f * f}, {0.05f, 0.05f, 0.03f}, {0, 0, a});
  }
  // sill, brackets and flower box
  b.box(tr, {x, y - H / 2 - 0.05f, z + 0.14f * f}, {W + 0.34f, 0.06f, 0.24f});
  for (int s = -1; s <= 1; s += 2) b.box(w2, {x + s * (W / 2 + 0.05f), y - H / 2 - 0.14f, z + 0.09f * f}, {0.07f, 0.14f, 0.12f});
  b.box("#6b4423", {x, y - H / 2 + 0.03f, z + 0.2f * f}, {W + 0.1f, 0.12f, 0.16f});
  for (int k = 0; k < 6; k++) {
    float fx = x - W / 2 + 0.08f + k * (W - 0.06f) / 5;
    b.ball("#3f6b2a", {fx, y - H / 2 + 0.12f, z + 0.2f * f}, {0.07f, 0.05f, 0.07f}, Z0, 0);
    b.ball(k % 3 == 0 ? "#dc2626" : k % 3 == 1 ? "#f59e0b" : "#f472b6", {fx + 0.02f, y - H / 2 + 0.17f, z + 0.22f * f}, {0.03f, 0.03f, 0.03f}, Z0, 0);
  }
  // shutters folded back: frame and two raised panels each
  for (int s = -1; s <= 1; s += 2) {
    float sx = x + s * (W / 2 + 0.27f);
    b.box(sh, {sx, y, z + 0.05f * f}, {0.32f, H + 0.04f, 0.04f});
    for (int r = -1; r <= 1; r += 2) b.box(w2, {sx, y + r * H * 0.23f, z + 0.075f * f}, {0.22f, H * 0.36f, 0.015f});
  }
  lights.push_back({x, y, z + 0.12f * face});
}

void doorAt(Shape& b, const Look& L, float x, float y0, float z, float h = 1.5f, float wd = 0.95f) {
  // a carved entrance: stone steps, heavy carved frame with stepped lintel and a painted panel, a double-leaf
  // panelled door with iron studs, a brass knocker and a protective sign (swastika-free knot motif) above
  const char* tr = trimColor(L);
  const char *w1 = WOODC[L.wood][0], *w2 = WOODC[L.wood][1];
  const char* leaf = L.wood == 0 ? "#7a4e2c" : w1;
  const char* acc = L.accent != 0 ? ACCENTC[L.accent] : "#b91c1c";
  b.box("#2a1a0e", {x, y0 + h / 2, z + 0.01f}, {wd, h, 0.04f});
  for (int s = -1; s <= 1; s += 2) {  // two leaves, each with three raised panels and a stile
    float lx = x + s * wd / 4;
    b.box(leaf, {lx, y0 + h / 2, z + 0.035f}, {wd / 2 - 0.02f, h - 0.02f, 0.05f});
    for (int r = 0; r < 3; r++) {
      float py = y0 + 0.2f + r * (h - 0.35f) / 3 + (h - 0.35f) / 6;
      b.box(w2, {lx, py, z + 0.065f}, {wd / 2 - 0.14f, (h - 0.35f) / 3 - 0.08f, 0.015f});
      b.box(leaf, {lx, py, z + 0.075f}, {wd / 2 - 0.2f, (h - 0.35f) / 3 - 0.14f, 0.012f});
    }
    for (int r = 0; r < 4; r++) b.ball("#2f2a26", {lx + (r & 1 ? 0.12f : -0.12f) * (wd / 0.95f), y0 + 0.12f + (r >> 1) * (h - 0.24f), z + 0.07f}, {0.015f, 0.015f, 0.01f}, Z0, 0);
  }
  b.add(torusGeo(0.05f, 0.01f, 4, 10), "#c8a24a", {x + 0.08f, y0 + h * 0.52f, z + 0.08f});
  b.add(torusGeo(0.05f, 0.01f, 4, 10), "#c8a24a", {x - 0.08f, y0 + h * 0.52f, z + 0.08f});
  // frame: jambs with carved bands, stepped lintel, painted panel with a knot motif
  for (int s = -1; s <= 1; s += 2) {
    float jx = x + s * (wd / 2 + 0.08f);
    b.box(tr, {jx, y0 + h / 2 + 0.05f, z + 0.06f}, {0.16f, h + 0.1f, 0.14f});
    for (int k = 0; k < 5; k++) b.box(w2, {jx, y0 + 0.2f + k * (h - 0.2f) / 4.5f, z + 0.135f}, {0.17f, 0.04f, 0.02f});
  }
  b.box(tr, {x, y0 + h + 0.08f, z + 0.07f}, {wd + 0.44f, 0.14f, 0.16f});
  b.box(tr, {x, y0 + h + 0.2f, z + 0.09f}, {wd + 0.6f, 0.1f, 0.2f});
  b.box(acc, {x, y0 + h + 0.36f, z + 0.06f}, {wd * 0.7f, 0.22f, 0.04f});
  b.add(torusGeo(0.07f, 0.012f, 4, 12), "#facc15", {x, y0 + h + 0.36f, z + 0.09f});
  for (int k = 0; k < 6; k++) b.box(tr, {x - wd * 0.3f + k * wd * 0.12f, y0 + h + 0.5f, z + 0.08f}, {0.07f, 0.07f, 0.04f}, {0, 0, PI / 4});
  if (L.door == D_ARCHED) b.add(torusGeo(wd / 2 + 0.12f, 0.06f, 4, 12, PI), tr, {x, y0 + h + 0.24f, z + 0.08f});
  // stone steps
  b.box(STONE[2], {x, y0 - 0.1f, z + 0.3f}, {wd + 0.6f, 0.14f, 0.5f});
  b.box(STONE[1], {x, y0 - 0.24f, z + 0.55f}, {wd + 0.8f, 0.14f, 0.5f});
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

// ---- baked building models (assets/buildings/*.hwb, made from Uğur Yakışık's CC BY medieval packs) ----
// per triangle: 3 corners, sRGB colour, class (0 keep, 1 wall, 2 roof, 3 wood, 4 lit window); classes 1-3 take the Look colours
struct HWB {
  bool ok = false;
  std::vector<float> p;
  std::vector<uint32_t> c, k;
  std::vector<V> lights;
  float ref[5] = {1, 1, 1, 1, 1};
  V mn{1e9f, 1e9f, 1e9f}, mx{-1e9f, -1e9f, -1e9f};
};
const HWB& hwb(const char* name) {
  static std::map<std::string, HWB> cache;
  auto it = cache.find(name);
  if (it != cache.end()) return it->second;
  HWB& h = cache[name];
  std::vector<uint8_t> b;
  if (!platform::readFile(platform::assetPath(std::string("buildings/") + name + ".hwb"), b) || b.size() < 36 || std::memcmp(b.data(), "HWB1", 4) != 0) return h;
  uint32_t nt, nl, nr;
  std::memcpy(&nt, &b[4], 4); std::memcpy(&nl, &b[8], 4); std::memcpy(&nr, &b[12], 4);
  size_t off = 16;
  for (uint32_t i = 0; i < nr && i < 5; i++) std::memcpy(&h.ref[i], &b[off + i * 4], 4);
  off += nr * 4;
  if (b.size() < off + nl * 12 + (size_t)nt * 44) return h;
  for (uint32_t i = 0; i < nl; i++) { float f[3]; std::memcpy(f, &b[off], 12); off += 12; h.lights.push_back({f[0], f[1], f[2]}); }
  h.p.resize((size_t)nt * 9); h.c.resize(nt); h.k.resize(nt);
  for (uint32_t i = 0; i < nt; i++) {
    std::memcpy(&h.p[(size_t)i * 9], &b[off], 36); std::memcpy(&h.c[i], &b[off + 36], 4); std::memcpy(&h.k[i], &b[off + 40], 4); off += 44;
    for (int v = 0; v < 3; v++) {
      const float* q = &h.p[(size_t)i * 9 + v * 3];
      h.mn = {std::min(h.mn.x, q[0]), std::min(h.mn.y, q[1]), std::min(h.mn.z, q[2])};
      h.mx = {std::max(h.mx.x, q[0]), std::max(h.mx.y, q[1]), std::max(h.mx.z, q[2])};
    }
  }
  h.ok = true;
  return h;
}
uint32_t tintPacked(uint32_t src, uint32_t target, float ref) {
  float r = (src & 255) / 255.f, g = ((src >> 8) & 255) / 255.f, bl = ((src >> 16) & 255) / 255.f;
  float l = 0.299f * r + 0.587f * g + 0.114f * bl;
  float f = std::min(1.6f, std::max(0.45f, l / std::max(0.02f, ref)));
  auto ch = [&](int sh) { return (uint32_t)std::min(255.f, std::round(((target >> sh) & 255) * f)) << sh; };
  return ch(0) | ch(8) | ch(16) | (255u << 24);
}
// adds a baked model; returns false (so the caller can fall back to the procedural build) when the file is missing
bool addModel(Shape& b, std::vector<V>& lights, const char* name, const Look& L, V off = {0, 0, 0}, int maxLights = 4) {
  const HWB& h = hwb(name);
  if (!h.ok) return false;
  uint32_t tgt[4] = {0, hexToPacked(wallColor(L)), hexToPacked(roofCol(L)), hexToPacked(WOODC[L.wood][0])};
  for (size_t i = 0; i < h.c.size(); i++) {
    const float* q = &h.p[i * 9];
    V a{q[0], q[1], q[2]}, c1{q[3], q[4], q[5]}, c2{q[6], q[7], q[8]};
    V n = (c1 - a).cross(c2 - a);
    float ln = n.length();
    n = ln > 1e-9f ? n * (1.0f / ln) : V{0, 1, 0};
    uint32_t k = h.k[i], col = (k >= 1 && k <= 3) ? tintPacked(h.c[i], tgt[k], h.ref[k]) : h.c[i];
    b.verts.push_back({a + off, n, col});
    b.verts.push_back({c1 + off, n, col});
    b.verts.push_back({c2 + off, n, col});
  }
  for (size_t i = 0; i < h.lights.size() && (int)i < maxLights; i++) lights.push_back(h.lights[(i * h.lights.size()) / std::min<size_t>(h.lights.size(), maxLights)] + off);
  return true;
}
// v0.9.87: homes sit on a stone plinth (0.4 m above the lowest ground, 1.6 m deep) so a slope never swallows the walls
bool addHouseModel(Out& m, const char* name, const Look& L) {
  const HWB& h = hwb(name);
  if (!h.ok) return false;
  const float lift = 0.4f;
  V c{(h.mn.x + h.mx.x) * 0.5f, 0, (h.mn.z + h.mx.z) * 0.5f};
  m.s.box("6b6760", {c.x, lift - 0.8f, c.z}, {(h.mx.x - h.mn.x) * 0.84f, 1.6f, (h.mx.z - h.mn.z) * 0.84f});
  addModel(m.s, m.lights, name, L, {0, lift, 0});
  m.door = {0, 0, h.mx.z + 1.0f};
  return true;
}
// collision box (local x0, x1, z0, z1) of a baked home; false for kinds that keep their own box
bool modelBoxImpl(int kind, float b[4]) {
  const char* n = kind == SK_COTTAGE ? "cottage" : kind == SK_RAISED_HOUSE ? "raised_house" : kind == SK_FAMILY_HOUSE ? "family_house" : nullptr;
  if (!n) return false;
  const HWB& h = hwb(n);
  if (!h.ok) return false;
  b[0] = h.mn.x * 0.84f; b[1] = h.mx.x * 0.84f; b[2] = h.mn.z * 0.84f; b[3] = h.mx.z * 0.84f;
  return true;
}
V modelFront(const char* name) { const HWB& h = hwb(name); return {0, 0, h.ok ? h.mx.z + 1.0f : 3.0f}; }

Out cottage(const Look& L) {
  { Out m; if (addHouseModel(m, "cottage", L)) return m; }
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
  {
    // front verandah: plank deck, carved posts, railing either side of the steps, lean-to roof
    const char *w1 = WOODC[L.wood][0], *w2 = WOODC[L.wood][1];
    float vz = d / 2 + 0.75f;
    b.box(w2, {0, y0 - 0.02f, vz}, {w + 0.2f, 0.08f, 1.5f});
    for (float px : {-w / 2, -0.75f, 0.75f, w / 2}) {
      b.box(w1, {px, y0 + 1.1f, vz + 0.65f}, {0.14f, 2.2f, 0.14f});
      b.box(w1, {px, y0 + 2.1f, vz + 0.65f}, {0.3f, 0.1f, 0.22f});
    }
    for (int s = -1; s <= 1; s += 2) {
      float cx = s * (w / 2 + 0.75f) / 2;
      b.box(w1, {cx, y0 + 0.85f, vz + 0.65f}, {w / 2 - 0.8f, 0.07f, 0.08f});
      for (float rx = s * 0.85f; std::fabs(rx) < w / 2 - 0.05f; rx += s * 0.22f) b.box(w2, {rx, y0 + 0.45f, vz + 0.65f}, {0.05f, 0.8f, 0.05f});
    }
    b.box(roofCol(L), {0, y0 + h + 0.05f, vz + 0.1f}, {w + 0.5f, 0.07f, 1.75f}, {0.22f, 0, 0});
    b.box(w1, {0, y0 + 2.2f, vz + 0.65f}, {w + 0.2f, 0.12f, 0.14f});
  }
  b.box(STONE[0], {w * 0.28f, ridge - 0.1f, -d * 0.18f}, {0.5f, 1.2f, 0.5f});
  b.box("#2b2522", {w * 0.28f, ridge + 0.52f, -d * 0.18f}, {0.56f, 0.08f, 0.56f});
  decorate(b, L, w, d, 0, o.lights);
  o.door = {0, 0, d / 2 + 2.0f};
  return o;
}

Out raisedHouse(const Look& L) {
  { Out m; if (addHouseModel(m, "raised_house", L)) return m; }
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
  { Out m; if (addHouseModel(m, "family_house", L)) return m; }
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
  { Out m; if (addModel(m.s, m.lights, "communal_house", L)) { m.door = modelFront("communal_house"); return m; } }
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
  { Out m; if (addModel(m.s, m.lights, "market_stall", L)) { m.door = modelFront("market_stall"); return m; } }
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
  if (!addModel(b, o.lights, "animal_shelter", L, {0, 0, -0.2f})) {
    for (auto& p : posts) b.rod(w2, {p[0], 0, p[1]}, {p[0], p[2], p[1]}, 0.09f, 0.09f, 6);
    b.box(roofCol(L), {0, 2.3f, 0}, {W + 0.6f, L.roof == R_THATCH ? 0.3f : 0.14f, D + 0.7f}, {-0.12f, 0, 0});
  }
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
  if (!addModel(b, o.lights, "food_store", L, {0, y0 - 0.02f, 0}, 2)) {
    walls(b, L, w, d, y0, 2.0f);
    roof(b, L, w, d, y0 + 2.0f, 0.6f);
    doorAt(b, L, 0, y0, d / 2, 1.4f, 0.8f);
  }
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
  { Out m; if (addModel(m.s, m.lights, "community_hall", L)) { m.door = modelFront("community_hall"); return m; } }
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
  MeshData data;  // CPU copy: lets the village merge all buildings into one mesh
};

std::map<std::string, std::unique_ptr<Entry>> g_cache;

std::string lookKey(StructureKind k, const Look& L) {
  char buf[96];
  std::snprintf(buf, sizeof buf, "%d|%d|%d|%d|%d|%d|%d", (int)k, L.roof, L.wall, L.wood, L.door, L.accent, L.deco);
  return buf;
}

}  // namespace
bool modelBox(int kind, float b[4]) { return modelBoxImpl(kind, b); }

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
  e->data = o.s.build(false);
  e->mesh = uploadMesh(e->data);
  e->model.data = &e->data;
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
