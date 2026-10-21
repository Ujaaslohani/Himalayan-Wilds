#include "../world/forest.h"
#include "screens.h"
#include "ui.h"
#include "../platform/platform.h"
#include "../gfx/gfx.h"
#include "../audio/audio.h"
#include "../game/engine.h"
#include "../game/app.h"
#include "../game/economy.h"
#include "../game/season.h"
#include "../game/village.h"
#include "../game/resources.h"
#include "../game/player.h"
#include "../models/houses.h"
#include "../render/render.h"
#include "../world/terrain.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace ui;

namespace {

uint32_t C(const char* h, float a = 1) { return rgba(h, a); }

const char* ICON_HEX[MI_COUNT] = {"#c2773a", "#7d9bc1", "#9bc53d", "#b26ad6", "#e05d44", "#f2b31d", "#3fb6a8", "#ffd23f", "#e8a15c", "#d6c3a1", "#f2d16b", "#ef4444"};
float ICON_RANGE[MI_COUNT] = {55, 55, 50, 50, 70, 140, 110, 600, 0, 0, 0, 0};

struct Popup { std::string text; uint32_t color; double t0; };
std::vector<Popup> g_popups;
double g_hitT = -99, g_knockT = -99;
uint32_t g_hitCol = 0xffffffff;
std::vector<Marker> g_markers;
bool g_stats = false;
FrameStats g_fs;
Texture g_mapTex;
float g_scroll = 0, g_contentH = 0;
Tab g_lastTab = Tab::None;
int g_openKind = -1, g_openUid = -1;
int g_invSection = 0;
bool g_block = false;

double now() { return platform::time(); }

void ellipse(float cx, float cy, float rx, float ry, uint32_t c, int n = 18) {
  std::vector<Vec2> p;
  p.reserve(n);
  for (int i = 0; i < n; i++) {
    float a = (float)i / n * TAU;
    p.push_back({cx + std::cos(a) * rx, cy + std::sin(a) * ry});
  }
  poly(p, c);
}

void quadCurve(Vec2 a, Vec2 c, Vec2 b, float w, uint32_t col) {
  Vec2 prev = a;
  for (int i = 1; i <= 10; i++) {
    float t = i / 10.0f, u = 1 - t;
    Vec2 p{u * u * a.x + 2 * u * t * c.x + t * t * b.x, u * u * a.y + 2 * u * t * c.y + t * t * b.y};
    line(prev.x, prev.y, p.x, p.y, w, col);
    circle(p.x, p.y, w * 0.5f, col);
    prev = p;
  }
}

void glyph(MarkerIcon icon, float cx, float cy, float r, float alpha = 1) {
  circle(cx, cy, r, C("#14100c", 0.78f * alpha));
  uint32_t col = C(ICON_HEX[icon], alpha);
  ring(cx, cy, r * 0.9f, r * 0.14f, col);
  float u = r / 10;
  switch (icon) {
    case MI_WOOD:
      rect({cx - 5.5f * u, cy - 2.4f * u, 10 * u, 4.8f * u}, col);
      ellipse(cx + 4.6f * u, cy, 1.8f * u, 2.4f * u, C("#f0d3a3", alpha));
      break;
    case MI_STONE:
      poly({{cx - 5.5f * u, cy + 3.5f * u}, {cx - 3.5f * u, cy - 3 * u}, {cx + 1 * u, cy - 4.5f * u}, {cx + 5.5f * u, cy - 1 * u}, {cx + 4.5f * u, cy + 3.5f * u}}, col);
      break;
    case MI_FIBER:
      for (int k = -1; k <= 1; k++) quadCurve({cx + k * 1.5f * u, cy + 4.5f * u}, {cx + k * 1.5f * u, cy - 1 * u}, {cx + k * 4.5f * u, cy - 4.5f * u}, 1.6f * u, col);
      break;
    case MI_HERB:
      for (int i = 0; i < 5; i++) {
        float a = i / 5.0f * TAU;
        circle(cx + std::cos(a) * 2.6f * u, cy - 0.8f * u + std::sin(a) * 2.6f * u, 1.9f * u, col);
      }
      circle(cx, cy - 0.8f * u, 1.4f * u, C("#ffe066", alpha));
      break;
    case MI_HARVEST: {
      ellipse(cx, cy + 1.8f * u, 3 * u, 2.4f * u, col);
      const float T[4][2] = {{-3.4f, -1.6f}, {-1.2f, -3.8f}, {1.2f, -3.8f}, {3.4f, -1.6f}};
      for (auto& t : T) circle(cx + t[0] * u, cy + t[1] * u, 1.2f * u, col);
      break;
    }
    case MI_DANGER:
      for (int k = -1; k <= 1; k += 2) tri({cx + k * 4.2f * u, cy - 4 * u}, {cx + k * 1.2f * u, cy - 4 * u}, {cx + k * 2.6f * u, cy + 4.8f * u}, col);
      rect({cx - 5 * u, cy - 5.6f * u, 10 * u, 1.8f * u}, col);
      break;
    case MI_QUEST:
      poly({{cx, cy - 5.5f * u}, {cx + 4 * u, cy}, {cx, cy + 5.5f * u}, {cx - 4 * u, cy}}, col);
      break;
    default: {
      const char* ch = icon == MI_RECRUIT ? "!" : icon == MI_CLUE ? "?" : nullptr;
      if (ch) text(ch, cx, cy + 0.3f * u, 12 * u, col, SANS, 1);
      else circle(cx, cy, 2 * u, col);
    }
  }
}

void heart(float cx, float cy, float s, uint32_t c) {
  circle(cx - s * 0.26f, cy - s * 0.12f, s * 0.3f, c);
  circle(cx + s * 0.26f, cy - s * 0.12f, s * 0.3f, c);
  tri({cx - s * 0.54f, cy - 0.02f * s}, {cx + s * 0.54f, cy - 0.02f * s}, {cx, cy + s * 0.55f}, c);
}

void seasonGlyph(int i, float cx, float cy, float r, float a = 1) {
  if (i == 0) {
    for (int k = 0; k < 5; k++) {
      float an = k / 5.0f * TAU - PI / 2;
      circle(cx + std::cos(an) * r * 0.5f, cy + std::sin(an) * r * 0.5f, r * 0.42f, C("#f9a8d4", a));
    }
    circle(cx, cy, r * 0.3f, C("#fde047", a));
  } else if (i == 1) {
    for (int k = 0; k < 8; k++) {
      float an = k / 8.0f * TAU;
      line(cx + std::cos(an) * r * 0.55f, cy + std::sin(an) * r * 0.55f, cx + std::cos(an) * r, cy + std::sin(an) * r, r * 0.18f, C("#fbbf24", a));
    }
    circle(cx, cy, r * 0.45f, C("#fcd34d", a));
  } else if (i == 2) {
    poly({{cx, cy - r}, {cx + r * 0.7f, cy - r * 0.1f}, {cx, cy + r * 0.8f}, {cx - r * 0.7f, cy - r * 0.1f}}, C("#f97316", a));
    line(cx, cy - r * 0.6f, cx, cy + r, r * 0.12f, C("#7c2d12", a));
  } else {
    for (int k = 0; k < 3; k++) {
      float an = k / 3.0f * PI;
      line(cx - std::cos(an) * r, cy - std::sin(an) * r, cx + std::cos(an) * r, cy + std::sin(an) * r, r * 0.16f, C("#bae6fd", a));
    }
    circle(cx, cy, r * 0.18f, C("#e0f2fe", a));
  }
}

void itemIcon(const std::string& id, float cx, float cy, float r) {
  std::string k = ::itemIcon(id);
  if (k == "wood") return glyph(MI_WOOD, cx, cy, r);
  if (k == "stone") return glyph(MI_STONE, cx, cy, r);
  if (k == "fiber") return glyph(MI_FIBER, cx, cy, r);
  if (k == "herb") return glyph(MI_HERB, cx, cy, r);
  circle(cx, cy, r, C("#14100c", 0.78f));
  float u = r / 10;
  if (k == "food") {
    ring(cx, cy, r * 0.9f, r * 0.14f, C("#f59e0b"));
    rect({cx - 5 * u, cy - 0.5f * u, 10 * u, 1.4f * u}, C("#fbbf24"));
    ellipse(cx, cy + 1.5f * u, 4.5f * u, 2.8f * u, C("#b45309"));
    circle(cx - 1.5f * u, cy - 2 * u, 1.2f * u, C("#fde68a"));
    circle(cx + 1.6f * u, cy - 2.2f * u, 1.0f * u, C("#fde68a"));
  } else if (k == "arrow") {
    ring(cx, cy, r * 0.9f, r * 0.14f, C("#38bdf8"));
    line(cx - 5 * u, cy + 5 * u, cx + 4 * u, cy - 4 * u, 1.3f * u, C("#e7e5e4"));
    tri({cx + 5.5f * u, cy - 5.5f * u}, {cx + 1.5f * u, cy - 4 * u}, {cx + 4 * u, cy - 1.5f * u}, C("#e7e5e4"));
    line(cx - 5 * u, cy + 5 * u, cx - 5 * u, cy + 2 * u, 1.2f * u, C("#ef4444"));
    line(cx - 5 * u, cy + 5 * u, cx - 2 * u, cy + 5 * u, 1.2f * u, C("#ef4444"));
  } else if (k == "bow") {
    ring(cx, cy, r * 0.9f, r * 0.14f, C("#a16207"));
    quadCurve({cx - 3 * u, cy - 5.5f * u}, {cx + 6 * u, cy}, {cx - 3 * u, cy + 5.5f * u}, 1.5f * u, C("#ca8a04"));
    line(cx - 3 * u, cy - 5.5f * u, cx - 3 * u, cy + 5.5f * u, 0.6f * u, C("#fef3c7"));
  } else if (k == "bone") {
    ring(cx, cy, r * 0.9f, r * 0.14f, C("#e7e5e4"));
    line(cx - 4 * u, cy + 4 * u, cx + 4 * u, cy - 4 * u, 2 * u, C("#f5f5f4"));
    for (int s = -1; s <= 1; s += 2) {
      circle(cx + s * 4.5f * u - 1.2f * u, cy - s * 4.5f * u - 1.2f * u, 1.5f * u, C("#f5f5f4"));
      circle(cx + s * 4.5f * u + 1.2f * u, cy - s * 4.5f * u + 1.2f * u, 1.5f * u, C("#f5f5f4"));
    }
  } else if (k == "sinew") {
    ring(cx, cy, r * 0.9f, r * 0.14f, C("#fca5a5"));
    quadCurve({cx - 5 * u, cy}, {cx - 2.5f * u, cy - 5 * u}, {cx, cy}, 1.3f * u, C("#fecaca"));
    quadCurve({cx, cy}, {cx + 2.5f * u, cy + 5 * u}, {cx + 5 * u, cy}, 1.3f * u, C("#fecaca"));
  } else if (k == "coat") {
    ring(cx, cy, r * 0.9f, r * 0.14f, C("#a8a29e"));
    poly({{cx - 2 * u, cy - 5 * u}, {cx + 2 * u, cy - 5 * u}, {cx + 5 * u, cy - 2 * u}, {cx + 4 * u, cy + 5 * u}, {cx - 4 * u, cy + 5 * u}, {cx - 5 * u, cy - 2 * u}}, C("#d6d3d1"));
  } else if (k == "armour") {
    ring(cx, cy, r * 0.9f, r * 0.14f, C("#b45309"));
    poly({{cx - 4.5f * u, cy - 4.5f * u}, {cx + 4.5f * u, cy - 4.5f * u}, {cx + 4 * u, cy + 1 * u}, {cx, cy + 5.5f * u}, {cx - 4 * u, cy + 1 * u}}, C("#92400e"));
    line(cx, cy - 4 * u, cx, cy + 4.5f * u, 0.8f * u, C("#fbbf24"));
  } else {
    const char* hex = k == "deer" ? "#b45309" : k == "boar" ? "#78716c" : k == "goral" ? "#d6d3d1" : k == "wolf" ? "#9ca3af" : k == "bear" ? "#44403c" : k == "leopard" ? "#eab308" : "#a8a29e";
    ring(cx, cy, r * 0.9f, r * 0.14f, C(hex));
    ellipse(cx, cy + 1.8f * u, 3 * u, 2.4f * u, C(hex));
    const float T[4][2] = {{-3.4f, -1.6f}, {-1.2f, -3.8f}, {1.2f, -3.8f}, {3.4f, -1.6f}};
    for (auto& t : T) circle(cx + t[0] * u, cy + t[1] * u, 1.2f * u, C(hex));
  }
}

enum Style { ST_AMBER, ST_STONE, ST_EMERALD, ST_RED, ST_OFF, ST_MENU, ST_ACTIVE, ST_GHOST, ST_GHOSTON };

bool button(const Rect& r, const std::string& label, int style, float size = 13, const char* key = nullptr, FontId f = SERIF) {
  bool hot = !g_block && style != ST_OFF && hovered(r);
  uint32_t bg, fg, bd = 0;
  switch (style) {
    case ST_AMBER: bg = C(hot ? "#f59e0b" : "#d97706"); fg = C("#0c0a09"); break;
    case ST_EMERALD: bg = C(hot ? "#10b981" : "#059669"); fg = C("#ffffff"); break;
    case ST_RED: bg = C(hot ? "#991b1b" : "#7f1d1d", 0.75f); fg = C("#fecaca"); bd = C("#b91c1c"); break;
    case ST_OFF: bg = C("#292524"); fg = C("#78716c"); break;
    case ST_MENU: bg = C(hot ? "#d97706" : "#b45309", 0.92f); fg = C("#f5f5f4"); bd = C("#f59e0b", 0.7f); break;
    case ST_ACTIVE: bg = C("#d97706"); fg = C("#0c0a09"); bd = C("#fbbf24"); break;
    case ST_GHOST: bg = C("#ffffff", hot ? 0.12f : 0); fg = C("#f5f5f4"); break;
    case ST_GHOSTON: bg = C("#ffffff", hot ? 0.12f : 0); fg = C("#fbbf24"); bd = C("#fbbf24", 0.8f); break;
    default: bg = C(hot ? "#44403c" : "#292524"); fg = C("#e7e5e4"); bd = C("#44403c"); break;
  }
  if ((bg >> 24) & 255) rect(r, bg, std::min(10.0f, r.h * 0.3f));
  if (bd) border(r, bd, 1, std::min(10.0f, r.h * 0.3f));
  float kw = 0;
  if (key) kw = textWidth(key, size * 0.62f, SANS) + 8;
  float tw = textWidth(label, size, f);
  float x = r.x + (r.w - tw - (key ? kw + 5 : 0)) / 2;
  text(label, x, r.y + r.h / 2, size, fg, f);
  if (key) {
    Rect kr{x + tw + 5, r.y + r.h / 2 - size * 0.5f, kw, size};
    bool dark = style == ST_AMBER || style == ST_ACTIVE;
    if (style == ST_GHOST || style == ST_GHOSTON) dark = false;
    border(kr, dark ? C("#1c1917", 0.45f) : C("#57534e"), 1, 3);
    text(key, kr.x + kr.w / 2, kr.y + kr.h / 2, size * 0.62f, dark ? C("#1c1917") : C("#fcd34d", 0.9f), SANS, 1);
  }
  if (style == ST_OFF || g_block) return false;
  if (clicked(r)) {
    audio::crispClick();
    return true;
  }
  return false;
}

void card(const Rect& r, const char* bd = "#292524", const char* bg = "#0c0a09", float a = 0.8f) {
  rect(r, C(bg, a), 12);
  border(r, C(bd), 1, 12);
}

std::string costText(const Cost& c, const char* sep = ", ") {
  std::string s;
  for (auto& e : c) {
    if (!s.empty()) s += sep;
    s += std::to_string(e.second) + " " + itemLabel(e.first);
  }
  return s;
}

float pill(float x, float y, const std::string& s, const char* bd, const char* fg, float size = 11) {
  float w = textWidth(s, size, SANS) + 10;
  Rect r{x, y, w, size + 7};
  border(r, C(bd), 1, 4);
  text(s, x + 5, y + r.h / 2, size, C(fg), SANS);
  return w;
}

float costPills(float x, float y, float maxW, const Cost& c, float size = 11) {
  float cx = x, cy = y;
  for (auto& e : c) {
    std::string s = std::to_string(invCount(e.first)) + "/" + std::to_string(e.second) + " " + itemLabel(e.first);
    float w = textWidth(s, size, SANS) + 10;
    if (cx + w > x + maxW && cx > x) { cx = x; cy += size + 11; }
    bool ok = invCount(e.first) >= e.second;
    cx += pill(cx, cy, s, ok ? "#047857" : "#991b1b", ok ? "#6ee7b7" : "#fca5a5", size) + 4;
  }
  return cy + size + 7 - y;
}

struct Clock { std::string time, phase; };
Clock formatClock(float hours) {
  int total = (int)std::floor(hours * 60);
  int h = (total / 60) % 24, m = total % 60;
  const char* ph = hours >= 4.5f && hours < 7 ? "Dawn" : hours < 11.5f && hours >= 7 ? "Morning" : hours < 15.5f && hours >= 11.5f ? "Midday" : hours < 18 && hours >= 15.5f ? "Afternoon" : hours < 21 && hours >= 18 ? "Dusk" : "Night";
  char b[16];
  std::snprintf(b, sizeof b, "%02d:%02d", h, m);
  return {b, ph};
}

const Quest* activeQuest() {
  for (auto& q : A.quests)
    if (!q.completed) return &q;
  return nullptr;
}

// ---------------------------------------------------------------- map texture
void buildMap() {
  if (g_mapTex.id) return;
  const int N = 256;
  std::vector<uint8_t> px((size_t)N * N * 4);
  float step = MAP_HALF * 2 / N;
  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) {
      float x = -MAP_HALF + (i + 0.5f) * step, z = -MAP_HALF + (j + 0.5f) * step;
      float h = world::getTerrainHeight(x, z);
      float hx = world::getTerrainHeight(x + step, z) - world::getTerrainHeight(x - step, z);
      float hz = world::getTerrainHeight(x, z + step) - world::getTerrainHeight(x, z - step);
      float ny = 1 / std::sqrt(1 + (hx / (2 * step)) * (hx / (2 * step)) + (hz / (2 * step)) * (hz / (2 * step)));
      Color col = world::terrainColor(x, z, h, ny);
      if (world::riverMask(x, z) > 0.5f || world::lakeAt(x, z) >= 0) col = Color::hex("#4f9cc4");
      float shade = clampf(1 + (hz - hx) * 0.025f, 0.7f, 1.3f);
      uint32_t c = (col * shade).packSrgb();
      uint8_t* d = &px[((size_t)j * N + i) * 4];
      d[0] = c & 255;
      d[1] = (c >> 8) & 255;
      d[2] = (c >> 16) & 255;
      d[3] = 255;
    }
  g_mapTex = uploadTextureRGBA(px.data(), N, N, false, false, false, false);
  glActiveTexture(GL_TEXTURE0);
}

// ---------------------------------------------------------------- HUD
bool clipSeg(float R, float& x0, float& y0, float& x1, float& y1) {
  float dx = x1 - x0, dy = y1 - y0;
  float a = dx * dx + dy * dy, b = 2 * (x0 * dx + y0 * dy), c = x0 * x0 + y0 * y0 - R * R;
  if (a < 1e-9f) return c <= 0;
  float disc = b * b - 4 * a * c;
  if (disc <= 0) return false;
  float sq = std::sqrt(disc), t0 = (-b - sq) / (2 * a), t1 = (-b + sq) / (2 * a);
  t0 = std::max(t0, 0.0f);
  t1 = std::min(t1, 1.0f);
  if (t0 >= t1) return false;
  float ax = x0 + dx * t0, ay = y0 + dy * t0;
  x1 = x0 + dx * t1;
  y1 = y0 + dy * t1;
  x0 = ax;
  y0 = ay;
  return true;
}

void drawRadar(float X, float Y, float size) {
  const float RANGE = 70;
  float R = size / 2, cx = X + R, cy = Y + R;
  const Vec3& p = E.player.pos;
  float yaw = E.player.yaw, s = R / RANGE;
  float fx = -std::sin(yaw), fz = -std::cos(yaw), rx = std::cos(yaw), rz = -std::sin(yaw);
  auto tx = [&](float x, float z) { float dx = x - p.x, dz = z - p.z; return Vec2{(dx * rx + dz * rz) * s, -(dx * fx + dz * fz) * s}; };
  for (float rr : {0.33f, 0.66f}) ring(cx, cy, R * rr, 1, C("#ffffff", 0.08f));
  auto seg = [&](Vec2 a, Vec2 b, float w, uint32_t col) {
    float x0 = a.x, y0 = a.y, x1 = b.x, y1 = b.y;
    if (!clipSeg(R - w * 0.5f, x0, y0, x1, y1)) return;
    line(cx + x0, cy + y0, cx + x1, cy + y1, w, col);
    circle(cx + x1, cy + y1, w * 0.5f, col);
  };
  float lim = RANGE * 1.6f;
  uint32_t trail = C("#c4a068", 0.85f);
  for (auto& r : world::ROADS) {
    if (r.maxX < p.x - lim || r.minX > p.x + lim || r.maxZ < p.z - lim || r.minZ > p.z + lim) continue;
    for (size_t i = 0; i + 1 < r.pts.size(); i++) {
      if (std::fabs(r.pts[i][0] - p.x) > lim || std::fabs(r.pts[i][1] - p.z) > lim) continue;
      seg(tx(r.pts[i][0], r.pts[i][1]), tx(r.pts[i + 1][0], r.pts[i + 1][1]), 2.5f, trail);
    }
  }
  for (auto& r : world::ROUTES) {
    if (r.maxX < p.x - lim || r.minX > p.x + lim || r.maxZ < p.z - lim || r.minZ > p.z + lim) continue;
    for (size_t i = 0; i + 1 < r.pts.size(); i++) {
      if (std::fabs(r.pts[i][0] - p.x) > lim || std::fabs(r.pts[i][1] - p.z) > lim) continue;
      seg(tx(r.pts[i][0], r.pts[i][1]), tx(r.pts[i + 1][0], r.pts[i + 1][1]), 1.75f, trail);
    }
  }
  const auto& rv = world::RIVER;
  uint32_t water = C("#46a0d2", 0.9f);
  for (size_t i = 0; i + 1 < rv.pts.size(); i++) {
    if (std::fabs(rv.pts[i][0] - p.x) > lim + 40 || std::fabs(rv.pts[i][1] - p.z) > lim + 40) continue;
    float w = std::max(2.0f, (i < rv.hw.size() ? rv.hw[i] : 7) * 2 * s);
    seg(tx(rv.pts[i][0], rv.pts[i][1]), tx(rv.pts[i + 1][0], rv.pts[i + 1][1]), w, water);
  }
  for (auto& L : world::LAKES) {
    Vec2 c = tx(L.x, L.z);
    float d = std::sqrt(c.x * c.x + c.y * c.y);
    if (d - L.r * s < R) circle(cx + c.x, cy + c.y, std::min(L.r * s, R - d + L.r * s), water);
  }
  for (auto& b : A.buildings) {
    if (!b.built) continue;
    Vec2 c = tx(b.pos[0], b.pos[2]);
    if (c.x * c.x + c.y * c.y < (R - 4) * (R - 4)) rect({cx + c.x - 3, cy + c.y - 3, 6, 6}, C("#e7d3ac"));
  }
  for (auto& st : A.structures) {
    if (KINDS[st.kind].group == KG_DECOR) continue;
    Vec2 c = tx(st.x, st.z);
    if (c.x * c.x + c.y * c.y < (R - 4) * (R - 4)) rect({cx + c.x - 2.5f, cy + c.y - 2.5f, 5, 5}, C("#d6c3a1", 0.9f));
  }
  for (auto& m : g_markers) {
    float d = hypot2(m.x - p.x, m.z - p.z);
    if (d > RANGE * 1.05f) continue;
    Vec2 c = tx(m.x, m.z);
    if (c.x * c.x + c.y * c.y > (R - 3) * (R - 3)) continue;
    bool big = m.icon == MI_RECRUIT || m.icon == MI_HARVEST || m.icon == MI_CLUE || m.icon == MI_DANGER;
    if (big) glyph(m.icon, cx + c.x, cy + c.y, 5.5f);
    else circle(cx + c.x, cy + c.y, m.icon == MI_ANIMAL ? 2.25f : 2.75f, C(ICON_HEX[m.icon]));
  }
  if (E.questTarget.valid) {
    const auto& q = E.questTarget;
    float d = hypot2(q.x - p.x, q.z - p.z);
    Vec2 c = tx(q.x, q.z);
    if (d > RANGE) { float k = RANGE * 0.9f / d; c.x *= k; c.y *= k; }
    glyph(MI_QUEST, cx + c.x, cy + c.y, 6);
  }
  uint32_t pc = C("#fef3c7");
  tri({cx, cy - 5.5f}, {cx + 4, cy + 4.5f}, {cx, cy + 2}, pc);
  tri({cx, cy - 5.5f}, {cx, cy + 2}, {cx - 4, cy + 4.5f}, pc);
  float nb = wrapAngle(-yaw);
  text("N", cx - std::sin(nb) * (R - 7), cy - std::cos(nb) * (R - 7), 11, C("#f87171"), SANS, 1);
  ring(cx, cy, R, 1.5f, C("#ffffff", 0.35f));
}

void drawCompass(float W) {
  float cw = std::min(520.0f, W * 0.6f), h = 38, x = (W - cw) / 2, y = 70;
  Rect r{x, y, cw, h};
  for (int k = 0; k <= 24; k++) {
    float tx = x + cw * k / 24.0f, edge = 1 - std::fabs(k / 12.0f - 1);
    line(tx, y + h - (k % 6 == 0 ? 7 : 4), tx, y + h, 1, C("#ffffff", 0.35f * edge));
  }
  pushClip(r);
  const float half = PI / 2;
  const char* names[4] = {"N", "E", "S", "W"};
  float yaws[4] = {0, -PI / 2, PI, PI / 2};
  for (int i = 0; i < 4; i++) {
    float b = wrapAngle(yaws[i] - E.player.yaw);
    if (std::fabs(b) > half) continue;
    text(names[i], x + cw * (0.5f - b / half * 0.5f), y + 29, 11, i == 0 ? C("#f87171") : C("#e7e5e4"), SANS, 1);
  }
  const Vec3& p = E.player.pos;
  int slot = 0;
  auto show = [&](float bearing, MarkerIcon icon, float dist) {
    if (slot >= 14) return;
    slot++;
    if (std::fabs(bearing) > half) return;
    float sx = x + cw * (0.5f - bearing / half * 0.5f);
    glyph(icon, sx, y + 12, 9, icon == MI_QUEST ? 1 : 0.9f);
    if (dist < 1000) text(std::to_string((int)std::lround(dist)) + "m", sx, y + 27, 10, C("#fde68a"), SANS, 1, 1);
  };
  if (E.questTarget.valid) show(relativeBearing(E.questTarget.x, E.questTarget.z), MI_QUEST, hypot2(E.questTarget.x - p.x, E.questTarget.z - p.z));
  MarkerIcon ri[4] = {MI_WOOD, MI_STONE, MI_FIBER, MI_HERB};
  for (int t = 0; t < 4; t++) {
    const ResNode* n = resources::nearestOfType((ResType)t, p.x, p.z);
    if (n) show(relativeBearing(n->x, n->z), ri[t], hypot2(n->x - p.x, n->z - p.z));
  }
  for (auto& m : g_markers) {
    if (m.icon != MI_RECRUIT && m.icon != MI_CLUE && m.icon != MI_HARVEST && m.icon != MI_DANGER) continue;
    float d = hypot2(m.x - p.x, m.z - p.z);
    if (m.icon == MI_DANGER && d > 45) continue;
    if (m.icon == MI_HARVEST && d > 90) continue;
    show(relativeBearing(m.x, m.z), m.icon, d);
  }
  popClip();
  line(x + cw / 2, y, x + cw / 2, y + h, 1, C("#fcd34d", 0.6f));
}

void drawWorldIcons(float W, float H) {
  const Vec3& p = E.player.pos;
  const Mat4& vp = render::cam.viewProj;
  float t = E.clock;
  auto put = [&](float x, float y, float z, MarkerIcon icon) {
    float range = ICON_RANGE[icon];
    if (range <= 0) return;
    float d = hypot2(x - p.x, z - p.z);
    if (d > range || (icon != MI_QUEST && d < 2.2f)) return;
    float yy = y + std::sin(t * 2.2f + x * 0.7f + z * 0.3f) * 0.12f;
    Vec4 c = vp.transform4({x, yy, z}, 1);
    if (c.w <= 0.1f) return;
    float sx = (c.x / c.w * 0.5f + 0.5f) * W, sy = (0.5f - c.y / c.w * 0.5f) * H;
    if (sx < -40 || sy < -40 || sx > W + 40 || sy > H + 40) return;
    float alpha = std::min(1.0f, (range - d) / (range * 0.3f)) * (icon == MI_QUEST && d < 8 ? d / 8 : 1);
    float size = (icon == MI_QUEST ? 40 : 34) * clampf(16 / std::max(d, 1.0f), 0.55f, 1.15f);
    glyph(icon, sx, sy, size * 0.44f, alpha);
  };
  for (auto& m : g_markers) put(m.x, m.y, m.z, m.icon);
  if (E.questTarget.valid) put(E.questTarget.x, world::getTerrainHeight(E.questTarget.x, E.questTarget.z) + 3.2f, E.questTarget.z, MI_QUEST);
}

void drawCutscene(float W, float H) {
  auto& cs = E.cutscene;
  if (cs.dur <= 0) return;
  float u = cs.t / cs.dur;
  float bar = std::min(1.0f, std::min(cs.t * 2, (cs.dur - cs.t) * 2));
  rect({0, 0, W, bar * 0.11f * H}, C("#000000"));
  rect({0, H - bar * 0.11f * H, W, bar * 0.11f * H}, C("#000000"));
  float op = clampf(std::min((u - 0.12f) * 5, (0.92f - u) * 6), 0, 1);
  if (op > 0) textShadowed(cs.name, W / 2, H * 0.86f - 16, 30, C("#ffffff", op), SERIF, 1);
}

void drawHud(float W, float H) {
  double t = now();
  for (auto& e : E.events) {
    if (e.kind == HudEvent::Popup) g_popups.push_back({e.text, e.color == 0xffffffff ? C("#fde68a") : e.color, t});
    else if (e.kind == HudEvent::Hit) {
      g_hitT = t;
      g_hitCol = e.kill ? C("#f87171") : C("#fde68a");
      if (e.headshot) g_popups.push_back({"Headshot!", C("#fca5a5"), t});
    } else if (e.kind == HudEvent::Knock) g_knockT = t;
    else if (e.kind == HudEvent::Hurt) {
      g_knockT = t;
      g_popups.push_back({"-" + std::to_string((int)std::lround(e.amount)), C("#f87171"), t});
    }
  }
  E.events.clear();
  g_popups.erase(std::remove_if(g_popups.begin(), g_popups.end(), [&](const Popup& p) { return t - p.t0 > 2.2; }), g_popups.end());
  while (g_popups.size() > 5) g_popups.erase(g_popups.begin());

  collectMarkers(g_markers);
  drawWorldIcons(W, H);
  float cx = W / 2, cy = H / 2;

  float vig = std::max(0.0f, std::max(0.75f - (float)(t - g_knockT) / 0.9f, E.player.hp < 35 ? 0.3f + 0.15f * std::sin((float)t * 1000 / 180) : 0.0f));
  if (vig > 0.01f) radialVignette(C("#b91c1c", 0.65f * vig), 0.45f);

  drawCompass(W);
  drawRadar(18, H - 18 - 160, 160);

  float sc = E.bow.aiming ? 0.8f : 1;
  ring(cx, cy, 15 * sc, 2.5f * sc, C("#ffffff", 0.18f));
  if (E.bow.draw > 0.005f) ring(cx, cy, 15 * sc, 2.5f * sc, E.bow.draw >= 1 ? C("#34d399") : C("#fbbf24"), 0, TAU * std::min(1.0f, E.bow.draw) * 0.9999f);
  circle(cx, cy, 1.8f, C("#ffffff", 0.9f));
  float ho = std::max(0.0f, 1 - (float)(t - g_hitT) / 0.35f);
  if (ho > 0) {
    uint32_t hc = withAlpha(g_hitCol, ho);
    line(cx - 8, cy - 8, cx + 8, cy + 8, 3, hc);
    line(cx - 8, cy + 8, cx + 8, cy - 8, 3, hc);
  }
  auto& w = E.world;
  std::string prompt = !w.placing.empty() ? (w.previewValid ? "Click to place · Q/E rotate · Esc cancel" : w.previewReason) : E.nearbyPrompt;
  if (!prompt.empty()) textShadowed(prompt, cx, cy - 46, 15, !w.placing.empty() && !w.previewValid ? C("#fca5a5") : C("#fcd34d"), SERIF, 1);
  const char* stealth = E.player.crouching ? "Crouching · quiet" : E.player.sprinting ? "Sprinting · loud" : "";
  if (*stealth) textShadowed(stealth, cx, cy + 30, 11, C("#e7e5e4", 0.8f), SERIF, 1);

  float py = cy - 10;
  for (auto& p : g_popups) {
    float k = (float)(t - p.t0) / 2.2f;
    float op = k < 0.12f ? k / 0.12f : k < 0.7f ? 1 : 1 - (k - 0.7f) / 0.3f;
    float dy = k < 0.12f ? 6 * (1 - k / 0.12f) : k < 0.7f ? 0 : -26 * (k - 0.7f) / 0.3f;
    float s = k < 0.12f ? 0.9f + 0.15f * k / 0.12f : 1;
    textShadowed(p.text, cx + 34, py + dy, 15 * s, withAlpha(p.color, op), SERIF, 0);
    py += 22;
  }

  float hpf = E.player.hp / std::max(1.0f, E.player.maxHp);
  float tw = 160 * std::min(1.9f, 0.75f + 0.25f * E.player.maxHp / 100);
  std::string lbl = std::to_string((int)std::ceil(E.player.hp)) + "/" + std::to_string((int)E.player.maxHp);
  float lw = textWidth(lbl, 10, SANS);
  float bw = 8 + 12 + 6 + tw + 6 + lw + 8;
  float bo = hpf < 0.995f || t - g_knockT < 4 ? 1 : 0.35f;
  Rect hb{cx - bw / 2, H - 14 - 22, bw, 22};
  heart(hb.x + 14, hb.y + 11, 11, C("#fca5a5", bo));
  Rect track{hb.x + 26, hb.y + 7, tw, 8};
  rect(track, C("#292524", bo), 4);
  rect({track.x, track.y, tw * clampf(hpf, 0, 1), 8}, C(hpf > 0.6f ? "#22c55e" : hpf > 0.3f ? "#f59e0b" : "#ef4444", bo), 4);
  text(lbl, track.x + tw + 6, hb.y + 11, 10, C("#d6d3d1", bo), SANS);
}

// ---------------------------------------------------------------- top bar
float chip(float x, float y, float w, float h) {
  Rect r{x, y, w, h};
  (void)r;
  return w;
}

void drawTopBar(float W) {
  float x = 8, y = 8, h = 46, bw = W - 16;
  Rect bar{x, y, bw, h};
  (void)bar;
  float ix = x + 6, iy = y + 5, ih = h - 10;

  Clock ck = formatClock(E.time);
  std::string sl = std::string(SEASON_NAMES[season.index]) + " " + std::to_string(season.dayOfSeason) + "/" + std::to_string(DAYS_PER_SEASON);
  float cw = std::max(textWidth(sl, 10, SERIF) + 22, textWidth(ck.time, 13, SANS) + 22) + 8;
  chip(ix, iy, cw, ih);
  seasonGlyph(season.index, ix + 11, iy + 11, 5);
  text(sl, ix + 20, iy + 11, 10, C("#fbbf24", 0.9f));
  text(ck.time, ix + 10, iy + 26, 13, C("#e7e5e4"), SANS);
  ix += cw + 6;

  StageInfo st = computeStage();
  int cap = app::capacity();
  if (W >= 900) {
    std::string a = std::to_string(A.score) + " renown";
    std::string b = (W >= 1180 ? st.name + " · " : std::string()) + "Pop " + std::to_string(A.population) + "/" + std::to_string(cap);
    float w2 = std::max(textWidth(a, 10, SERIF), textWidth(b, 12, SERIF)) + 18;
    chip(ix, iy, w2, ih);
    text(a, ix + 9, iy + 11, 10, C("#a8a29e"));
    text(b, ix + 9, iy + 26, 12, C("#fbbf24"));
    ix += w2 + 6;
  }

  int arrows = invCount("arrow");
  std::string an = std::to_string(arrows);
  float aw = 26 + textWidth(an, 13, SANS) + 6 + 58;
  chip(ix, iy, aw, ih);
  itemIcon("arrow", ix + 13, iy + ih / 2, 8);
  text(an, ix + 25, iy + ih / 2, 13, arrows == 0 ? C("#f87171") : C("#7dd3fc"), SANS);
  Rect cr{ix + aw - 62, iy + 7, 56, ih - 14};
  if (button(cr, "+10", ST_GHOSTON, 11, "R", SANS)) app::craftArrows(false);
  ix += aw + 6;

  if (W >= 1100) {
    int food = invCount("food");
    int need = dailyNeed(A.population, season.index == 3);
    const char* ids[5] = {"food", "wood", "stone", "fiber", "herb"};
    float rw = 8;
    for (auto id : ids) rw += 18 + textWidth(std::to_string(invCount(id)), 12, SANS) + 8;
    chip(ix, iy, rw, ih);
    float rx = ix + 8;
    for (auto id : ids) {
      itemIcon(id, rx + 8, iy + ih / 2, 8);
      std::string n = std::to_string(invCount(id));
      text(n, rx + 19, iy + ih / 2, 12, std::string(id) == "food" && food < need ? C("#fca5a5") : C("#e7e5e4"), SANS);
      rx += 18 + textWidth(n, 12, SANS) + 8;
    }
    ix += rw + 6;
  }

  struct TabDef { Tab t; const char* label; const char* key; };
  TabDef tabs[4] = {{Tab::Inventory, "Pouch", "Tab"}, {Tab::Quests, "Journal", "J"}, {Tab::Buildings, "Build", "B"}, {Tab::Map, "Map", "M"}};
  bool labels = W >= 1280;
  float rx = x + bw - 6;
  float mw = labels ? 92 : 66;
  rx -= mw;
  if (button({rx, iy + 3, mw, ih - 6}, labels ? "Menu" : "II", ST_GHOST, 12, "Esc")) A.menuOpen = true;
  rx -= 8;
  for (int i = 3; i >= 0; i--) {
    std::string lab = labels ? tabs[i].label : std::string(tabs[i].label).substr(0, 1);
    float tw = textWidth(lab, 12) + textWidth(tabs[i].key, 7.5f, SANS) + 8 + 5 + 18;
    rx -= tw;
    if (button({rx, iy + 3, tw, ih - 6}, lab, A.tab == tabs[i].t ? ST_GHOSTON : ST_GHOST, 12, tabs[i].key)) actions::openTab(tabs[i].t);
    rx -= 4;
  }

  float room = rx - ix - 12;
  if (room > 180) {
    const Quest* q = activeQuest();
    std::string t1 = q ? q->title : "All chapters complete";
    std::string t2 = q ? q->objective + (q->target > 1 ? " (" + std::to_string(q->progress) + "/" + std::to_string(q->target) + ")" : "") : "Free exploration";
    float ow = std::min(room, std::min(380.0f, std::max(textWidth(t1, 10), textWidth(t2, 12)) + 30));
    float ox = ix + (room - ow) / 2 + 6;
    Rect orr{ox, iy, ow, ih};
    (void)orr;
    text(ellipsize(t1, 10, ow - 24), ox + ow / 2, iy + 11, 10, C("#fbbf24"), SERIF, 1);
    text(ellipsize(t2, 12, ow - 24), ox + ow / 2, iy + 26, 12, C("#e7e5e4"), SERIF, 1);
  }
}

void drawNotification(float W) {
  if (A.notification.empty()) return;
  double age = now() - A.notifyAt;
  if (age > 5.5) return;
  float op = clampf((float)std::min(age / 0.25, (5.5 - age) / 0.5), 0, 1);
  float maxW = std::min(448.0f, W * 0.4f);
  auto lines = wrap(A.notification, 14, maxW);
  float y = 66 + 8;
  for (auto& l : lines) {
    textShadowed(l, W - 20, y + 8, 14, C("#fcd34d", op), SERIF, 2);
    y += 19;
  }
}

// ---------------------------------------------------------------- look editor
const int DECO_ORDER[7] = {DK_POTS, DK_LANTERN, DK_FIREWOOD, DK_BENCH, DK_BARRELS, DK_FENCE, DK_CLOTH};

float lookEditor(float x, float y, float w, StructureKind kind, Look& look, bool compact, bool& changed) {
  const auto& opts = KINDS[kind].options;
  if (opts.empty()) { text("No style options.", x, y + 8, 11, C("#78716c"), SANS); return 18; }
  float yy = y;
  for (int slot = 0; slot < LO_COUNT; slot++) {
    if (std::find(opts.begin(), opts.end(), (LookOpt)slot) == opts.end()) continue;
    std::string lab = (compact ? "[" + std::to_string(slot + 1) + "] " : std::string()) + LOOK_OPT_NAMES[slot];
    text(lab, x, yy + 10, 11, C("#a8a29e"), SANS);
    float cx = x + 108, rowH = 20;
    for (int c = 0; c < LOOK_COUNTS[slot]; c++) {
      const char* nm = LOOK_LABELS[slot][c];
      bool on;
      if (slot == LO_DECO) on = look.has((DecoKey)DECO_ORDER[c]);
      else on = (&look.roof)[slot] == c;
      float bw = textWidth(nm, 10, SANS) + 14;
      if (cx + bw > x + w) { cx = x + 108; yy += 24; }
      Rect r{cx, yy, bw, rowH};
      bool hot = !g_block && hovered(r);
      rect(r, on ? C("#d97706") : C(hot ? "#292524" : "#1c1917"), 6);
      border(r, on ? C("#fbbf24") : C("#44403c"), 1, 6);
      text(nm, cx + bw / 2, yy + rowH / 2, 10, on ? C("#0c0a09") : C("#d6d3d1"), SANS, 1);
      if (!g_block && clicked(r)) {
        audio::crispClick();
        if (slot == LO_DECO) look.deco ^= 1 << DECO_ORDER[c];
        else (&look.roof)[slot] = c;
        changed = true;
      }
      cx += bw + 4;
    }
    yy += compact ? 24 : 26;
  }
  return yy - y;
}

// ---------------------------------------------------------------- placement panels
void drawPlacement(float W, float H) {
  auto& w = E.world;
  if (w.placing.empty() || E.cutscene.dur > 0) return;
  bool structure = w.placeKind != SK_COUNT && w.hasPlaceLook;
  const Building* b = nullptr;
  if (!structure)
    for (auto& x : A.buildings)
      if (x.id == w.placing) b = &x;
  if (!structure && !b) return;
  bool moving = structure ? w.placing.rfind("m:", 0) == 0 : b->built;
  std::string title = std::string(moving ? "Moving: " : "Placing: ") + (structure ? KINDS[w.placeKind].name : b->name);
  bool bridge = b && b->type == "bridge";
  std::string hint = structure ? "Rotate [Q]/[E] · Place [Click] · Restyle [1-6] · Cancel [Esc]" : bridge ? "Walk along the river bank — the bridge snaps across the water." : "Rotate [Q]/[E] · Place [Left click] · Cancel [Esc / Right click]";
  std::string cost;
  if (structure && !moving) cost = "Cost: " + costText(KINDS[w.placeKind].cost);
  if (b && !b->built) cost = "Cost: " + costText(b->cost);
  float pw = std::min(W - 24, 720.0f);
  float styleH = 0;
  bool style = structure && A.showStyle && !KINDS[w.placeKind].options.empty();
  if (style) styleH = (float)KINDS[w.placeKind].options.size() * 24 + 30;
  float ph = 70 + (cost.empty() ? 0 : 16) + styleH;
  Rect r{(W - pw) / 2, H - 24 - ph, pw, ph};
  rect(r, C("#022c22", 0.9f), 16);
  border(r, C("#10b981"), 2, 16);
  text(title, r.x + 18, r.y + 20, 14, C("#6ee7b7"));
  text(hint, r.x + 18, r.y + 40, 11, C("#d6d3d1"), SANS);
  if (!cost.empty()) text(cost, r.x + 18, r.y + 57, 11, C("#fcd34d"), SANS);
  float bx = r.x + pw - 14, by = r.y + 12, bh = 28;
  auto btnR = [&](float bw) { bx -= bw; Rect q{bx, by, bw, bh}; bx -= 6; return q; };
  if (button(btnR(structure ? 64 : 74), structure ? "Done" : "Cancel", ST_STONE, 11)) actions::cancelPlacement();
  if (structure && !KINDS[w.placeKind].options.empty())
    if (button(btnR(60), "Style", A.showStyle ? ST_ACTIVE : ST_STONE, 11)) A.showStyle = !A.showStyle;
  if (!bridge) {
    if (button(btnR(34), "E", ST_STONE, 11, nullptr, SANS)) w.previewRot += PI / 8;
    if (button(btnR(34), "Q", ST_STONE, 11, nullptr, SANS)) w.previewRot -= PI / 8;
  }
  if (style) {
    bool ch = false;
    lookEditor(r.x + 18, r.y + ph - styleH + 4, pw - 36, w.placeKind, w.placeLook, true, ch);
    if (ch && w.placing.rfind("s:", 0) == 0) A.drafts[w.placeKind] = w.placeLook;
  }
}

// ---------------------------------------------------------------- modal helpers
void backdrop(float W, float H, float a) { rect({0, 0, W, H}, C("#000000", a)); }

Rect modal(float W, float H, float mw, float mh, const char* bd = "#44403c") {
  mw = std::min(mw, W - 32);
  mh = std::min(mh, H - 32);
  Rect r{(W - mw) / 2, (H - mh) / 2, mw, mh};
  rect(r, C("#1c1917", 0.96f), 16);
  border(r, C(bd), 2, 16);
  return r;
}

void drawDialogue(float W, float H) {
  if (A.dialogue < 0 || A.dialogue >= (int)A.specialists.size()) return;
  Specialist& s = A.specialists[A.dialogue];
  Cost cost = app::recruitCost(s.id);
  backdrop(W, H, 0.4f);
  float mw = std::min(512.0f, W - 32), inner = mw - 48;
  float dh = (float)wrap("\"" + s.dialogue + "\"", 13, inner - 32).size() * 18 + 32;
  float bh = (float)wrap("Benefit: " + s.bonus, 12, inner - 24).size() * 16 + 24;
  float mh = 24 + 54 + 12 + dh + 14 + bh + 14 + (s.recruited || cost.empty() ? 0 : 34) + 50 + 10;
  Rect r = modal(W, H, mw, mh, "#b45309");
  float x = r.x + 24, y = r.y + 24;
  text(s.name, x, y + 12, 20, C("#fbbf24"));
  text(s.title, x, y + 36, 11, C("#34d399"), SANS);
  if (button({r.x + r.w - 46, y - 4, 28, 28}, "x", ST_STONE, 13, nullptr, SANS)) { A.dialogue = -1; return; }
  y += 54;
  line(x, y, x + inner, y, 1, C("#292524"));
  y += 12;
  Rect dr{x, y, inner, dh};
  rect(dr, C("#0c0a09", 0.5f), 12);
  border(dr, C("#292524"), 1, 12);
  paragraph("\"" + s.dialogue + "\"", x + 16, y + 16, inner - 32, 13, C("#e7e5e4"), SANS, 1.38f);
  y += dh + 14;
  Rect br{x, y, inner, bh};
  rect(br, C("#451a03", 0.4f), 12);
  border(br, C("#78350f", 0.5f), 1, 12);
  paragraph("Benefit: " + s.bonus, x + 12, y + 12, inner - 24, 12, C("#fde68a", 0.9f), SANS, 1.33f);
  y += bh + 14;
  if (!s.recruited && !cost.empty()) {
    costPills(x, y, inner, cost, 12);
    y += 34;
  }
  float by = r.y + r.h - 24 - 36;
  bool ok = app::has(cost);
  if (!s.recruited) {
    float w2 = 210;
    if (button({r.x + r.w - 24 - w2, by, w2, 36}, ok ? "Recruit to the village" : "Recruit (need more)", ok ? ST_AMBER : ST_STONE, 12)) app::recruit(A.dialogue);
    if (A.dialogue >= 0 && button({r.x + r.w - 24 - w2 - 12 - 110, by, 110, 36}, "Farewell", ST_STONE, 12)) A.dialogue = -1;
  } else if (button({r.x + r.w - 24 - 110, by, 110, 36}, "Farewell", ST_STONE, 12)) A.dialogue = -1;
}

// ---------------------------------------------------------------- tab contents
float tabInventory(float x, float y, float w) {
  float y0 = y;
  if (g_invSection == 1) {
  Rect fr{x, y, w, 52};
  card(fr, "#292524", "#0c0a09", 0.7f);
  text("Fletching", x + 14, y + 26, 13, C("#d6d3d1"));
  float bw2 = std::min(220.0f, (w - 140) / 2);
  if (button({x + w - 12 - bw2, y + 10, bw2, 32}, "+25 arrows (2 timber, 1 fibre)", ST_EMERALD, bw2 < 200 ? 9.5f : 11)) app::craftArrows(true);
  if (button({x + w - 12 - bw2 * 2 - 8, y + 10, bw2, 32}, "+10 arrows (1 timber)", ST_AMBER, 11)) app::craftArrows(false);
  y += 52 + 14;
  }

  if (g_invSection == 2) {
  bool market = false;
  for (auto& s : A.structures) if (s.kind == SK_MARKET_STALL) market = true;
  int cols = w > 520 ? 2 : 1;
  int rows = (9 + cols - 1) / cols;
  float th = 40 + rows * 30 + 6;
  card({x, y, w, th}, "#292524", "#0c0a09", 0.7f);
  text("Market trades", x + 14, y + 20, 13, C("#e7e5e4"));
  if (!market) text("Build a Market Stall to trade", x + w - 14, y + 20, 11, C("#fdba74"), SANS, 2);
  float cw = (w - 28 - (cols - 1) * 6) / cols;
  for (int i = 0; i < 9; i++) {
    const Trade& t = TRADES[i];
    Rect r{x + 14 + (i % cols) * (cw + 6), y + 38 + (i / cols) * 30, cw, 26};
    bool ok = market && app::has(t.give);
    bool hot = ok && !g_block && hovered(r);
    rect(r, ok ? C(hot ? "#292524" : "#1c1917") : C("#0c0a09", 0), 5);
    border(r, ok ? C("#b45309") : C("#292524"), 1, 5);
    std::string g = costText(t.give, " + "), r2 = costText(t.get, " + ");
    float gx = text(g + "  ->  ", r.x + 8, r.y + 13, 11, ok ? C("#e7e5e4") : C("#78716c"), SANS);
    text(r2, r.x + 8 + gx, r.y + 13, 11, ok ? C("#6ee7b7") : C("#78716c"), SANS);
    if (ok && !g_block && clicked(r)) app::trade(t);
  }
  y += th + 14;
  }

  if (g_invSection == 1) {
  float gh = 36 + 2 * 62;
  card({x, y, w, gh}, "#292524", "#0c0a09", 0.7f);
  text("Craft gear", x + 14, y + 20, 13, C("#e7e5e4"));
  for (int i = 0; i < 2; i++) {
    const GearDef& g = GEAR[i];
    float gy = y + 38 + i * 62;
    bool owned = invCount(g.id) > 0, ok = !owned && app::has(g.cost);
    float tx = text(g.name, x + 14, gy + 8, 13, C("#fbbf24"));
    if (owned) text(" · worn", x + 14 + tx, gy + 8, 11, C("#6ee7b7"), SANS);
    text(ellipsize(g.blurb, 11, w - 140, SANS), x + 14, gy + 26, 11, C("#a8a29e"), SANS);
    if (!owned) text("Needs " + costText(g.cost, " + "), x + 14, gy + 43, 11, C("#d6d3d1"), SANS);
    if (button({x + w - 14 - 84, gy + 6, 84, 32}, owned ? "Owned" : "Craft", ok ? ST_AMBER : ST_OFF, 12)) app::craftGear(g);
  }
  y += gh + 14;
  }

  if (g_invSection == 0) {
  int ic = w > 560 ? 3 : 2;
  float iw = (w - (ic - 1) * 12) / ic, ih = 104;
  for (int i = 0; i < ITEM_COUNT; i++) {
    std::string id = ITEM_ORDER[i];
    int n = invCount(id);
    Rect r{x + (i % ic) * (iw + 12), y + (i / ic) * (ih + 12), iw, ih};
    float a = n ? 1 : 0.6f;
    rect(r, C("#0c0a09", 0.8f * a), 12);
    border(r, n ? C("#44403c") : C("#292524"), 1, 12);
    itemIcon(id, r.x + 24, r.y + 24, 13);
    std::string cnt = "x" + std::to_string(n);
    float cw2 = textWidth(cnt, 11, SANS) + 16;
    Rect pr{r.x + r.w - 12 - cw2, r.y + 12, cw2, 20};
    rect(pr, C("#451a03", 0.6f), 10);
    border(pr, C("#b45309", 0.5f), 1, 10);
    text(cnt, pr.x + cw2 / 2, pr.y + 10, 11, C("#fbbf24", a), SANS, 1);
    text(itemName(id), r.x + 12, r.y + 52, 12, C("#e7e5e4", a));
    auto lines = wrap(itemDesc(id), 10, iw - 24, SANS);
    for (size_t k = 0; k < lines.size() && k < 3; k++) text(lines[k], r.x + 12, r.y + 70 + k * 13, 10, C("#a8a29e", a), SANS);
  }
  y += ((ITEM_COUNT + ic - 1) / ic) * (ih + 12);
  }
  return y - y0;
}

float tabQuests(float x, float y, float w) {
  float y0 = y;
  const Quest* aq = activeQuest();
  for (auto& q : A.quests) {
    bool act = &q == aq;
    auto dl = wrap(q.desc, 12, w - 32, SANS);
    float h = 16 + 34 + dl.size() * 16 + 8 + 18 + (act && E.questTarget.valid ? 18 : 0) + 12;
    Rect r{x, y, w, h};
    float a = q.completed || act ? 1 : 0.7f;
    rect(r, q.completed ? C("#022c22", 0.3f) : act ? C("#451a03", 0.3f) : C("#0c0a09", 0.8f), 12);
    border(r, q.completed ? C("#065f46", 0.5f) : act ? C("#b45309", 0.6f) : C("#292524"), 1, 12);
    text(q.title, x + 16, y + 24, 14, C("#fbbf24", a));
    text("From: " + q.giver, x + 16, y + 42, 10, C("#a8a29e", a), SANS);
    std::string badge = q.completed ? "DONE" : std::to_string(q.progress) + "/" + std::to_string(q.target);
    float bw = textWidth(badge, 10, SANS) + 20;
    Rect br{x + w - 16 - bw, y + 14, bw, 22};
    rect(br, q.completed ? C("#064e3b", 0.6f) : C("#78350f", 0.6f), 11);
    text(badge, br.x + bw / 2, br.y + 11, 10, q.completed ? C("#6ee7b7") : C("#fcd34d"), SANS, 1);
    float yy = y + 58;
    for (auto& l : dl) { text(l, x + 16, yy, 12, C("#d6d3d1", a), SANS); yy += 16; }
    yy += 6;
    float ow = text("Objective: ", x + 16, yy, 11, C("#d6d3d1", a), SANS);
    text(q.objective, x + 16 + ow, yy, 11, C("#a8a29e", a), SANS);
    if (act && E.questTarget.valid) {
      yy += 18;
      text("Next: " + E.questTarget.label + " — follow the gold marker on your compass.", x + 16, yy, 11, C("#fcd34d"), SANS);
    }
    y += h + 12;
  }
  return y - y0;
}

float stageCard(float x, float y, float w) {
  StageInfo st = computeStage();
  int cap = app::capacity();
  float h = 36;
  std::string nx = st.next.empty() ? "Your settlement is fully established. Keep shaping it." : "";
  for (size_t i = 0; i < st.next.size(); i++) nx += (i ? " · " : "Next stage needs: ") + st.next[i];
  auto nl = wrap(nx, 10.5f, w - 28, SANS);
  h += nl.size() * 14;
  std::string extra = cap == 0 ? "Build a home so settlers can move in." : A.population >= cap ? "All beds are taken. Build more homes to welcome new settlers." : "";
  if (!extra.empty()) h += 14;
  h += 8;
  card({x, y, w, h}, "#78350f");
  text(st.name, x + 14, y + 18, 14, C("#fbbf24"));
  text("Residents " + std::to_string(A.population) + "/" + std::to_string(cap) + " · Appeal " + std::to_string(villageAppeal()), x + w - 14, y + 18, 11, C("#d6d3d1"), SANS, 2);
  float yy = y + 38;
  for (auto& l : nl) { text(l, x + 14, yy, 10.5f, st.next.empty() ? C("#6ee7b7") : C("#a8a29e"), SANS); yy += 14; }
  if (!extra.empty()) text(extra, x + 14, yy, 10.5f, C("#7dd3fc"), SANS);
  return h;
}

float catalog(float x, float y, float w, KindGroup group) {
  float y0 = y;
  int stage = computeStage().level;
  const char* STAGE_SHORT[3] = {"Clearing", "Growing Village", "Established"};
  for (int k = 0; k < SK_COUNT; k++) {
    const KindInfo& info = KINDS[k];
    if (info.group != group) continue;
    bool locked = stage < UNLOCK[k];
    bool afford = app::has(info.cost);
    bool open = g_openKind == k;
    Look look = A.drafts.count((StructureKind)k) ? A.drafts[(StructureKind)k] : app::defaultLookFor((StructureKind)k);
    float edH = 0;
    if (open) edH = (float)info.options.size() * 26 + 10 + (info.options.size() && std::find(info.options.begin(), info.options.end(), LO_DECO) != info.options.end() ? 24 : 0);
    auto bl = wrap(info.blurb, 11, w - 160, SANS);
    float h = 14 + 20 + bl.size() * 15 + 30 + edH + 6;
    card({x, y, w, h});
    text(info.name, x + 14, y + 22, 14, C("#fbbf24"));
    float yy = y + 40;
    for (auto& l : bl) { text(l, x + 14, yy, 11, C("#a8a29e"), SANS); yy += 15; }
    float px = x + 14;
    for (auto& e : info.cost) {
      bool ok = invCount(e.first) >= e.second;
      px += pill(px, yy - 2, std::to_string(invCount(e.first)) + "/" + std::to_string(e.second) + " " + itemLabel(e.first), ok ? "#047857" : "#991b1b", ok ? "#6ee7b7" : "#fca5a5", 10) + 4;
    }
    if (info.capacity > 0) px += pill(px, yy - 2, "Beds " + std::to_string(info.capacity), "#075985", "#7dd3fc", 10) + 4;
    if (info.appeal > 0) pill(px, yy - 2, "Appeal +" + std::to_string(info.appeal), "#92400e", "#fcd34d", 10);
    float bx = x + w - 14 - 130;
    if (button({bx, y + 12, 130, 30}, locked ? std::string("Needs ") + STAGE_SHORT[UNLOCK[k]] : "Place", locked ? ST_OFF : afford ? ST_AMBER : ST_STONE, locked ? 10 : 12)) app::startStructure((StructureKind)k);
    if (!info.options.empty() && !locked && button({bx, y + 46, 130, 24}, open ? "Hide style" : "Style", ST_STONE, 10)) g_openKind = open ? -1 : k;
    if (open) {
      bool ch = false;
      lookEditor(x + 14, y + h - edH - 2, w - 28, (StructureKind)k, look, false, ch);
      if (ch) A.drafts[(StructureKind)k] = look;
    }
    y += h + 8;
  }
  return y - y0;
}

float peoplePanel(float x, float y, float w) {
  float y0 = y;
  int pop = A.population, cap = app::capacity();
  int used = 0;
  for (int j = 0; j < JOB_COUNT; j++) used += A.jobs[j];
  int idle = pop - used;
  int food = invCount("food"), fcap = foodCap(), need = dailyNeed(pop, season.index == 3);
  float def = villageDefense();
  card({x, y, w, 96});
  float cw = (w - 28) / 2;
  auto stat = [&](int i, const std::string& a, const std::string& b, uint32_t col) {
    float sx = x + 14 + (i % 2) * cw, sy = y + 12 + (i / 2) * 42;
    text(a, sx, sy + 6, 10, C("#a8a29e"), SANS);
    text(b, sx, sy + 24, 12, col, SANS);
  };
  stat(0, "Residents", std::to_string(pop) + "/" + std::to_string(cap) + " · " + std::to_string(idle) + " idle", C("#7dd3fc"));
  stat(1, "Food store", std::to_string(food) + "/" + std::to_string(fcap), food < need ? C("#fca5a5") : C("#fcd34d"));
  std::string lasts = need > 0 ? " · lasts " + std::to_string(food / need) + (food / need == 1 ? " day" : " days") : "";
  stat(2, "Eaten each dawn", std::to_string(need) + " food" + lasts, C("#e7e5e4"));
  char db[64];
  std::snprintf(db, sizeof db, "%g/%g%s", std::round(def * 10) / 10, SAFE_DEFENSE, def >= SAFE_DEFENSE ? " · safe from raids" : "");
  stat(3, "Night defence", db, def >= SAFE_DEFENSE ? C("#6ee7b7") : C("#fdba74"));
  y += 96 + 8;
  if (A.hungerDays > 0) {
    std::string m = "The village is hungry: workers produce half and no settlers arrive. Two hungry days in a row and families start leaving. Hunt, assign hunters or trade for food.";
    auto l = wrap(m, 11, w - 24, SANS);
    float h = l.size() * 15 + 16;
    rect({x, y, w, h}, C("#450a0a", 0.4f), 8);
    border({x, y, w, h}, C("#7f1d1d"), 1, 8);
    paragraph(m, x + 12, y + 8, w - 24, 11, C("#fca5a5"), SANS, 1.36f);
    y += h + 8;
  }
  for (int j = 0; j < JOB_COUNT; j++) {
    card({x, y, w, 52});
    text(JOBS[j].name, x + 14, y + 18, 13, C("#fbbf24"));
    text(std::string(JOBS[j].blurb) + " each", x + 14, y + 36, 10, C("#a8a29e"), SANS);
    float bx = x + w - 14 - 28;
    if (button({bx, y + 12, 28, 28}, "+", idle > 0 ? ST_AMBER : ST_OFF, 14, nullptr, SANS)) app::setJob(j, A.jobs[j] + 1);
    text(std::to_string(A.jobs[j]), bx - 18, y + 26, 13, C("#e7e5e4"), SANS, 1);
    if (button({bx - 64, y + 12, 28, 28}, "-", A.jobs[j] > 0 ? ST_STONE : ST_OFF, 14, nullptr, SANS)) app::setJob(j, A.jobs[j] - 1);
    y += 52 + 8;
  }
  y += paragraph("Lantern posts, fences, the Hunter's Lodge and the Watchtower raise night defence. Wolves raid the food store on cold nights when defence is low.", x, y + 4, w, 10, C("#78716c"), SANS, 1.36f) + 6;
  return y - y0;
}

float myVillage(float x, float y, float w) {
  float y0 = y;
  if (A.structures.empty()) {
    text("Nothing built yet. Start with a Small Cottage from the Homes tab.", x + w / 2, y + 30, 12, C("#a8a29e"), SANS, 1);
    return 60;
  }
  int decor = 0;
  int removeUid = -1, moveUid = -1;
  for (auto& s : A.structures) {
    const KindInfo& info = KINDS[s.kind];
    if (info.group == KG_DECOR) { decor++; continue; }
    bool open = g_openUid == s.uid;
    float edH = open ? (float)info.options.size() * 26 + 34 : 0;
    float h = 58 + edH;
    card({x, y, w, h});
    text(info.name, x + 14, y + 20, 14, C("#fbbf24"));
    std::string sub = std::to_string((int)std::lround(hypot2(s.x, s.z))) + " m from the hearth" + (info.capacity ? " · " + std::to_string(info.capacity) + " beds" : "");
    text(sub, x + 14, y + 40, 10, C("#a8a29e"), SANS);
    float bx = x + w - 14;
    bx -= 70;
    if (button({bx, y + 15, 70, 28}, "Remove", ST_RED, 10)) removeUid = s.uid;
    bx -= 66;
    if (button({bx, y + 15, 60, 28}, "Move", ST_STONE, 10)) moveUid = s.uid;
    if (!info.options.empty()) {
      bx -= 76;
      if (button({bx, y + 15, 70, 28}, "Restyle", ST_STONE, 10)) g_openUid = open ? -1 : s.uid;
    }
    if (open) {
      Look l = s.look;
      bool ch = false;
      lookEditor(x + 14, y + 60, w - 28, s.kind, l, false, ch);
      if (ch) { app::restyleStructure(s.uid, l); break; }
    }
    y += h + 8;
  }
  if (decor > 0) {
    card({x, y, w, 48});
    text(std::to_string(decor) + " paths, fences and decorations", x + 14, y + 24, 12, C("#d6d3d1"), SANS);
    if (button({x + w - 14 - 110, y + 10, 110, 28}, "Remove last", ST_STONE, 10)) {
      for (auto it = A.structures.rbegin(); it != A.structures.rend(); ++it)
        if (KINDS[it->kind].group == KG_DECOR) { removeUid = it->uid; break; }
    }
    y += 56;
  }
  if (moveUid >= 0) app::moveStructure(moveUid);
  if (removeUid >= 0) app::removeStructure(removeUid);
  return y - y0;
}

float workshops(float x, float y, float w) {
  float y0 = y;
  int cols = w > 560 ? 2 : 1;
  float cw = (w - (cols - 1) * 12) / cols;
  std::vector<Building*> list;
  for (auto& b : A.buildings) if (b.type != "campfire") list.push_back(&b);
  float rowH = 0;
  for (size_t i = 0; i < list.size(); i++) {
    Building& b = *list[i];
    const Production* prod = production(b.id);
    float h = 150;
    if ((int)i % cols == 0 && i) { y += rowH + 12; rowH = 0; }
    rowH = std::max(rowH, h);
    float cx = x + (i % cols) * (cw + 12);
    card({cx, y, cw, h});
    text(ellipsize(b.name, 13, cw - 110), cx + 14, y + 20, 13, C("#fbbf24"));
    std::string badge = b.built ? "Level " + std::to_string(b.level) : "Not built";
    float bw = textWidth(badge, 10, SANS) + 16;
    rect({cx + cw - 14 - bw, y + 10, bw, 20}, b.built ? C("#064e3b") : C("#292524"), 10);
    text(badge, cx + cw - 14 - bw / 2, y + 20, 10, b.built ? C("#6ee7b7") : C("#a8a29e"), SANS, 1);
    float yy = y + 40;
    auto dl = wrap(b.desc, 11, cw - 28, SANS);
    for (size_t k = 0; k < dl.size() && k < 2; k++) { text(dl[k], cx + 14, yy, 11, C("#a8a29e"), SANS); yy += 15; }
    if (prod) { text(std::string("Produces ") + prod->label + (b.level > 1 ? " x" + std::to_string(b.level) : ""), cx + 14, yy, 11, C("#6ee7b7"), SANS); yy += 15; }
    if (!b.built) costPills(cx + 14, yy, cw - 28, b.cost, 10);
    bool canAfford = app::has(b.cost);
    float by = y + h - 12 - 32;
    bool up = b.built && prod && b.level < b.maxLevel;
    float bw1 = up ? (cw - 28 - 8) / 2 : cw - 28;
    if (button({cx + 14, by, bw1, 32}, b.built ? "Move" : "Place blueprint", b.built ? ST_STONE : canAfford ? ST_AMBER : ST_OFF, 12)) app::placeWorkshop(b.id);
    if (!b.built && !canAfford && !g_block && clicked({cx + 14, by, bw1, 32})) app::placeWorkshop(b.id);
    if (up) {
      Cost uc = app::upgradeCost(b);
      if (button({cx + 14 + bw1 + 8, by, bw1, 32}, "Upgrade", app::has(uc) ? ST_EMERALD : ST_OFF, 12)) app::upgrade(b);
      text(ellipsize(costText(uc), 9, bw1, SANS), cx + 14 + bw1 + 8 + bw1 / 2, by - 8, 10, C("#a8a29e"), SANS, 1);
    }
  }
  y += rowH;
  return y - y0;
}

float tabBuildings(float x, float y, float w) {
  float y0 = y;
  y += stageCard(x, y, w) + 14;
  switch (A.buildSection) {
    case 1: y += catalog(x, y, w, KG_HOME); break;
    case 2: y += catalog(x, y, w, KG_VILLAGE); break;
    case 3: y += catalog(x, y, w, KG_DECOR); break;
    case 0: y += workshops(x, y, w); break;
    case 4: y += peoplePanel(x, y, w); break;
    default: y += myVillage(x, y, w); break;
  }
  return y - y0;
}

float tabMap(float x, float y, float w) {
  float y0 = y;
  buildMap();
  float s = std::min(w, 460.0f);
  float mx = x + (w - s) / 2, my = y;
  image(g_mapTex.id, {mx, my, s, s});
  border({mx, my, s, s}, C("#78350f", 0.6f), 2, 10);
  auto P = [&](float wx, float wz) { return Vec2{mx + (0.5f + wx / MAP_HALF * 0.5f) * s, my + (0.5f + wz / MAP_HALF * 0.5f) * s}; };
  std::vector<Marker> ms;
  collectMarkers(ms);
  for (auto& m : ms)
    if (m.icon <= MI_HERB) { Vec2 p = P(m.x, m.z); circle(p.x, p.y, 3, C("#000000", 0.4f)); circle(p.x, p.y, 2.2f, C(ICON_HEX[m.icon])); }
  for (auto& b : A.buildings)
    if (b.built) { Vec2 p = P(b.pos[0], b.pos[2]); rect({p.x - 5, p.y - 5, 10, 10}, C("#1c1917")); rect({p.x - 4, p.y - 4, 8, 8}, C("#fef3c7")); }
  std::string tip;
  for (auto& m : ms)
    if (m.icon == MI_RECRUIT || m.icon == MI_CLUE || m.icon == MI_HARVEST) {
      Vec2 p = P(m.x, m.z);
      glyph(m.icon, p.x, p.y, 9);
      if (hypot2(mouse().x - p.x, mouse().y - p.y) < 9) tip = m.label;
    }
  if (E.questTarget.valid) {
    Vec2 p = P(E.questTarget.x, E.questTarget.z);
    glyph(MI_QUEST, p.x, p.y, 11 + std::sin(E.clock * 4) * 1.2f);
    if (hypot2(mouse().x - p.x, mouse().y - p.y) < 11) tip = E.questTarget.label;
  }
  Vec2 pp = P(E.player.pos.x, E.player.pos.z);
  float a = -E.player.yaw;
  auto rot = [&](float lx, float ly) { return Vec2{pp.x + lx * std::cos(a) - ly * std::sin(a), pp.y + lx * std::sin(a) + ly * std::cos(a)}; };
  uint32_t ink = C("#1c1917"), fill = C("#fff7d6");
  tri(rot(0, -10), rot(7.5f, 8.5f), rot(0, 4), ink);
  tri(rot(0, -10), rot(0, 4), rot(-7.5f, 8.5f), ink);
  tri(rot(0, -8), rot(6, 7), rot(0, 3), fill);
  tri(rot(0, -8), rot(0, 3), rot(-6, 7), fill);
  textShadowed("N", mx + s / 2, my + 12, 12, C("#fca5a5"), SANS, 1);
  if (!tip.empty()) {
    float tw = textWidth(tip, 11, SANS) + 14;
    Rect tr{mouse().x + 12, mouse().y - 24, tw, 20};
    rect(tr, C("#0c0a09", 0.9f), 5);
    text(tip, tr.x + 7, tr.y + 10, 11, C("#fde68a"), SANS);
  }
  y += s + 14;
  const char* lab[4] = {"Timber", "Slate", "Nettle fibre", "Herbs"};
  float lw = 0;
  for (int i = 0; i < 4; i++) lw += textWidth(lab[i], 11, SANS) + 26;
  lw += 3 * (textWidth("Specialist", 11, SANS) + 30);
  float lx = x + std::max(0.0f, (w - lw) / 2);
  for (int i = 0; i < 4; i++) {
    circle(lx + 5, y + 8, 4, C(ICON_HEX[i]));
    lx += 14 + text(lab[i], lx + 14, y + 8, 11, C("#d6d3d1"), SANS) + 12;
  }
  MarkerIcon gi[3] = {MI_RECRUIT, MI_CLUE, MI_QUEST};
  const char* gl[3] = {"Specialist", "Landmark", "Objective"};
  for (int i = 0; i < 3; i++) {
    glyph(gi[i], lx + 7, y + 8, 7);
    lx += 18 + text(gl[i], lx + 18, y + 8, 11, C("#d6d3d1"), SANS) + 12;
  }
  y += 24;
  if (E.questTarget.valid) { text("Objective: " + E.questTarget.label, x + w / 2, y + 8, 12, C("#fcd34d"), SERIF, 1); y += 22; }
  return y - y0;
}

void drawTabs(float W, float H) {
  if (A.tab == Tab::None) return;
  static int lastSec = -99;
  int secKey = A.tab == Tab::Inventory ? g_invSection : A.tab == Tab::Buildings ? 10 + A.buildSection : -1;
  if (A.tab != g_lastTab || secKey != lastSec) { g_scroll = 0; g_lastTab = A.tab; lastSec = secKey; }
  backdrop(W, H, 0.6f);
  float mw = std::min(940.0f, W - 32), mh = std::min(H * 0.9f, H - 32);
  Rect r = modal(W, H, mw, mh);
  Rect head{r.x + 2, r.y + 2, r.w - 4, 52};
  rect(head, C("#0c0a09", 0.9f), 14);
  struct TD { Tab t; const char* l; const char* k; };
  TD tabs[4] = {{Tab::Inventory, "Pouch", "Tab"}, {Tab::Quests, "Journal", "J"}, {Tab::Buildings, "Build", "B"}, {Tab::Map, "Map", "M"}};
  float tw = (r.w - 16 - 8 * 4 - 44) / 4;
  for (int i = 0; i < 4; i++)
    if (button({r.x + 8 + i * (tw + 8), r.y + 10, tw, 36}, tabs[i].l, A.tab == tabs[i].t ? ST_ACTIVE : ST_STONE, 13, tabs[i].k)) A.tab = tabs[i].t;
  if (button({r.x + r.w - 8 - 44, r.y + 10, 44, 36}, "x", ST_STONE, 14, nullptr, SANS)) { A.tab = Tab::None; return; }
  line(r.x, r.y + 56, r.x + r.w, r.y + 56, 1, C("#292524"));
  Rect body{r.x + 2, r.y + 57, r.w - 4, r.h - 59};

  bool side = A.tab == Tab::Inventory || A.tab == Tab::Buildings;
  std::string sectionTitle;
  if (side) {
    float sw = 190;
    Rect sb{body.x, body.y, sw, body.h};
    rect(sb, C("#0c0a09", 0.55f), 0);
    line(sb.x + sw, sb.y, sb.x + sw, sb.y + sb.h, 1, C("#292524"));
    float y = sb.y + 16;
    auto item = [&](const char* label, const char* sub, bool on) {
      Rect b{sb.x + 10, y, sw - 20, 46};
      bool hot = !g_block && hovered(b);
      rect(b, on ? C("#d97706", 0.18f) : C("#ffffff", hot ? 0.06f : 0), 8);
      if (on) rect({b.x, b.y + 8, 3, b.h - 16}, C("#f59e0b"), 1.5f);
      text(label, b.x + 14, b.y + 17, 13, on ? C("#fbbf24") : C("#e7e5e4"));
      text(sub, b.x + 14, b.y + 33, 10, C("#a8a29e"), SANS);
      y += 52;
      if (!g_block && clicked(b)) { audio::crispClick(); return true; }
      return false;
    };
    auto group = [&](const char* g) { text(g, sb.x + 22, y + 8, 9.5f, C("#78716c"), SANS); y += 20; };
    if (A.tab == Tab::Inventory) {
      const char* L[3] = {"Items", "Crafting", "Market"};
      const char* S[3] = {"Everything you carry", "Arrows and gear", "Trade trophies"};
      for (int i = 0; i < 3; i++) if (item(L[i], S[i], g_invSection == i)) g_invSection = i;
      sectionTitle = L[g_invSection];
    } else {
      struct SD { int id; const char* l; const char* s; };
      SD build[4] = {{1, "Homes", "Beds for settlers"}, {2, "Village", "Market, hall, stores"}, {3, "Paths & Decor", "Fences, lamps, flags"}, {0, "Workshops", "Production buildings"}};
      SD manage[2] = {{4, "Villagers", "Jobs and food"}, {5, "My Village", "Move, restyle, remove"}};
      group("CONSTRUCT");
      for (auto& d : build) if (item(d.l, d.s, A.buildSection == d.id)) A.buildSection = d.id;
      y += 6;
      group("MANAGE");
      for (auto& d : manage) if (item(d.l, d.s, A.buildSection == d.id)) A.buildSection = d.id;
      for (auto& d : build) if (d.id == A.buildSection) sectionTitle = d.l;
      for (auto& d : manage) if (d.id == A.buildSection) sectionTitle = d.l;
    }
    body = {body.x + sw + 1, body.y, body.w - sw - 1, body.h};
  }

  if (hovered(body)) g_scroll -= mouse().wheel * 60;
  float maxS = std::max(0.0f, g_contentH - (body.h - 32));
  g_scroll = clampf(g_scroll, 0, maxS);
  pushClip(body);
  float cx = body.x + 20, cy = body.y + 16 - g_scroll, cw = body.w - 40 - (maxS > 0 ? 8 : 0);
  float h = 0;
  if (!sectionTitle.empty()) {
    text(sectionTitle, cx, cy + 14, 20, C("#fbbf24"));
    line(cx, cy + 34, cx + cw, cy + 34, 1, C("#292524"));
    cy += 48;
    h += 48;
  }
  bool wasBlock = g_block;
  switch (A.tab) {
    case Tab::Inventory: h += tabInventory(cx, cy, cw); break;
    case Tab::Quests: h += tabQuests(cx, cy, cw); break;
    case Tab::Buildings: h += tabBuildings(cx, cy, cw); break;
    case Tab::Map: h += tabMap(cx, cy, cw); break;
    default: break;
  }
  g_block = wasBlock;
  popClip();
  g_contentH = h;
  if (maxS > 0) {
    float th = std::max(30.0f, body.h * (body.h - 32) / std::max(1.0f, g_contentH));
    float ty = body.y + (body.h - th) * (g_scroll / maxS);
    rect({body.x + body.w - 8, ty + 4, 4, th - 8}, C("#78716c", 0.6f), 2);
  }
}

// ---------------------------------------------------------------- menus
const char* HOWTO[] = {
    "Move: WASD · Shift sprint (loud) · C crouch (quiet) · Space jump. Click the view to capture the mouse, Esc to release and pause.",
    "Bow: hold left mouse to draw — the ring fills, green means full power — release to shoot. Hold right mouse to aim. Headshots deal double damage.",
    "Finding things: the compass shows your gold objective plus the nearest timber, slate, nettle and herbs with distances. The radar (bottom left) shows everything within 70 m. Floating icons mark resources up close.",
    "Gather: walk up and press E. Each node gives several harvests and regrows later.",
    "Build: B opens blueprints. The preview turns green where you can build. Built workshops produce materials for you, and can be upgraded.",
    "Predators: wolves hunt in packs, black bears rear up and charge, leopards stalk and pounce. They have far more health than deer, so aim for the head. The red fang icon on your compass and radar marks them. Your health recovers when you stay out of fights, and fastest in the village.",
    "Shortcuts: Tab pouch · J journal · B build · M map · E gather / talk · R craft 10 arrows · F11 fullscreen · F3 performance stats · F2 screenshot.",
};

void drawHowTo(float W, float H) {
  backdrop(W, H, 0.9f);
  float mw = std::min(560.0f, W - 32), inner = mw - 48;
  float th = 0;
  for (auto s : HOWTO) th += wrap(s, 12, inner, SANS).size() * 17 + 8;
  Rect r = modal(W, H, mw, 24 + 40 + th + 12 + 44 + 24);
  text("HOW TO PLAY", r.x + 24, r.y + 40, 24, C("#fbbf24"));
  float y = r.y + 72;
  for (auto s : HOWTO) y += paragraph(s, r.x + 24, y, inner, 12, C("#d6d3d1"), SANS, 1.4f) + 8;
  if (button({r.x + 24, r.y + r.h - 24 - 40, inner, 40}, "Close", ST_AMBER, 14)) A.howToOpen = false;
}

bool g_options = false;
int g_titlePage = 0;
const void* g_drag = nullptr;
bool g_settingsDirty = false;

void menuBackdrop(float W, float H, float a) {
  rect({0, 0, W, H}, C("#000000", a));
  const int N = 40;
  for (int i = 0; i < N; i++) {
    float t = (float)i / N;
    rect({W * 0.7f * t, 0, W * 0.7f / N + 1, H}, C("#0c0a09", 0.6f * (1 - t * t)));
  }
}

bool menuItem(float x, float y, float w, const std::string& label, const std::string& sub = "", bool primary = false, bool enabled = true) {
  float h = sub.empty() ? 44 : 56;
  Rect r{x, y, w, h};
  bool hot = enabled && !g_block && hovered(r);
  if (hot) rect(r, C("#ffffff", 0.07f), 8);
  if (hot || primary) rect({r.x, r.y + 8, 3, h - 16}, C(primary ? "#f59e0b" : "#fbbf24", hot ? 1 : 0.7f), 1.5f);
  uint32_t col = !enabled ? C("#57534e") : primary ? C("#fbbf24") : hot ? C("#ffffff") : C("#d6d3d1");
  text(label, r.x + 18, r.y + (sub.empty() ? h / 2 : 21), 20, col, SERIF, 0, 1.5f);
  if (!sub.empty()) text(sub, r.x + 18, r.y + 41, 11, C("#a8a29e"), SANS, 0, 1);
  if (enabled && !g_block && clicked(r)) { audio::crispClick(); return true; }
  return false;
}

bool slider(float x, float y, float w, const char* label, float& v, float lo, float hi, const char* fmt, float mul = 1) {
  text(label, x, y + 10, 12, C("#d6d3d1"), SANS);
  char b[32];
  std::snprintf(b, sizeof b, fmt, v * mul);
  text(b, x + w, y + 10, 12, C("#fbbf24"), SANS, 2);
  Rect track{x, y + 24, w, 6};
  rect(track, C("#292524"), 3);
  float t = clampf((v - lo) / (hi - lo), 0, 1);
  rect({x, y + 24, w * t, 6}, C("#d97706"), 3);
  circle(x + w * t, y + 27, 8, C("#fbbf24"));
  Rect hit{x - 8, y + 14, w + 16, 26};
  const MouseState& m = mouse();
  if (!g_block && m.pressed && hovered(hit)) { g_drag = &v; consumeClick(); }
  if (!m.down && g_drag == &v) g_drag = nullptr;
  if (g_drag == &v) {
    float nv = lo + clampf((m.x - x) / w, 0, 1) * (hi - lo);
    if (nv != v) { v = nv; return true; }
  }
  return false;
}

bool toggle(float x, float y, float w, const char* label, bool on, const char* onTxt = "On", const char* offTxt = "Off") {
  text(label, x, y + 14, 12, C("#d6d3d1"), SANS);
  Rect r{x + w - 90, y + 2, 90, 26};
  return button(r, on ? onTxt : offTxt, on ? ST_ACTIVE : ST_STONE, 11, nullptr, SANS);
}

void drawOptions(float W, float H) {
  backdrop(W, H, 0.7f);
  Rect r = modal(W, H, 760, 640, "#b45309");
  text("OPTIONS", r.x + 32, r.y + 38, 24, C("#fbbf24"));
  Settings& st = g_settings;
  float colW = (r.w - 64 - 40) / 2, x1 = r.x + 32, x2 = x1 + colW + 40, y0 = r.y + 76;
  text("DISPLAY", x1, y0, 10, C("#78716c"), SANS);
  float y = y0 + 16;
  if (slider(x1, y, colW, st.fullMap ? "Draw distance (full map on)" : "Draw distance", st.drawDistance, 1.0f, 3.0f, "%.1fx")) g_settingsDirty = true;
  y += 50;
  if (slider(x1, y, colW, st.adaptiveQuality ? "Resolution scale (max, dynamic on)" : "Resolution scale", st.renderScale, 0.5f, 1.0f, "%.0f%%", 100)) g_settingsDirty = true;
  y += 50;
  if (slider(x1, y, colW, "Field of view", st.fov, 60, 100, "%.0f")) g_settingsDirty = true;
  y += 50;
  if (slider(x1, y, colW, "UI scale", st.uiScale, 0.8f, 1.4f, "%.2f")) g_settingsDirty = true;
  y += 54;
  if (toggle(x1, y, colW, "Fullscreen", platform::isFullscreen())) platform::toggleFullscreen();
  y += 36;
  if (toggle(x1, y, colW, "VSync", st.vsync)) { platform::setVsync(!st.vsync); g_settingsDirty = true; }
  y += 36;
  if (toggle(x1, y, colW, "Dynamic resolution", st.adaptiveQuality)) { st.adaptiveQuality = !st.adaptiveQuality; g_settingsDirty = true; }
  y += 36;
  if (toggle(x1, y, colW, "Dynamic detail (hold FPS)", st.adaptiveDetail)) { st.adaptiveDetail = !st.adaptiveDetail; if (!st.adaptiveDetail) st.detailScale = 1; g_settingsDirty = true; }
  y += 36;
  const int SH[3] = {1024, 1536, 2048};
  const char* SHN[3] = {"Low", "Medium", "High"};
  int si = st.shadowSize >= 2048 ? 2 : st.shadowSize >= 1536 ? 1 : 0;
  text("Shadow quality", x1, y + 14, 12, C("#d6d3d1"), SANS);
  if (button({x1 + colW - 90, y + 2, 90, 26}, SHN[si], ST_STONE, 11, nullptr, SANS)) { st.shadowSize = SH[(si + 1) % 3]; g_settingsDirty = true; }
  y += 36;
  {
    const int FL[8] = {30, 45, 60, 75, 90, 120, 144, 0};
    int fi = 2;
    for (int i = 0; i < 8; i++) if (FL[i] == st.fpsLimit) fi = i;
    text("FPS limit (lower = cooler GPU)", x1, y + 14, 12, C("#d6d3d1"), SANS);
    char lb[16];
    std::snprintf(lb, sizeof lb, FL[fi] ? "%d" : "Unlimited", FL[fi]);
    if (button({x1 + colW - 90, y + 2, 90, 26}, lb, ST_STONE, 11, nullptr, SANS)) { st.fpsLimit = FL[(fi + 1) % 8]; g_settingsDirty = true; }
  }
  y += 36;
  if (toggle(x1, y, colW, "Render full map (high-end GPU)", st.fullMap)) { st.fullMap = !st.fullMap; g_settingsDirty = true; }

  text("AUDIO & CONTROLS", x2, y0, 10, C("#78716c"), SANS);
  y = y0 + 16;
  if (slider(x2, y, colW, "Music volume", st.musicVolume, 0, 1, "%.0f%%", 100)) { audio::setMusicVolume(st.musicVolume); g_settingsDirty = true; }
  y += 50;
  if (slider(x2, y, colW, "Effects volume", st.sfxVolume, 0, 1, "%.0f%%", 100)) { audio::setSfxVolume(st.sfxVolume); g_settingsDirty = true; }
  y += 50;
  float sens = st.mouseSensitivity * 1000;
  if (slider(x2, y, colW, "Mouse sensitivity", sens, 0.5f, 6, "%.1f")) { st.mouseSensitivity = sens / 1000; g_settingsDirty = true; }
  y += 54;
  int mi = st.msaa >= 4 ? 3 : st.msaa >= 2 ? 2 : st.fxaa ? 1 : 0;
  text("Anti-aliasing (MSAA needs restart)", x2, y + 14, 12, C("#d6d3d1"), SANS);
  const char* MSN[4] = {"Off", "FXAA", "MSAA 2x", "MSAA 4x"};
  if (button({x2 + colW - 90, y + 2, 90, 26}, MSN[mi], ST_STONE, 11, nullptr, SANS)) {
    int nm = (mi + 1) % 4;
    st.fxaa = nm == 1;
    st.msaa = nm == 2 ? 2 : nm == 3 ? 4 : 0;
    g_settingsDirty = true;
  }
  y += 44;
  {
    float tb = st.triBudget / 1000.0f;
    if (slider(x2, y, colW, "Forest triangle budget (k, 0 = off)", tb, 0, 1500, "%.0fk")) { st.triBudget = (int)(std::round(tb / 50) * 50) * 1000; g_settingsDirty = true; }
  }
  y += 60;
  text("SAVE FILE", x2, y, 10, C("#78716c"), SANS);
  y += 12;
  paragraph(platform::saveDir() + "/save.json", x2, y, colW, 11, C("#a8a29e"), SANS, 1.35f);

  float by = r.y + r.h - 32 - 42;
  text("Settings are saved to config.ini next to the game.", r.x + 32, by + 21, 11, C("#78716c"), SANS);
  if (button({r.x + r.w - 32 - 160, by, 160, 42}, "Save & Close", ST_AMBER, 14)) {
    platform::saveSettings();
    g_settingsDirty = false;
    g_options = false;
  }
}

void drawMenu(float W, float H) {
  menuBackdrop(W, H, 0.45f);
  float x = std::max(48.0f, W * 0.08f), w = 360, y = H * 0.24f;
  text("PAUSED", x + 18, y, 40, C("#fbbf24"), SERIF, 0, 2);
  text(std::string(SEASON_NAMES[season.index]) + ", day " + std::to_string(season.dayOfSeason) + "  ·  " + formatClock(E.time).time + "  ·  " + std::to_string(A.score) + " renown", x + 18, y + 38, 12, C("#a8a29e"), SANS, 0, 1);
  y += 78;
  if (menuItem(x, y, w, "Resume", "", true)) A.menuOpen = false;
  y += 50;
  static double savedAt = -10;
  if (menuItem(x, y, w, "Save Game", now() - savedAt < 2.5 ? "Saved." : "Writes your village to disk")) {
    app::saveNow();
    savedAt = now();
    audio::confirmSelect();
  }
  y += 62;
  if (menuItem(x, y, w, "Options")) g_options = true;
  y += 50;
  if (menuItem(x, y, w, "How to Play")) A.howToOpen = true;
  y += 50;
  if (menuItem(x, y, w, std::string("Music: ") + (A.musicMuted ? "Off" : "On"))) A.musicMuted = !A.musicMuted;
  y += 50;
  if (menuItem(x, y, w, std::string("Sound: ") + (A.sfxMuted ? "Off" : "On"))) A.sfxMuted = audio::toggleSfxMute();
  y += 62;
  if (menuItem(x, y, w, "Quit to Title", "Saves first")) { app::quitToTitle(); g_titlePage = 0; }
  y += 62;
  if (menuItem(x, y, w, "Exit Game", "Saves and closes")) { app::saveNow(); platform::requestClose(); }
}

void drawTitle(float W, float H) {
  menuBackdrop(W, H, 0.35f);
  static std::string summary;
  static double summaryAt = -10;
  if (now() - summaryAt > 2) { summaryAt = now(); if (!app::readSaveSummary(summary)) summary.clear(); }
  bool hasSave = !summary.empty();
  float x = std::max(48.0f, W * 0.08f), w = 380, y = H * 0.2f;
  text("HIMALAYAN", x + 18, y, 54, C("#fbbf24"), SERIF, 0, 2.5f);
  text("WILDS", x + 18, y + 54, 54, C("#fef3c7"), SERIF, 0, 2.5f);
  y += 110;
  if (g_titlePage == 0) {
    paragraph("Hunt, gather, recruit four specialists and grow a struggling camp into a thriving Kath-Kuni village.", x + 18, y, w, 12, C("#d6d3d1"), SANS, 1.4f);
    y += 56;
    if (menuItem(x, y, w, "Continue", hasSave ? summary : "No saved village yet", hasSave, hasSave)) { audio::confirmSelect(); app::continueGame(); return; }
    y += 66;
    if (menuItem(x, y, w, "New Game", hasSave ? "Replaces your save once it autosaves" : "", !hasSave)) g_titlePage = 1;
    y += hasSave ? 66 : 54;
    if (menuItem(x, y, w, "Options")) g_options = true;
    y += 54;
    if (menuItem(x, y, w, "How to Play")) A.howToOpen = true;
    y += 54;
    if (menuItem(x, y, w, "Exit")) platform::requestClose();
    return;
  }
  text("CHOOSE A STARTING SEASON", x + 18, y + 4, 12, C("#a8a29e"), SANS, 0, 1);
  y += 24;
  float cw = std::min(170.0f, (W - x - 60) / 4 - 10);
  for (int i = 0; i < 4; i++) {
    Rect b{x + 18 + i * (cw + 10), y, cw, 190};
    bool on = A.startSeason == i, hot = hovered(b);
    rect(b, C("#0c0a09", on ? 0.75f : hot ? 0.6f : 0.45f), 12);
    border(b, on ? C("#f59e0b") : C("#57534e", 0.6f), on ? 2 : 1, 12);
    seasonGlyph(i, b.x + cw / 2, b.y + 44, 22);
    text(SEASON_NAMES[i], b.x + cw / 2, b.y + 92, 17, on ? C("#fbbf24") : C("#e7e5e4"), SERIF, 1);
    paragraph(SEASON_NOTES[i], b.x + 12, b.y + 116, cw - 24, 10.5f, C("#a8a29e"), SANS, 1.35f, 1);
    if (clicked(b)) { A.startSeason = i; audio::crispClick(); }
  }
  y += 214;
  if (button({x + 18, y, 200, 46}, "BEGIN", ST_AMBER, 16)) { g_titlePage = 0; app::newGame(); return; }
  if (button({x + 230, y, 120, 46}, "Back", ST_STONE, 14)) g_titlePage = 0;
}

void drawVictory(float W, float H) {
  backdrop(W, H, 0.6f);
  Rect r = modal(W, H, 520, 250, "#f59e0b");
  text("MOUNTAIN HAVEN RESTORED", r.x + r.w / 2, r.y + 56, 26, C("#fbbf24"), SERIF, 1);
  paragraph("All 8 chapters complete! Final renown: " + std::to_string(A.score) + ". Keep exploring the valley.", r.x + 32, r.y + 96, r.w - 64, 13, C("#e7e5e4"), SANS, 1.4f, 1);
  if (button({r.x + 32, r.y + r.h - 32 - 48, r.w - 64, 48}, "CONTINUE", ST_AMBER, 16)) A.status = Status::Playing;
}

std::string g_passGpu, g_passCpu;
void drawStats(float W) {
  char b[256];
  std::snprintf(b, sizeof b, "%.0f fps  %.1f ms  |  CPU %.1f ms  GPU %.1f ms  |  scale %.0f%%  detail %.0f%%  |  %d draws  %dk tris  |  RAM %.0f MB  %s %.0f MB", g_fs.fps, g_fs.ms, g_fs.cpuMs, g_fs.gpuMs, g_fs.renderScale * 100, forest::detailLevel() * 100, g_fs.drawCalls, g_fs.tris / 1000, g_fs.ramMB, g_fs.vramExact == 1 ? "VRAM" : g_fs.vramExact == 2 ? "VRAM (all apps)" : "VRAM (est.)", g_fs.gpuMB);
  char r[160];
  std::snprintf(r, sizeof r, "%s", (const char*)glGetString(GL_RENDERER));
  float w = textWidth(b, 11, SANS) + 16;
  rect({W - w - 8, 60, w, 22}, C("#000000", 0.6f), 6);
  text(b, W - w, 71, 11, C("#a7f3d0"), SANS);
  float rw = textWidth(r, 11, SANS) + 16;
  rect({W - rw - 8, 84, rw, 20}, C("#000000", 0.6f), 6);
  text(r, W - rw, 94, 11, C("#fde68a"), SANS);
  float y = 108;
  for (const std::string* t : {&g_passGpu, &g_passCpu}) {
    if (t->empty()) continue;
    float tw = textWidth(*t, 11, SANS) + 16;
    rect({W - tw - 8, y, tw, 20}, C("#000000", 0.6f), 6);
    text(*t, W - tw, y + 10, 11, C("#bae6fd"), SANS);
    y += 24;
  }
}

}  // namespace

namespace screens {

void init() {}

void toggleStats() { g_stats = !g_stats; }
bool statsVisible() { return g_stats; }
void setPassText(const std::string& g, const std::string& c) { g_passGpu = g; g_passCpu = c; }
bool optionsOpen() { return g_options; }
void openOptions(bool on) { g_options = on; }
void setTitlePage(int p) { g_titlePage = p; }
bool takeSettingsChanged() { bool d = g_settingsDirty; g_settingsDirty = false; return d; }
void setStats(const FrameStats& s) { g_fs = s; }

bool escape() {
  if (A.howToOpen) { A.howToOpen = false; return true; }
  if (g_options) { platform::saveSettings(); g_options = false; return true; }
  if (A.status == Status::Start && g_titlePage) { g_titlePage = 0; return true; }
  if (A.dialogue >= 0) { A.dialogue = -1; return true; }
  if (A.tab != Tab::None) { A.tab = Tab::None; return true; }
  if (A.menuOpen) { A.menuOpen = false; return true; }
  if (A.status == Status::Playing && !E.world.placing.empty()) { actions::cancelPlacement(); return true; }
  if (A.status == Status::Playing) { A.menuOpen = true; return true; }
  return false;
}

void draw(float, bool cursorFree) {
  float W = width(), H = height();
  g_block = !cursorFree;
  bool playing = A.status == Status::Playing;
  bool cut = E.cutscene.dur > 0;
  drawCutscene(W, H);
  bool modalOpen = A.tab != Tab::None || A.dialogue >= 0 || A.menuOpen || A.howToOpen || A.status != Status::Playing;
  setTextShadow(0.09f);
  if (playing && !cut && !modalOpen) drawHud(W, H);
  else E.events.clear();
  if (playing && !cut) {
    bool b = g_block;
    g_block = b || modalOpen;
    drawTopBar(W);
    if (!modalOpen) drawNotification(W);
    setTextShadow(0);
    drawPlacement(W, H);
    g_block = b;
  }
  setTextShadow(0);
  if (playing) {
    bool b = g_block;
    g_block = b || A.menuOpen || A.howToOpen || g_options;
    if (A.dialogue >= 0) drawDialogue(W, H);
    drawTabs(W, H);
    g_block = b;
    if (A.menuOpen) {
      g_block = b || A.howToOpen || g_options;
      drawMenu(W, H);
      g_block = b;
    }
  }
  if (A.status == Status::Start) {
    bool b = g_block;
    g_block = b || A.howToOpen || g_options;
    drawTitle(W, H);
    g_block = b;
  }
  if (A.status == Status::Victory) drawVictory(W, H);
  if (g_options && !A.howToOpen) drawOptions(W, H);
  if (A.howToOpen) drawHowTo(W, H);
  if (g_stats) drawStats(W);
}

void shutdown() { g_mapTex.destroy(); }

}  // namespace screens
