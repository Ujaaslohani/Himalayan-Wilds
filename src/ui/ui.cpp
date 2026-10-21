#include "ui.h"
#include "font.h"
#include "../gfx/gfx.h"
#include "../platform/platform.h"
#include "stb_truetype.h"
#include <unordered_map>
#include <cstring>


namespace ui {

namespace {

struct UIV {
  float x, y, u, v;
  uint32_t col;
  float lx, ly, hw, hh;
  float rad, wid, mode;
};

struct Glyph { float u0, v0, u1, v1, xoff, yoff, w, h, adv; bool ok; };

const float BASE = 48.0f;
const int PAD = 6;
const int ATLAS = 1024;

GLuint g_prog = 0, g_vao = 0, g_vbo = 0, g_atlas = 0;
GLint g_locScale = -1, g_locScreen = -1, g_locAtlas = -1, g_locImg = -1;
std::vector<UIV> g_verts;
std::unordered_map<int, Glyph> g_glyphs[2];
float g_lineAscent[2] = {0, 0}, g_lineDescent[2] = {0, 0};
int g_w = 0, g_h = 0;
float g_scale = 1;
float g_defShadow = 0;
unsigned g_curImg = 0;
std::vector<Rect> g_clips;
MouseState g_mouse{};
bool g_consumed = false;
size_t g_vcap = 0;
float g_whiteU = 0, g_whiteV = 0;

const char* VS = R"(#version 330 core
layout(location=0) in vec2 a_pos;
layout(location=1) in vec2 a_uv;
layout(location=2) in vec4 a_col;
layout(location=3) in vec4 a_loc;
layout(location=4) in vec3 a_par;
uniform vec2 u_screen;
uniform float u_scale;
out vec2 v_uv;
out vec4 v_col;
out vec4 v_loc;
out vec3 v_par;
void main(){
  v_uv = a_uv; v_col = a_col; v_loc = a_loc; v_par = a_par;
  vec2 p = a_pos * u_scale;
  gl_Position = vec4(p.x / u_screen.x * 2.0 - 1.0, 1.0 - p.y / u_screen.y * 2.0, 0.0, 1.0);
}
)";

const char* FS = R"(#version 330 core
in vec2 v_uv;
in vec4 v_col;
in vec4 v_loc;
in vec3 v_par;
uniform sampler2D u_atlas;
uniform sampler2D u_img;
uniform float u_scale;
out vec4 fragColor;
float sdRound(vec2 p, vec2 b, float r){ vec2 q = abs(p) - b + r; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; }
void main(){
  int mode = int(v_par.z + 0.5);
  float a = 1.0;
  vec4 c = v_col;
  if (mode == 0) {
    a = texture(u_atlas, v_uv).r;
  } else if (mode == 1) {
    float d = sdRound(v_loc.xy, v_loc.zw, v_par.x);
    a = clamp(0.5 - d * u_scale, 0.0, 1.0);
  } else if (mode == 2) {
    float d = sdRound(v_loc.xy, v_loc.zw, v_par.x);
    float w = v_par.y;
    a = clamp(0.5 - (abs(d + w * 0.5) - w * 0.5) * u_scale, 0.0, 1.0);
  } else if (mode == 3) {
    float dist = texture(u_atlas, v_uv).r;
    float aa = max(fwidth(dist) * 0.75, 0.01);
    a = smoothstep(0.5 - aa, 0.5 + aa, dist);
  } else if (mode == 4) {
    float r = length(v_loc.xy);
    float R = v_loc.z;
    float w = v_par.y;
    float d = w > 0.0 ? abs(r - R) - w * 0.5 : r - R;
    a = clamp(0.5 - d * u_scale, 0.0, 1.0);
    if (v_par.x < 6.28) {
      float ang = atan(v_loc.x, -v_loc.y);
      if (ang < 0.0) ang += 6.2831853;
      if (ang > v_par.x) a = 0.0;
    }
  } else if (mode == 5) {
    c *= texture(u_img, v_uv);
  } else if (mode == 6) {
    float r = length(v_loc.xy);
    a = smoothstep(v_par.x, 1.0, r);
  }
  fragColor = vec4(c.rgb, c.a * a);
  if (fragColor.a <= 0.002) discard;
}
)";

GLuint compile(GLenum t, const char* src) {
  GLuint s = glCreateShader(t);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[2048];
    glGetShaderInfoLog(s, sizeof log, nullptr, log);
    std::fprintf(stderr, "UI shader: %s\n", log);
  }
  return s;
}

FontFace* face(int f) { return f == SERIF ? fonts::serif() : fonts::sans(); }

void buildAtlas() {
  std::vector<uint8_t> img((size_t)ATLAS * ATLAS, 0);
  int x = 0, y = 0, rowH = 0;
  for (int yy = 0; yy < 4; yy++)
    for (int xx = 0; xx < 4; xx++) img[(size_t)yy * ATLAS + xx] = 255;
  g_whiteU = 2.0f / ATLAS;
  g_whiteV = 2.0f / ATLAS;
  x = 6;
  std::vector<int> cps;
  for (int c = 32; c < 127; c++) cps.push_back(c);
  int extra[] = {0xB7, 0x2014, 0x2013, 0x2019, 0x2018, 0x201C, 0x201D, 0x2192, 0xD7, 0x2026, 0x2212, 0xB0, 0xE9};
  for (int c : extra) cps.push_back(c);
  for (int fi = 0; fi < 2; fi++) {
    FontFace* f = face(fi);
    if (!f) continue;
    float scale = stbtt_ScaleForPixelHeight(&f->info, BASE);
    g_lineAscent[fi] = f->ascent * scale;
    g_lineDescent[fi] = f->descent * scale;
    for (int cp : cps) {
      int gi = stbtt_FindGlyphIndex(&f->info, cp);
      Glyph g{};
      int adv, lsb;
      stbtt_GetCodepointHMetrics(&f->info, cp, &adv, &lsb);
      g.adv = adv * scale;
      if (cp != 32 && gi == 0) { g.ok = false; g_glyphs[fi][cp] = g; continue; }
      int w = 0, h = 0, xo = 0, yo = 0;
      unsigned char* sdf = cp == 32 ? nullptr : stbtt_GetCodepointSDF(&f->info, scale, cp, PAD, 128, 128.0f / PAD, &w, &h, &xo, &yo);
      g.ok = true;
      if (sdf && w > 0 && h > 0) {
        if (x + w + 2 > ATLAS) { x = 0; y += rowH + 2; rowH = 0; }
        if (y + h + 2 <= ATLAS) {
          for (int j = 0; j < h; j++) std::memcpy(&img[(size_t)(y + j) * ATLAS + x], sdf + (size_t)j * w, w);
          g.u0 = (float)x / ATLAS;
          g.v0 = (float)y / ATLAS;
          g.u1 = (float)(x + w) / ATLAS;
          g.v1 = (float)(y + h) / ATLAS;
          g.xoff = (float)xo;
          g.yoff = (float)yo;
          g.w = (float)w;
          g.h = (float)h;
          x += w + 2;
          rowH = std::max(rowH, h);
        }
        stbtt_FreeSDF(sdf, nullptr);
      }
      g_glyphs[fi][cp] = g;
    }
  }
  glGenTextures(1, &g_atlas);
  glBindTexture(GL_TEXTURE_2D, g_atlas);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS, ATLAS, 0, GL_RED, GL_UNSIGNED_BYTE, img.data());
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void flush() {
  if (g_verts.empty()) return;
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  size_t bytes = g_verts.size() * sizeof(UIV);
  if (bytes > g_vcap) {
    g_vcap = bytes * 2;
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)g_vcap, nullptr, GL_STREAM_DRAW);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, g_verts.data());
  glBindVertexArray(g_vao);
  glDrawArrays(GL_TRIANGLES, 0, (GLsizei)g_verts.size());
  gfx::drawCalls++;
  g_verts.clear();
}

void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, uint32_t col, float lx0, float ly0, float lx1, float ly1, float hw, float hh, float rad, float wid, float mode) {
  UIV a{x0, y0, u0, v0, col, lx0, ly0, hw, hh, rad, wid, mode};
  UIV b{x1, y0, u1, v0, col, lx1, ly0, hw, hh, rad, wid, mode};
  UIV c{x1, y1, u1, v1, col, lx1, ly1, hw, hh, rad, wid, mode};
  UIV d{x0, y1, u0, v1, col, lx0, ly1, hw, hh, rad, wid, mode};
  g_verts.insert(g_verts.end(), {a, b, c, a, c, d});
}

void applyClip() {
  flush();
  if (g_clips.empty()) {
    glDisable(GL_SCISSOR_TEST);
    return;
  }
  const Rect& r = g_clips.back();
  glEnable(GL_SCISSOR_TEST);
  float S = g_scale;
  int x = (int)std::floor(r.x * S), y = (int)std::floor(g_h - (r.y + r.h) * S), w = (int)std::ceil(r.w * S), h = (int)std::ceil(r.h * S);
  glScissor(x, y, std::max(0, w), std::max(0, h));
}

int decode(const std::string& s, size_t& i) {
  unsigned char c = (unsigned char)s[i++];
  if (c < 0x80) return c;
  int n = (c & 0xe0) == 0xc0 ? 1 : (c & 0xf0) == 0xe0 ? 2 : 3;
  int cp = c & (0x3f >> n);
  for (int k = 0; k < n && i < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i++] & 0x3f);
  return cp;
}

const Glyph* glyph(int f, int cp) {
  auto it = g_glyphs[f].find(cp);
  if (it != g_glyphs[f].end() && it->second.ok) return &it->second;
  int other = 1 - f;
  it = g_glyphs[other].find(cp);
  if (it != g_glyphs[other].end() && it->second.ok) return &it->second;
  it = g_glyphs[f].find('?');
  return it != g_glyphs[f].end() ? &it->second : nullptr;
}

}  // namespace

uint32_t rgba(const char* hex, float alpha) {
  if (*hex == '#') hex++;
  unsigned v = 0;
  size_t n = std::strlen(hex);
  for (size_t i = 0; i < n && i < 8; i++) {
    char ch = hex[i];
    v = v * 16 + (unsigned)(ch >= '0' && ch <= '9' ? ch - '0' : (ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch - 'A' + 10));
  }
  uint32_t r, g, b, a = 255;
  if (n >= 8) { r = (v >> 24) & 255; g = (v >> 16) & 255; b = (v >> 8) & 255; a = v & 255; }
  else { r = (v >> 16) & 255; g = (v >> 8) & 255; b = v & 255; }
  a = (uint32_t)std::lround(a * clampf(alpha, 0, 1));
  return r | (g << 8) | (b << 16) | (a << 24);
}

uint32_t withAlpha(uint32_t c, float a) {
  uint32_t al = (uint32_t)std::lround(((c >> 24) & 255) * clampf(a, 0, 1));
  return (c & 0x00ffffffu) | (al << 24);
}

bool init() {
  GLuint v = compile(GL_VERTEX_SHADER, VS), f = compile(GL_FRAGMENT_SHADER, FS);
  g_prog = glCreateProgram();
  glAttachShader(g_prog, v);
  glAttachShader(g_prog, f);
  glLinkProgram(g_prog);
  glDeleteShader(v);
  glDeleteShader(f);
  g_locScreen = glGetUniformLocation(g_prog, "u_screen");
  g_locScale = glGetUniformLocation(g_prog, "u_scale");
  g_locAtlas = glGetUniformLocation(g_prog, "u_atlas");
  g_locImg = glGetUniformLocation(g_prog, "u_img");
  glGenVertexArrays(1, &g_vao);
  glBindVertexArray(g_vao);
  glGenBuffers(1, &g_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  GLsizei st = sizeof(UIV);
  glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, st, (void*)0);
  glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, st, (void*)8);
  glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void*)16);
  glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, st, (void*)20);
  glEnableVertexAttribArray(4); glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, st, (void*)36);
  glBindVertexArray(0);
  buildAtlas();
  return true;
}

void shutdown() {
  if (g_atlas) glDeleteTextures(1, &g_atlas);
  if (g_vbo) glDeleteBuffers(1, &g_vbo);
  if (g_vao) glDeleteVertexArrays(1, &g_vao);
  if (g_prog) glDeleteProgram(g_prog);
}

void begin(int w, int h, float s) {
  g_w = w;
  g_h = h;
  g_scale = s;
  g_clips.clear();
  g_curImg = 0;
  g_consumed = false;
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(g_prog);
  glUniform2f(g_locScreen, (float)w, (float)h);
  glUniform1f(g_locScale, s);
  glUniform1i(g_locAtlas, 0);
  glUniform1i(g_locImg, 1);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g_atlas);
}

void end() {
  flush();
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}

float scale() { return g_scale; }
void setTextShadow(float k) { g_defShadow = k; }
float width() { return g_w / g_scale; }
float height() { return g_h / g_scale; }

void rect(const Rect& r, uint32_t c, float radius) {
  if (r.w <= 0 || r.h <= 0) return;
  if (radius <= 0) {
    quad(r.x, r.y, r.x + r.w, r.y + r.h, g_whiteU, g_whiteV, g_whiteU, g_whiteV, c, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    return;
  }
  float hw = r.w / 2, hh = r.h / 2;
  radius = std::min(radius, std::min(hw, hh));
  quad(r.x - 1, r.y - 1, r.x + r.w + 1, r.y + r.h + 1, 0, 0, 0, 0, c, -hw - 1, -hh - 1, hw + 1, hh + 1, hw, hh, radius, 0, 1);
}

void border(const Rect& r, uint32_t c, float w, float radius) {
  float hw = r.w / 2, hh = r.h / 2;
  radius = std::min(radius, std::min(hw, hh));
  quad(r.x - 1, r.y - 1, r.x + r.w + 1, r.y + r.h + 1, 0, 0, 0, 0, c, -hw - 1, -hh - 1, hw + 1, hh + 1, hw, hh, radius, w, 2);
}

void circle(float cx, float cy, float r, uint32_t c) {
  quad(cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1, 0, 0, 0, 0, c, -r - 1, -r - 1, r + 1, r + 1, r, 0, 7, 0, 4);
}

void ring(float cx, float cy, float r, float w, uint32_t c, float start, float sweep) {
  float R = r + w;
  (void)start;
  quad(cx - R - 1, cy - R - 1, cx + R + 1, cy + R + 1, 0, 0, 0, 0, c, -R - 1, -R - 1, R + 1, R + 1, r, 0, sweep >= TAU ? 7.0f : sweep, w, 4);
}

void line(float x0, float y0, float x1, float y1, float w, uint32_t c) {
  float dx = x1 - x0, dy = y1 - y0, l = std::sqrt(dx * dx + dy * dy);
  if (l < 1e-4f) return;
  float nx = -dy / l * w * 0.5f, ny = dx / l * w * 0.5f;
  UIV a{x0 + nx, y0 + ny, g_whiteU, g_whiteV, c, 0, 0, 0, 0, 0, 0, 0};
  UIV b{x1 + nx, y1 + ny, g_whiteU, g_whiteV, c, 0, 0, 0, 0, 0, 0, 0};
  UIV cc{x1 - nx, y1 - ny, g_whiteU, g_whiteV, c, 0, 0, 0, 0, 0, 0, 0};
  UIV d{x0 - nx, y0 - ny, g_whiteU, g_whiteV, c, 0, 0, 0, 0, 0, 0, 0};
  g_verts.insert(g_verts.end(), {a, b, cc, a, cc, d});
}

void tri(Vec2 a, Vec2 b, Vec2 c, uint32_t col) {
  UIV va{a.x, a.y, g_whiteU, g_whiteV, col, 0, 0, 0, 0, 0, 0, 0};
  UIV vb{b.x, b.y, g_whiteU, g_whiteV, col, 0, 0, 0, 0, 0, 0, 0};
  UIV vc{c.x, c.y, g_whiteU, g_whiteV, col, 0, 0, 0, 0, 0, 0, 0};
  g_verts.insert(g_verts.end(), {va, vb, vc});
}

void poly(const std::vector<Vec2>& pts, uint32_t c) {
  for (size_t i = 1; i + 1 < pts.size(); i++) tri(pts[0], pts[i], pts[i + 1], c);
}

void image(unsigned tex, const Rect& r, uint32_t c, float u0, float v0, float u1, float v1) {
  flush();
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, tex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g_atlas);
  quad(r.x, r.y, r.x + r.w, r.y + r.h, u0, v0, u1, v1, c, 0, 0, 0, 0, 0, 0, 0, 0, 5);
  flush();
}

void gradientV(const Rect& r, uint32_t top, uint32_t bottom) {
  UIV a{r.x, r.y, g_whiteU, g_whiteV, top, 0, 0, 0, 0, 0, 0, 0};
  UIV b{r.x + r.w, r.y, g_whiteU, g_whiteV, top, 0, 0, 0, 0, 0, 0, 0};
  UIV c{r.x + r.w, r.y + r.h, g_whiteU, g_whiteV, bottom, 0, 0, 0, 0, 0, 0, 0};
  UIV d{r.x, r.y + r.h, g_whiteU, g_whiteV, bottom, 0, 0, 0, 0, 0, 0, 0};
  g_verts.insert(g_verts.end(), {a, b, c, a, c, d});
}

void radialVignette(uint32_t edge, float inner) {
  float W = width(), H = height();
  quad(0, 0, W, H, 0, 0, 0, 0, edge, -1, -1, 1, 1, 0, 0, inner, 0, 6);
}

void pushClip(const Rect& r) {
  Rect c = r;
  if (!g_clips.empty()) {
    const Rect& p = g_clips.back();
    float x0 = std::max(c.x, p.x), y0 = std::max(c.y, p.y), x1 = std::min(c.x + c.w, p.x + p.w), y1 = std::min(c.y + c.h, p.y + p.h);
    c = {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
  }
  g_clips.push_back(c);
  applyClip();
}

void popClip() {
  if (!g_clips.empty()) g_clips.pop_back();
  applyClip();
}

float textWidth(const std::string& s, float size, FontId f) {
  float k = size / BASE, w = 0;
  size_t i = 0;
  while (i < s.size()) {
    int cp = decode(s, i);
    const Glyph* g = glyph(f, cp);
    if (g) w += g->adv * k;
  }
  return w;
}

float text(const std::string& s, float x, float y, float size, uint32_t c, FontId f, int align, float shadow) {
  float w = textWidth(s, size, f);
  if (align == 1) x -= w / 2;
  else if (align == 2) x -= w;
  if (shadow == 0 && g_defShadow > 0) shadow = std::max(1.0f, size * g_defShadow);
  float k = size / BASE;
  float baseline = y + (g_lineAscent[f] + g_lineDescent[f]) * k * 0.5f + size * 0.04f;
  if (shadow > 0) {
    uint32_t sc = withAlpha(0xff000000u, ((c >> 24) & 255) / 255.0f * 0.85f);
    text(s, x + shadow, y + shadow, size, sc, f, 0, -1);
  }
  size_t i = 0;
  float px = x;
  while (i < s.size()) {
    int cp = decode(s, i);
    const Glyph* g = glyph(f, cp);
    if (!g) continue;
    if (g->w > 0) {
      float x0 = px + g->xoff * k, y0 = baseline + g->yoff * k;
      quad(x0, y0, x0 + g->w * k, y0 + g->h * k, g->u0, g->v0, g->u1, g->v1, c, 0, 0, 0, 0, 0, 0, 0, 0, 3);
    }
    px += g->adv * k;
  }
  return w;
}

float textShadowed(const std::string& s, float x, float y, float size, uint32_t c, FontId f, int align) {
  return text(s, x, y, size, c, f, align, std::max(1.0f, size * 0.08f));
}

std::vector<std::string> wrap(const std::string& s, float size, float maxW, FontId f) {
  std::vector<std::string> lines;
  std::string cur, word;
  auto flushWord = [&]() {
    if (word.empty()) return;
    std::string cand = cur.empty() ? word : cur + " " + word;
    if (textWidth(cand, size, f) > maxW && !cur.empty()) {
      lines.push_back(cur);
      cur = word;
    } else cur = cand;
    word.clear();
  };
  for (char ch : s) {
    if (ch == ' ') flushWord();
    else if (ch == '\n') { flushWord(); lines.push_back(cur); cur.clear(); }
    else word += ch;
  }
  flushWord();
  if (!cur.empty()) lines.push_back(cur);
  return lines;
}

float paragraph(const std::string& s, float x, float y, float maxW, float size, uint32_t c, FontId f, float lineH, int align) {
  auto lines = wrap(s, size, maxW, f);
  float yy = y;
  for (auto& l : lines) {
    float ax = align == 1 ? x + maxW / 2 : align == 2 ? x + maxW : x;
    text(l, ax, yy + size * 0.5f, size, c, f, align);
    yy += size * lineH;
  }
  return yy - y;
}

std::string ellipsize(const std::string& s, float size, float maxW, FontId f) {
  if (textWidth(s, size, f) <= maxW) return s;
  std::string out = s;
  while (!out.empty() && textWidth(out + "...", size, f) > maxW) out.pop_back();
  return out + "...";
}

void setMouse(const MouseState& m) {
  g_mouse = m;
  g_mouse.x /= g_scale;
  g_mouse.y /= g_scale;
}
const MouseState& mouse() { return g_mouse; }
bool hovered(const Rect& r) {
  if (!g_clips.empty() && !g_clips.back().contains(g_mouse.x, g_mouse.y)) return false;
  return r.contains(g_mouse.x, g_mouse.y);
}
bool clicked(const Rect& r) {
  if (g_consumed || !g_mouse.pressed || !hovered(r)) return false;
  g_consumed = true;
  return true;
}
bool anyHot() { return g_consumed; }
void consumeClick() { g_consumed = true; }
bool clickConsumed() { return g_consumed; }

}  // namespace ui
