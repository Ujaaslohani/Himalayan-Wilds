#include <cstdlib>
#include "gfx.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace gfx {
int drawCalls = 0;
long long triangles = 0;
static bool s_s3tc = false;
static float s_aniso = 1.0f;
static size_t s_texBytes = 0;
static long long s_bufBytes = 0;

void init() {
  GLint n = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS, &n);
  for (GLint i = 0; i < n; i++) {
    const char* e = (const char*)glGetStringi(GL_EXTENSIONS, i);
    if (!e) continue;
    if (!std::strcmp(e, "GL_EXT_texture_compression_s3tc")) s_s3tc = !std::getenv("HW_NOBC");
    if (!std::strcmp(e, "GL_EXT_texture_filter_anisotropic") || !std::strcmp(e, "GL_ARB_texture_filter_anisotropic")) {
      float a = 1;
      glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &a);
      s_aniso = std::min(8.0f, std::max(1.0f, a));
    }
  }
}
bool hasS3TC() { return s_s3tc; }
float maxAniso() { return s_aniso; }
size_t textureBytes() { return s_texBytes; }
size_t bufferBytes() { return (size_t)std::max(0LL, s_bufBytes); }
void addBufferBytes(long long n) { s_bufBytes += n; }
void addTextureBytes(long long n) { s_texBytes = (size_t)std::max(0LL, (long long)s_texBytes + n); }
}  // namespace gfx

uint16_t floatToHalf(float f) {
  uint32_t x;
  std::memcpy(&x, &f, 4);
  uint32_t sign = (x >> 16) & 0x8000;
  int32_t exp = (int32_t)((x >> 23) & 0xff) - 127 + 15;
  uint32_t mant = x & 0x7fffff;
  if (exp <= 0) {
    if (exp < -10) return (uint16_t)sign;
    mant |= 0x800000;
    uint32_t t = 14 - exp;
    return (uint16_t)(sign | (mant >> t));
  }
  if (exp >= 31) return (uint16_t)(sign | 0x7c00);
  return (uint16_t)(sign | (exp << 10) | (mant >> 13));
}

GLsizei vertexStride(VFmt f) {
  switch (f) {
    case VFmt::P3C4: return sizeof(VtxP3C4);
    case VFmt::P3C4N4: return sizeof(VtxP3C4N4);
    case VFmt::GLB: return sizeof(VtxGlb);
    case VFmt::Terrain: return 4;
    case VFmt::Pos3: return 12;
    case VFmt::Pos2: return 8;
  }
  return 16;
}

static void setupAttribs(VFmt f) {
  GLsizei st = vertexStride(f);
  switch (f) {
    case VFmt::P3C4:
      glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void*)0);
      glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void*)12);
      glVertexAttrib4f(2, 0, 1, 0, 0);
      break;
    case VFmt::P3C4N4:
      glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void*)0);
      glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void*)12);
      glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_BYTE, GL_TRUE, st, (void*)16);
      break;
    case VFmt::GLB:
      glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void*)0);
      glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_BYTE, GL_TRUE, st, (void*)12);
      glEnableVertexAttribArray(3); glVertexAttribPointer(3, 2, GL_HALF_FLOAT, GL_FALSE, st, (void*)16);
      glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void*)20);
      break;
    case VFmt::Terrain:
      glEnableVertexAttribArray(4); glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_FALSE, st, (void*)0);
      break;
    case VFmt::Pos3:
      glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void*)0);
      break;
    case VFmt::Pos2:
      glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, st, (void*)0);
      break;
  }
}

Mesh uploadMesh(const MeshData& d) {
  Mesh m;
  if (d.verts.empty()) return m;
  m.fmt = d.fmt;
  m.vertexCount = d.vertexCount;
  glGenVertexArrays(1, &m.vao);
  glBindVertexArray(m.vao);
  glGenBuffers(1, &m.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)d.verts.size(), d.verts.data(), GL_STATIC_DRAW);
  gfx::addBufferBytes((long long)d.verts.size());
  setupAttribs(d.fmt);
  if (!d.idx.empty()) {
    glGenBuffers(1, &m.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    bool small = d.vertexCount < 65535;
    if (small) {
      std::vector<uint16_t> s(d.idx.begin(), d.idx.end());
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(s.size() * 2), s.data(), GL_STATIC_DRAW);
      gfx::addBufferBytes((long long)s.size() * 2);
      m.indexType = GL_UNSIGNED_SHORT;
    } else {
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(d.idx.size() * 4), d.idx.data(), GL_STATIC_DRAW);
      gfx::addBufferBytes((long long)d.idx.size() * 4);
      m.indexType = GL_UNSIGNED_INT;
    }
    m.count = (GLsizei)d.idx.size();
  } else {
    m.ibo = 0;
    m.count = d.vertexCount;
  }
  glBindVertexArray(0);
  m.bmin = d.bmin;
  m.bmax = d.bmax;
  m.center = (d.bmin + d.bmax) * 0.5f;
  m.radius = (d.bmax - d.bmin).length() * 0.5f;
  return m;
}

void Mesh::destroy() {
  if (vbo) glDeleteBuffers(1, &vbo);
  if (ibo) glDeleteBuffers(1, &ibo);
  if (vao) glDeleteVertexArrays(1, &vao);
  vao = vbo = ibo = 0;
}

void Mesh::draw() const {
  if (!vao) return;
  glBindVertexArray(vao);
  if (ibo) glDrawElements(GL_TRIANGLES, count, indexType, nullptr);
  else glDrawArrays(GL_TRIANGLES, 0, count);
  gfx::drawCalls++;
  gfx::triangles += count / 3;
}

void Mesh::drawInstanced(GLsizei n) const {
  if (!vao || n <= 0) return;
  glBindVertexArray(vao);
  if (ibo) glDrawElementsInstanced(GL_TRIANGLES, count, indexType, nullptr, n);
  else glDrawArraysInstanced(GL_TRIANGLES, 0, count, n);
  gfx::drawCalls++;
  gfx::triangles += (long long)(count / 3) * n;
}

void InstBatch::create(const Mesh* m, InstKind k, GLuint shared) {
  mesh = m;
  kind = k;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
  setupAttribs(m->fmt);
  if (m->ibo) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo);
  owns = shared == 0;
  if (owns) glGenBuffers(1, &ivbo);
  else ivbo = shared;
  glBindBuffer(GL_ARRAY_BUFFER, ivbo);
  if (k == InstKind::Lite) {
    GLsizei st = sizeof(InstLite);
    glEnableVertexAttribArray(6); glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, st, (void*)0); glVertexAttribDivisor(6, 1);
    glEnableVertexAttribArray(7); glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, st, (void*)16); glVertexAttribDivisor(7, 1);
  } else {
    GLsizei st = sizeof(InstFull);
    glEnableVertexAttribArray(6); glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, st, (void*)0); glVertexAttribDivisor(6, 1);
    glEnableVertexAttribArray(7); glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, st, (void*)16); glVertexAttribDivisor(7, 1);
    glEnableVertexAttribArray(8); glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, st, (void*)32); glVertexAttribDivisor(8, 1);
    glEnableVertexAttribArray(9); glVertexAttribPointer(9, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void*)48); glVertexAttribDivisor(9, 1);
  }
  glBindVertexArray(0);
}

void InstBatch::upload(const void* data, size_t n) {
  count = (GLsizei)n;
  if (!n) return;
  size_t st = kind == InstKind::Lite ? sizeof(InstLite) : sizeof(InstFull);
  glBindBuffer(GL_ARRAY_BUFFER, ivbo);
  if (n > capacity) {
    size_t cap = std::max<size_t>(16, n + n / 2);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(cap * st), nullptr, GL_DYNAMIC_DRAW);
    gfx::addBufferBytes((long long)((cap - capacity) * st));
    capacity = cap;
  } else {
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(capacity * st), nullptr, GL_DYNAMIC_DRAW);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(n * st), data);
}

void InstBatch::draw() const {
  if (!vao || count <= 0) return;
  glBindVertexArray(vao);
  if (mesh->ibo) glDrawElementsInstanced(GL_TRIANGLES, mesh->count, mesh->indexType, nullptr, count);
  else glDrawArraysInstanced(GL_TRIANGLES, 0, mesh->count, count);
  gfx::drawCalls++;
  gfx::triangles += (long long)(mesh->count / 3) * count;
}

void InstBatch::destroy() {
  if (ivbo && owns) glDeleteBuffers(1, &ivbo);
  if (vao) glDeleteVertexArrays(1, &vao);
  ivbo = vao = 0;
  capacity = 0;
}

GLint Shader::loc(const char* name) {
  auto it = locs.find(name);
  if (it != locs.end()) return it->second;
  GLint l = glGetUniformLocation(prog, name);
  locs[name] = l;
  return l;
}

static GLuint compileStage(GLenum type, const std::string& src) {
  GLuint s = glCreateShader(type);
  const char* c = src.c_str();
  glShaderSource(s, 1, &c, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[4096];
    glGetShaderInfoLog(s, sizeof log, nullptr, log);
    std::fprintf(stderr, "Shader compile error (%s):\n%s\n", type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
    int line = 1;
    std::fprintf(stderr, "%4d: ", line);
    for (char ch : src) {
      std::fputc(ch, stderr);
      if (ch == '\n') std::fprintf(stderr, "%4d: ", ++line);
    }
    std::fprintf(stderr, "\n");
    glDeleteShader(s);
    return 0;
  }
  return s;
}

bool compileShader(Shader& sh, const std::string& vs, const std::string& fs, const std::string& defines) {
  std::string head = "#version 330 core\n" + defines + "\n";
  GLuint v = compileStage(GL_VERTEX_SHADER, head + vs);
  GLuint f = compileStage(GL_FRAGMENT_SHADER, head + fs);
  if (!v || !f) return false;
  GLuint p = glCreateProgram();
  glAttachShader(p, v);
  glAttachShader(p, f);
  glBindAttribLocation(p, 0, "a_pos");
  glBindAttribLocation(p, 1, "a_col");
  glBindAttribLocation(p, 2, "a_nrm");
  glBindAttribLocation(p, 3, "a_uv");
  glBindAttribLocation(p, 4, "a_grid");
  glBindAttribLocation(p, 5, "a_extra");
  glBindAttribLocation(p, 6, "a_i0");
  glBindAttribLocation(p, 7, "a_i1");
  glBindAttribLocation(p, 8, "a_i2");
  glBindAttribLocation(p, 9, "a_itint");
  glLinkProgram(p);
  glDeleteShader(v);
  glDeleteShader(f);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[4096];
    glGetProgramInfoLog(p, sizeof log, nullptr, log);
    std::fprintf(stderr, "Shader link error:\n%s\n", log);
    glDeleteProgram(p);
    return false;
  }
  sh.prog = p;
  sh.locs.clear();
  GLuint blk = glGetUniformBlockIndex(p, "Frame");
  if (blk != GL_INVALID_INDEX) glUniformBlockBinding(p, blk, 0);
  return true;
}

void Texture::destroy() {
  if (id) {
    glDeleteTextures(1, &id);
    gfx::addTextureBytes(-(long long)bytes);
  }
  id = 0;
}

namespace {

inline uint16_t to565(int r, int g, int b) { return (uint16_t)(((r * 31 + 127) / 255) << 11 | ((g * 63 + 127) / 255) << 5 | ((b * 31 + 127) / 255)); }
inline void from565(uint16_t c, int& r, int& g, int& b) {
  r = ((c >> 11) & 31) * 255 / 31;
  g = ((c >> 5) & 63) * 255 / 63;
  b = (c & 31) * 255 / 31;
}

void encodeBlock(const uint8_t* px, bool alphaCut, uint8_t* out) {
  int mn[3] = {255, 255, 255}, mx[3] = {0, 0, 0};
  bool anyClear = false, anySolid = false;
  for (int i = 0; i < 16; i++) {
    const uint8_t* p = px + i * 4;
    if (alphaCut && p[3] < 128) { anyClear = true; continue; }
    anySolid = true;
    for (int c = 0; c < 3; c++) { mn[c] = std::min(mn[c], (int)p[c]); mx[c] = std::max(mx[c], (int)p[c]); }
  }
  if (!anySolid) {
    uint16_t z = 0;
    std::memcpy(out, &z, 2);
    std::memcpy(out + 2, &z, 2);
    uint32_t idx = 0xffffffffu;
    std::memcpy(out + 4, &idx, 4);
    return;
  }
  for (int c = 0; c < 3; c++) {
    int inset = (mx[c] - mn[c]) / 16;
    mn[c] = std::min(255, mn[c] + inset);
    mx[c] = std::max(0, mx[c] - inset);
  }
  uint16_t c0 = to565(mx[0], mx[1], mx[2]), c1 = to565(mn[0], mn[1], mn[2]);
  int pal[4][3];
  bool threeColor = anyClear;
  if (threeColor) { if (c0 > c1) std::swap(c0, c1); }
  else {
    if (c0 < c1) std::swap(c0, c1);
    if (c0 == c1) {
      uint32_t idx = 0;
      std::memcpy(out, &c0, 2);
      std::memcpy(out + 2, &c1, 2);
      std::memcpy(out + 4, &idx, 4);
      return;
    }
  }
  from565(c0, pal[0][0], pal[0][1], pal[0][2]);
  from565(c1, pal[1][0], pal[1][1], pal[1][2]);
  int npal;
  if (!threeColor) {
    for (int c = 0; c < 3; c++) {
      pal[2][c] = (2 * pal[0][c] + pal[1][c]) / 3;
      pal[3][c] = (pal[0][c] + 2 * pal[1][c]) / 3;
    }
    npal = 4;
  } else {
    for (int c = 0; c < 3; c++) pal[2][c] = (pal[0][c] + pal[1][c]) / 2;
    npal = 3;
  }
  uint32_t idx = 0;
  for (int i = 0; i < 16; i++) {
    const uint8_t* p = px + i * 4;
    int best = 0;
    if (threeColor && p[3] < 128) best = 3;
    else {
      int bd = 1 << 30;
      for (int k = 0; k < npal; k++) {
        int dr = p[0] - pal[k][0], dg = p[1] - pal[k][1], db = p[2] - pal[k][2];
        int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
        if (d < bd) { bd = d; best = k; }
      }
    }
    idx |= (uint32_t)best << (i * 2);
  }
  std::memcpy(out, &c0, 2);
  std::memcpy(out + 2, &c1, 2);
  std::memcpy(out + 4, &idx, 4);
}

std::vector<uint8_t> compressBC1(const uint8_t* rgba, int w, int h, bool alphaCut) {
  int bw = (w + 3) / 4, bh = (h + 3) / 4;
  std::vector<uint8_t> out((size_t)bw * bh * 8);
  uint8_t block[64];
  for (int by = 0; by < bh; by++)
    for (int bx = 0; bx < bw; bx++) {
      for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
          int sx = std::min(w - 1, bx * 4 + x), sy = std::min(h - 1, by * 4 + y);
          std::memcpy(block + (y * 4 + x) * 4, rgba + ((size_t)sy * w + sx) * 4, 4);
        }
      encodeBlock(block, alphaCut, out.data() + ((size_t)by * bw + bx) * 8);
    }
  return out;
}

void dilate(std::vector<uint8_t>& img, int w, int h, int passes) {
  std::vector<uint8_t> src;
  for (int pass = 0; pass < passes; pass++) {
    src = img;
    bool changed = false;
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        uint8_t* p = &img[((size_t)y * w + x) * 4];
        if (src[((size_t)y * w + x) * 4 + 3] >= 128) continue;
        int sr = 0, sg = 0, sb = 0, n = 0;
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++) {
            int xx = x + dx, yy = y + dy;
            if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
            const uint8_t* q = &src[((size_t)yy * w + xx) * 4];
            if (q[3] >= 128) { sr += q[0]; sg += q[1]; sb += q[2]; n++; }
          }
        if (n) {
          p[0] = (uint8_t)(sr / n); p[1] = (uint8_t)(sg / n); p[2] = (uint8_t)(sb / n);
          p[3] = 127;
          changed = true;
        }
      }
    for (size_t i = 3; i < img.size(); i += 4) if (img[i] == 127) img[i] = 0;
    for (size_t i = 0; i < img.size(); i += 4) if (img[i + 3] < 128 && src[i + 3] >= 128) img[i + 3] = src[i + 3];
    if (!changed) break;
  }
}

std::vector<uint8_t> downsample(const std::vector<uint8_t>& src, int w, int h, int& nw, int& nh, bool alphaCut, float coverage) {
  nw = std::max(1, w / 2);
  nh = std::max(1, h / 2);
  std::vector<uint8_t> out((size_t)nw * nh * 4);
  std::vector<float> alpha((size_t)nw * nh);
  for (int y = 0; y < nh; y++)
    for (int x = 0; x < nw; x++) {
      float acc[4] = {0, 0, 0, 0};
      float wsum = 0;
      for (int dy = 0; dy < 2; dy++)
        for (int dx = 0; dx < 2; dx++) {
          int sx = std::min(w - 1, x * 2 + dx), sy = std::min(h - 1, y * 2 + dy);
          const uint8_t* p = &src[((size_t)sy * w + sx) * 4];
          float a = p[3] / 255.0f;
          float wt = alphaCut ? (0.05f + a) : 1.0f;
          for (int c = 0; c < 3; c++) acc[c] += p[c] * wt;
          acc[3] += p[3];
          wsum += wt;
        }
      uint8_t* o = &out[((size_t)y * nw + x) * 4];
      for (int c = 0; c < 3; c++) o[c] = (uint8_t)std::min(255.0f, acc[c] / wsum + 0.5f);
      alpha[(size_t)y * nw + x] = acc[3] / 4.0f;
    }
  float scale = 1.0f;
  if (alphaCut && coverage > 0) {
    float lo = 0.5f, hi = 4.0f;
    for (int it = 0; it < 14; it++) {
      float s = (lo + hi) * 0.5f;
      size_t c = 0;
      for (float a : alpha) if (a * s >= 128.0f) c++;
      float cov = (float)c / alpha.size();
      if (cov < coverage) lo = s; else hi = s;
    }
    scale = (lo + hi) * 0.5f;
  }
  for (size_t i = 0; i < alpha.size(); i++) out[i * 4 + 3] = (uint8_t)std::min(255.0f, alpha[i] * scale + 0.5f);
  return out;
}

}  // namespace

TexLevels prepareTexture(const uint8_t* rgba, int w, int h, bool mipmaps, bool compress, bool alphaCut) {
  TexLevels L;
  L.alpha = alphaCut;
  std::vector<uint8_t> level(rgba, rgba + (size_t)w * h * 4);
  if (alphaCut) dilate(level, w, h, 8);
  float coverage = 0;
  if (alphaCut) {
    size_t c = 0;
    for (size_t i = 3; i < level.size(); i += 4) if (level[i] >= 128) c++;
    coverage = (float)c / (level.size() / 4);
  }
  L.bc = compress && gfx::hasS3TC() && (w % 4 == 0) && (h % 4 == 0);
  int lw = w, lh = h;
  while (true) {
    if (L.bc) L.data.push_back(compressBC1(level.data(), lw, lh, alphaCut));
    else L.data.push_back(level);
    L.dims.push_back({lw, lh});
    if (!mipmaps || (lw == 1 && lh == 1)) break;
    if (L.bc && (lw <= 4 || lh <= 4)) break;
    int nw, nh;
    level = downsample(level, lw, lh, nw, nh, alphaCut, coverage);
    lw = nw;
    lh = nh;
  }
  L.mipmaps = mipmaps;
  return L;
}

Texture uploadPrepared(const TexLevels& L, bool repeat) {
  Texture t;
  if (L.data.empty()) return t;
  t.w = L.dims[0].first;
  t.h = L.dims[0].second;
  glGenTextures(1, &t.id);
  glBindTexture(GL_TEXTURE_2D, t.id);
  size_t total = 0;
  for (size_t i = 0; i < L.data.size(); i++) {
    int lw = L.dims[i].first, lh = L.dims[i].second;
    if (L.bc) glCompressedTexImage2D(GL_TEXTURE_2D, (GLint)i, L.alpha ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT, lw, lh, 0, (GLsizei)L.data[i].size(), L.data[i].data());
    else glTexImage2D(GL_TEXTURE_2D, (GLint)i, GL_RGBA8, lw, lh, 0, GL_RGBA, GL_UNSIGNED_BYTE, L.data[i].data());
    total += L.data[i].size();
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint)L.data.size() - 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, L.mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  if (gfx::maxAniso() > 1.0f) glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, gfx::maxAniso());
  t.bytes = total;
  gfx::addTextureBytes((long long)total);
  return t;
}

Texture uploadTextureRGBA(const uint8_t* rgba, int w, int h, bool mipmaps, bool compress, bool alphaCut, bool repeat) {
  return uploadPrepared(prepareTexture(rgba, w, h, mipmaps, compress, alphaCut), repeat);
}

Texture uploadTextureR8(const uint8_t* data, int w, int h, bool linear) {
  Texture t;
  t.w = w;
  t.h = h;
  glGenTextures(1, &t.id);
  glBindTexture(GL_TEXTURE_2D, t.id);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, data);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  t.bytes = (size_t)w * h;
  gfx::addTextureBytes((long long)t.bytes);
  return t;
}

Texture createFloatTexture(const float* data, int w, int h) {
  Texture t;
  t.w = w;
  t.h = h;
  glGenTextures(1, &t.id);
  glBindTexture(GL_TEXTURE_2D, t.id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, w, h, 0, GL_RED, GL_FLOAT, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  t.bytes = (size_t)w * h * 4;
  gfx::addTextureBytes((long long)t.bytes);
  return t;
}

Texture createRGBA8Texture(const uint8_t* data, int w, int h, bool linear) {
  Texture t;
  t.w = w;
  t.h = h;
  glGenTextures(1, &t.id);
  glBindTexture(GL_TEXTURE_2D, t.id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  t.bytes = (size_t)w * h * 4;
  gfx::addTextureBytes((long long)t.bytes);
  return t;
}
