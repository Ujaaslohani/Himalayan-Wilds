#include "font.h"
#include "../platform/platform.h"
#include "stb_truetype.h"
#include <cmath>
#include <algorithm>
#include <memory>


namespace {
std::unique_ptr<FontFace> g_serif, g_sans;

std::unique_ptr<FontFace> load(const std::string& rel) {
  auto f = std::make_unique<FontFace>();
  if (!platform::readFile(platform::assetPath(rel), f->data)) return nullptr;
  if (!stbtt_InitFont(&f->info, f->data.data(), stbtt_GetFontOffsetForIndex(f->data.data(), 0))) return nullptr;
  stbtt_GetFontVMetrics(&f->info, &f->ascent, &f->descent, &f->gap);
  return f;
}

int decodeUtf8(const std::string& s, size_t& i) {
  unsigned char c = (unsigned char)s[i++];
  if (c < 0x80) return c;
  int n = (c & 0xe0) == 0xc0 ? 1 : (c & 0xf0) == 0xe0 ? 2 : 3;
  int cp = c & (0x3f >> n);
  for (int k = 0; k < n && i < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i++] & 0x3f);
  return cp;
}
}  // namespace

namespace fonts {
bool init() {
  g_serif = load("fonts/Cinzel-Bold.ttf");
  g_sans = load("fonts/NotoSans-Bold.ttf");
  if (!g_sans && g_serif) return true;
  return g_serif || g_sans;
}
FontFace* serif() { return g_serif ? g_serif.get() : g_sans.get(); }
FontFace* sans() { return g_sans ? g_sans.get() : g_serif.get(); }

float measure(FontFace* f, const std::string& s, float px) {
  if (!f) return s.size() * px * 0.5f;
  float scale = stbtt_ScaleForPixelHeight(&f->info, px);
  float w = 0;
  size_t i = 0;
  int prev = 0;
  while (i < s.size()) {
    int cp = decodeUtf8(s, i);
    int adv, lsb;
    stbtt_GetCodepointHMetrics(&f->info, cp, &adv, &lsb);
    if (prev) w += stbtt_GetCodepointKernAdvance(&f->info, prev, cp) * scale;
    w += adv * scale;
    prev = cp;
  }
  return w;
}

void shutdown() {
  g_serif.reset();
  g_sans.reset();
}
}  // namespace fonts

void Canvas::init(int W, int H) {
  w = W;
  h = H;
  px.assign((size_t)w * h * 4, 0);
}

void Canvas::blend(int x, int y, uint32_t rgba, float cover) {
  if (x < 0 || y < 0 || x >= w || y >= h || cover <= 0) return;
  uint8_t* d = &px[((size_t)y * w + x) * 4];
  float a = ((rgba >> 24) & 255) / 255.0f * std::min(1.0f, cover);
  for (int c = 0; c < 3; c++) {
    float s = ((rgba >> (8 * c)) & 255);
    d[c] = (uint8_t)std::lround(d[c] + (s - d[c]) * a);
  }
  d[3] = (uint8_t)std::lround(d[3] + (255 - d[3]) * a);
}

void Canvas::fillRect(int x, int y, int rw, int rh, uint32_t rgba) {
  for (int j = std::max(0, y); j < std::min(h, y + rh); j++)
    for (int i = std::max(0, x); i < std::min(w, x + rw); i++) blend(i, j, rgba, 1);
}

void Canvas::vgradient(int x, int y, int rw, int rh, uint32_t top, uint32_t bottom) {
  for (int j = 0; j < rh; j++) {
    float t = rh > 1 ? (float)j / (rh - 1) : 0;
    uint32_t c = 0;
    for (int k = 0; k < 4; k++) {
      float a = (top >> (8 * k)) & 255, b = (bottom >> (8 * k)) & 255;
      c |= (uint32_t)std::lround(a + (b - a) * t) << (8 * k);
    }
    for (int i = 0; i < rw; i++) blend(x + i, y + j, c, 1);
  }
}

void Canvas::line(float x0, float y0, float x1, float y1, float width, uint32_t rgba) {
  int minx = (int)std::floor(std::min(x0, x1) - width), maxx = (int)std::ceil(std::max(x0, x1) + width);
  int miny = (int)std::floor(std::min(y0, y1) - width), maxy = (int)std::ceil(std::max(y0, y1) + width);
  float dx = x1 - x0, dy = y1 - y0, l2 = std::max(1e-6f, dx * dx + dy * dy);
  for (int j = std::max(0, miny); j <= std::min(h - 1, maxy); j++)
    for (int i = std::max(0, minx); i <= std::min(w - 1, maxx); i++) {
      float px0 = i + 0.5f - x0, py0 = j + 0.5f - y0;
      float t = std::max(0.0f, std::min(1.0f, (px0 * dx + py0 * dy) / l2));
      float ex = px0 - dx * t, ey = py0 - dy * t;
      float d = std::sqrt(ex * ex + ey * ey);
      float cover = std::max(0.0f, std::min(1.0f, width * 0.5f + 0.5f - d));
      if (cover > 0) blend(i, j, rgba, cover);
    }
}

void Canvas::bezier(float x0, float y0, float c1x, float c1y, float c2x, float c2y, float x1, float y1, float width, uint32_t rgba) {
  const int N = 24;
  float px0 = x0, py0 = y0;
  for (int k = 1; k <= N; k++) {
    float t = (float)k / N, u = 1 - t;
    float x = u * u * u * x0 + 3 * u * u * t * c1x + 3 * u * t * t * c2x + t * t * t * x1;
    float y = u * u * u * y0 + 3 * u * u * t * c1y + 3 * u * t * t * c2y + t * t * t * y1;
    line(px0, py0, x, y, width, rgba);
    px0 = x;
    py0 = y;
  }
}

void Canvas::strokeRect(float x, float y, float rw, float rh, float width, uint32_t rgba) {
  float hw = width / 2;
  fillRect((int)std::lround(x - hw), (int)std::lround(y - hw), (int)std::lround(rw + width), (int)std::lround(width), rgba);
  fillRect((int)std::lround(x - hw), (int)std::lround(y + rh - hw), (int)std::lround(rw + width), (int)std::lround(width), rgba);
  fillRect((int)std::lround(x - hw), (int)std::lround(y + hw), (int)std::lround(width), (int)std::lround(rh - width), rgba);
  fillRect((int)std::lround(x + rw - hw), (int)std::lround(y + hw), (int)std::lround(width), (int)std::lround(rh - width), rgba);
}

void Canvas::text(FontFace* f, const std::string& s, float pxSize, float cx, float cy, uint32_t rgba, bool center) {
  if (!f) return;
  float scale = stbtt_ScaleForPixelHeight(&f->info, pxSize);
  float tw = fonts::measure(f, s, pxSize);
  float x = center ? cx - tw / 2 : cx;
  float baseline = cy + (f->ascent + f->descent) * scale * 0.5f;
  size_t i = 0;
  int prev = 0;
  while (i < s.size()) {
    int cp = decodeUtf8(s, i);
    if (prev) x += stbtt_GetCodepointKernAdvance(&f->info, prev, cp) * scale;
    int adv, lsb;
    stbtt_GetCodepointHMetrics(&f->info, cp, &adv, &lsb);
    int x0, y0, x1, y1;
    float fx = x - std::floor(x);
    stbtt_GetCodepointBitmapBoxSubpixel(&f->info, cp, scale, scale, fx, 0, &x0, &y0, &x1, &y1);
    int gw = x1 - x0, gh = y1 - y0;
    if (gw > 0 && gh > 0) {
      std::vector<uint8_t> bmp((size_t)gw * gh);
      stbtt_MakeCodepointBitmapSubpixel(&f->info, bmp.data(), gw, gh, gw, scale, scale, fx, 0, cp);
      int ox = (int)std::floor(x) + x0, oy = (int)std::lround(baseline) + y0;
      for (int j = 0; j < gh; j++)
        for (int k = 0; k < gw; k++) blend(ox + k, oy + j, rgba, bmp[(size_t)j * gw + k] / 255.0f);
    }
    x += adv * scale;
    prev = cp;
  }
}
