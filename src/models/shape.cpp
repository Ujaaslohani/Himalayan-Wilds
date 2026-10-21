#include <algorithm>
#include "shape.h"
#include <unordered_map>
#include <cstring>

uint32_t hexToPacked(const char* hex) {
  if (*hex == '#') hex++;
  unsigned v = 0;
  for (int i = 0; i < 6 && hex[i]; i++) {
    char ch = hex[i];
    v = v * 16 + (unsigned)(ch >= '0' && ch <= '9' ? ch - '0' : (ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch - 'A' + 10));
  }
  return ((v >> 16) & 255) | (((v >> 8) & 255) << 8) | ((v & 255) << 16) | (255u << 24);
}

static void buildPlane(Geo& g, int u, int v, int w, float udir, float vdir, float width, float height, float depth, int gridX, int gridY) {
  float segW = width / gridX, segH = height / gridY;
  float wh = width / 2, hh = height / 2, dh = depth / 2;
  int gx1 = gridX + 1, gy1 = gridY + 1;
  uint32_t base = (uint32_t)g.pos.size();
  for (int iy = 0; iy < gy1; iy++) {
    float y = iy * segH - hh;
    for (int ix = 0; ix < gx1; ix++) {
      float x = ix * segW - wh;
      float vec[3], nv[3] = {0, 0, 0};
      vec[u] = x * udir;
      vec[v] = y * vdir;
      vec[w] = dh;
      nv[w] = depth > 0 ? 1.0f : -1.0f;
      g.pos.push_back({vec[0], vec[1], vec[2]});
      g.nrm.push_back({nv[0], nv[1], nv[2]});
    }
  }
  for (int iy = 0; iy < gridY; iy++)
    for (int ix = 0; ix < gridX; ix++) {
      uint32_t a = base + ix + gx1 * iy, b = base + ix + gx1 * (iy + 1), c = base + (ix + 1) + gx1 * (iy + 1), d = base + (ix + 1) + gx1 * iy;
      g.idx.insert(g.idx.end(), {a, b, d, b, c, d});
    }
}

Geo boxGeo(float w, float h, float d, int ws, int hs, int ds) {
  Geo g;
  buildPlane(g, 2, 1, 0, -1, -1, d, h, w, ds, hs);
  buildPlane(g, 2, 1, 0, 1, -1, d, h, -w, ds, hs);
  buildPlane(g, 0, 2, 1, 1, 1, w, d, h, ws, ds);
  buildPlane(g, 0, 2, 1, 1, -1, w, d, -h, ws, ds);
  buildPlane(g, 0, 1, 2, 1, -1, w, h, d, ws, hs);
  buildPlane(g, 0, 1, 2, -1, -1, w, h, -d, ws, hs);
  return g;
}

int g_geoLod = 0;
Geo cylinderGeo(float rTop, float rBot, float height, int radial, int hSeg, bool openEnded, float thetaStart, float thetaLen) {
  if (g_geoLod) { radial = std::max(3, radial / 2); hSeg = 1; }
  Geo g;
  float half = height / 2;
  std::vector<std::vector<uint32_t>> grid;
  float slope = (rBot - rTop) / height;
  for (int y = 0; y <= hSeg; y++) {
    std::vector<uint32_t> row;
    float v = (float)y / hSeg;
    float radius = v * (rBot - rTop) + rTop;
    for (int x = 0; x <= radial; x++) {
      float u = (float)x / radial;
      float th = u * thetaLen + thetaStart;
      float s = std::sin(th), c = std::cos(th);
      g.pos.push_back({radius * s, -v * height + half, radius * c});
      g.nrm.push_back(Vec3(s, slope, c).normalized());
      row.push_back((uint32_t)g.pos.size() - 1);
    }
    grid.push_back(row);
  }
  for (int x = 0; x < radial; x++)
    for (int y = 0; y < hSeg; y++) {
      uint32_t a = grid[y][x], b = grid[y + 1][x], c = grid[y + 1][x + 1], d = grid[y][x + 1];
      if (rTop > 0 || y != 0) g.idx.insert(g.idx.end(), {a, b, d});
      if (rBot > 0 || y != hSeg - 1) g.idx.insert(g.idx.end(), {b, c, d});
    }
  auto cap = [&](bool top) {
    uint32_t cs = (uint32_t)g.pos.size();
    float radius = top ? rTop : rBot, sign = top ? 1.0f : -1.0f;
    for (int x = 1; x <= radial; x++) { g.pos.push_back({0, half * sign, 0}); g.nrm.push_back({0, sign, 0}); }
    uint32_t ce = (uint32_t)g.pos.size();
    for (int x = 0; x <= radial; x++) {
      float u = (float)x / radial, th = u * thetaLen + thetaStart;
      g.pos.push_back({radius * std::sin(th), half * sign, radius * std::cos(th)});
      g.nrm.push_back({0, sign, 0});
    }
    for (int x = 0; x < radial; x++) {
      uint32_t c = cs + x, i = ce + x;
      if (top) g.idx.insert(g.idx.end(), {i, i + 1, c});
      else g.idx.insert(g.idx.end(), {i + 1, i, c});
    }
  };
  if (!openEnded) {
    if (rTop > 0) cap(true);
    if (rBot > 0) cap(false);
  }
  return g;
}

Geo coneGeo(float r, float h, int radial) { return cylinderGeo(0, r, h, radial); }

static Geo polyhedron(const std::vector<float>& vtx, const std::vector<int>& ind, float radius, int detail) {
  std::vector<Vec3> buf;
  auto subdivideFace = [&](Vec3 a, Vec3 b, Vec3 c) {
    int cols = detail + 1;
    std::vector<std::vector<Vec3>> v(cols + 1);
    for (int i = 0; i <= cols; i++) {
      Vec3 aj = Vec3::lerp(a, c, (float)i / cols), bj = Vec3::lerp(b, c, (float)i / cols);
      int rows = cols - i;
      v[i].resize(rows + 1);
      for (int j = 0; j <= rows; j++) {
        if (j == 0 && i == cols) v[i][j] = aj;
        else v[i][j] = Vec3::lerp(aj, bj, (float)j / rows);
      }
    }
    for (int i = 0; i < cols; i++)
      for (int j = 0; j < 2 * (cols - i) - 1; j++) {
        int k = j / 2;
        if (j % 2 == 0) { buf.push_back(v[i][k + 1]); buf.push_back(v[i + 1][k]); buf.push_back(v[i][k]); }
        else { buf.push_back(v[i][k + 1]); buf.push_back(v[i + 1][k + 1]); buf.push_back(v[i + 1][k]); }
      }
  };
  for (size_t i = 0; i < ind.size(); i += 3) {
    Vec3 a(vtx[ind[i] * 3], vtx[ind[i] * 3 + 1], vtx[ind[i] * 3 + 2]);
    Vec3 b(vtx[ind[i + 1] * 3], vtx[ind[i + 1] * 3 + 1], vtx[ind[i + 1] * 3 + 2]);
    Vec3 c(vtx[ind[i + 2] * 3], vtx[ind[i + 2] * 3 + 1], vtx[ind[i + 2] * 3 + 2]);
    subdivideFace(a, b, c);
  }
  Geo g;
  for (auto& p : buf) g.pos.push_back(p.normalized() * radius);
  for (size_t i = 0; i < g.pos.size(); i += 3) {
    if (detail == 0) {
      Vec3 n = (g.pos[i + 2] - g.pos[i + 1]).cross(g.pos[i] - g.pos[i + 1]).normalized();
      g.nrm.push_back(n); g.nrm.push_back(n); g.nrm.push_back(n);
    } else {
      for (int k = 0; k < 3; k++) g.nrm.push_back(g.pos[i + k].normalized());
    }
  }
  return g;
}

Geo icosaGeo(float radius, int detail) {
  if (g_geoLod) detail = std::min(detail, 0);
  static float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
  static std::vector<float> V = {-1, t, 0, 1, t, 0, -1, -t, 0, 1, -t, 0, 0, -1, t, 0, 1, t, 0, -1, -t, 0, 1, -t, t, 0, -1, t, 0, 1, -t, 0, -1, -t, 0, 1};
  static std::vector<int> I = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
                               3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};
  return polyhedron(V, I, radius, detail);
}

Geo dodecaGeo(float radius) {
  static float t = (1.0f + std::sqrt(5.0f)) / 2.0f, r = 1.0f / t;
  static std::vector<float> V = {-1, -1, -1, -1, -1, 1, -1, 1, -1, -1, 1, 1, 1, -1, -1, 1, -1, 1, 1, 1, -1, 1, 1, 1,
                                 0, -r, -t, 0, -r, t, 0, r, -t, 0, r, t, -r, -t, 0, -r, t, 0, r, -t, 0, r, t, 0,
                                 -t, 0, -r, t, 0, -r, -t, 0, r, t, 0, r};
  static std::vector<int> I = {3, 11, 7, 3, 7, 15, 3, 15, 13, 7, 19, 17, 7, 17, 6, 7, 6, 15, 17, 4, 8, 17, 8, 10, 17, 10, 6, 8, 0, 16, 8, 16, 2, 8, 2, 10,
                               0, 12, 1, 0, 1, 18, 0, 18, 16, 6, 10, 2, 6, 2, 13, 6, 13, 15, 2, 16, 18, 2, 18, 3, 2, 3, 13, 18, 1, 9, 18, 9, 11, 18, 11, 3,
                               4, 14, 12, 4, 12, 0, 4, 0, 8, 11, 9, 5, 11, 5, 19, 11, 19, 7, 19, 5, 14, 19, 14, 4, 19, 4, 17, 1, 12, 14, 1, 14, 5, 1, 5, 9};
  return polyhedron(V, I, radius, 0);
}

Geo torusGeo(float radius, float tube, int radialSeg, int tubularSeg, float arc) {
  if (g_geoLod) { radialSeg = std::max(3, radialSeg / 2); tubularSeg = std::max(4, tubularSeg / 2); }
  Geo g;
  for (int j = 0; j <= radialSeg; j++)
    for (int i = 0; i <= tubularSeg; i++) {
      float u = (float)i / tubularSeg * arc, v = (float)j / radialSeg * TAU;
      Vec3 p((radius + tube * std::cos(v)) * std::cos(u), (radius + tube * std::cos(v)) * std::sin(u), tube * std::sin(v));
      Vec3 c(radius * std::cos(u), radius * std::sin(u), 0);
      g.pos.push_back(p);
      g.nrm.push_back((p - c).normalized());
    }
  for (int j = 1; j <= radialSeg; j++)
    for (int i = 1; i <= tubularSeg; i++) {
      uint32_t a = (tubularSeg + 1) * j + i - 1, b = (tubularSeg + 1) * (j - 1) + i - 1, c = (tubularSeg + 1) * (j - 1) + i, d = (tubularSeg + 1) * j + i;
      g.idx.insert(g.idx.end(), {a, b, d, b, c, d});
    }
  return g;
}

Geo planeGeo(float w, float h, int ws, int hs) {
  Geo g;
  float wh = w / 2, hh = h / 2, sw = w / ws, sh = h / hs;
  for (int iy = 0; iy <= hs; iy++) {
    float y = iy * sh - hh;
    for (int ix = 0; ix <= ws; ix++) {
      g.pos.push_back({ix * sw - wh, -y, 0});
      g.nrm.push_back({0, 0, 1});
    }
  }
  for (int iy = 0; iy < hs; iy++)
    for (int ix = 0; ix < ws; ix++) {
      uint32_t a = ix + (ws + 1) * iy, b = ix + (ws + 1) * (iy + 1), c = (ix + 1) + (ws + 1) * (iy + 1), d = (ix + 1) + (ws + 1) * iy;
      g.idx.insert(g.idx.end(), {a, b, d, b, c, d});
    }
  return g;
}

Geo circleGeo(float r, int seg) {
  Geo g;
  g.pos.push_back({0, 0, 0});
  g.nrm.push_back({0, 0, 1});
  for (int s = 0; s <= seg; s++) {
    float a = (float)s / seg * TAU;
    g.pos.push_back({r * std::cos(a), r * std::sin(a), 0});
    g.nrm.push_back({0, 0, 1});
  }
  for (int i = 1; i <= seg; i++) g.idx.insert(g.idx.end(), {(uint32_t)i, (uint32_t)i + 1, 0u});
  return g;
}

Geo sphereGeo(float r, int ws, int hs) {
  if (g_geoLod) { ws = std::max(4, ws / 2); hs = std::max(3, hs / 2); }
  Geo g;
  std::vector<std::vector<uint32_t>> grid;
  for (int iy = 0; iy <= hs; iy++) {
    std::vector<uint32_t> row;
    float v = (float)iy / hs;
    for (int ix = 0; ix <= ws; ix++) {
      float u = (float)ix / ws;
      Vec3 p(-r * std::cos(u * TAU) * std::sin(v * PI), r * std::cos(v * PI), r * std::sin(u * TAU) * std::sin(v * PI));
      g.pos.push_back(p);
      g.nrm.push_back(p.normalized());
      row.push_back((uint32_t)g.pos.size() - 1);
    }
    grid.push_back(row);
  }
  for (int iy = 0; iy < hs; iy++)
    for (int ix = 0; ix < ws; ix++) {
      uint32_t a = grid[iy][ix + 1], b = grid[iy][ix], c = grid[iy + 1][ix], d = grid[iy + 1][ix + 1];
      if (iy != 0) g.idx.insert(g.idx.end(), {a, b, d});
      if (iy != hs - 1) g.idx.insert(g.idx.end(), {b, c, d});
    }
  return g;
}

Geo prismGeo(float base, float height, float depth) {
  Geo g;
  Vec3 A(-base / 2, 0, -depth / 2), B(base / 2, 0, -depth / 2), C(0, height, -depth / 2);
  Vec3 A2 = A + Vec3(0, 0, depth), B2 = B + Vec3(0, 0, depth), C2 = C + Vec3(0, 0, depth);
  auto tri = [&](Vec3 a, Vec3 b, Vec3 c) {
    Vec3 n = (b - a).cross(c - a).normalized();
    uint32_t s = (uint32_t)g.pos.size();
    g.pos.insert(g.pos.end(), {a, b, c});
    g.nrm.insert(g.nrm.end(), {n, n, n});
    g.idx.insert(g.idx.end(), {s, s + 1, s + 2});
  };
  tri(A, C, B);
  tri(A2, B2, C2);
  tri(A, B, B2); tri(A, B2, A2);
  tri(B, C, C2); tri(B, C2, B2);
  tri(C, A, A2); tri(C, A2, C2);
  return g;
}

void computeSmoothNormals(Geo& g) {
  g.nrm.assign(g.pos.size(), Vec3(0, 0, 0));
  for (size_t i = 0; i + 2 < g.idx.size(); i += 3) {
    uint32_t a = g.idx[i], b = g.idx[i + 1], c = g.idx[i + 2];
    Vec3 n = (g.pos[c] - g.pos[b]).cross(g.pos[a] - g.pos[b]);
    g.nrm[a] += n; g.nrm[b] += n; g.nrm[c] += n;
  }
  for (auto& n : g.nrm) n = n.normalized();
}

Geo translated(Geo g, const Vec3& t) {
  for (auto& p : g.pos) p += t;
  return g;
}

Mat4 placeMatrix(Vec3 pos, Vec3 rot, Vec3 scale) {
  return Mat4::compose(pos, Quat::fromEulerXYZ(rot.x, rot.y, rot.z), scale);
}

static void normalMatrix(const Mat4& m, float out[9]) {
  float a00 = m.m[0], a01 = m.m[1], a02 = m.m[2], a10 = m.m[4], a11 = m.m[5], a12 = m.m[6], a20 = m.m[8], a21 = m.m[9], a22 = m.m[10];
  float b01 = a22 * a11 - a12 * a21, b11 = -a22 * a10 + a12 * a20, b21 = a21 * a10 - a11 * a20;
  float det = a00 * b01 + a01 * b11 + a02 * b21;
  if (std::fabs(det) < 1e-20f) det = 1e-20f;
  float id = 1.0f / det;
  float inv[9] = {b01 * id, (-a22 * a01 + a02 * a21) * id, (a12 * a01 - a02 * a11) * id,
                  b11 * id, (a22 * a00 - a02 * a20) * id, (-a12 * a00 + a02 * a10) * id,
                  b21 * id, (-a21 * a00 + a01 * a20) * id, (a11 * a00 - a01 * a10) * id};
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++) out[c * 3 + r] = inv[r * 3 + c];
}

Shape& Shape::addM(const Geo& g, uint32_t packed, const Mat4& m, Pivot pivot) {
  float nm[9];
  normalMatrix(m, nm);
  Vec3 bmin(1e9f, 1e9f, 1e9f), bmax(-1e9f, -1e9f, -1e9f);
  if (pivot != Pivot::None)
    for (auto& p : g.pos) {
      bmin = Vec3(std::min(bmin.x, p.x), std::min(bmin.y, p.y), std::min(bmin.z, p.z));
      bmax = Vec3(std::max(bmax.x, p.x), std::max(bmax.y, p.y), std::max(bmax.z, p.z));
    }
  auto emit = [&](uint32_t i) {
    const Vec3& p = g.pos[i];
    V v;
    v.p = m.transformPoint(p);
    Vec3 n = i < g.nrm.size() ? g.nrm[i] : Vec3(0, 1, 0);
    v.n = Vec3(nm[0] * n.x + nm[3] * n.y + nm[6] * n.z, nm[1] * n.x + nm[4] * n.y + nm[7] * n.z, nm[2] * n.x + nm[5] * n.y + nm[8] * n.z).normalized();
    uint32_t c = packed;
    if (pivot != Pivot::None) {
      float t = 0;
      if (pivot == Pivot::Top) t = (bmax.y - p.y) / std::max(1e-4f, bmax.y - bmin.y);
      else if (pivot == Pivot::XNeg) t = (p.x - bmin.x) / std::max(1e-4f, bmax.x - bmin.x);
      else t = (p.z - bmin.z) / std::max(1e-4f, bmax.z - bmin.z);
      uint32_t w = (uint32_t)std::lround(clampf(t, 0, 1) * 254.0f);
      c = (c & 0x00ffffffu) | (w << 24);
    }
    v.c = c;
    verts.push_back(v);
  };
  if (!g.idx.empty()) for (uint32_t i : g.idx) emit(i);
  else for (uint32_t i = 0; i < g.pos.size(); i++) emit(i);
  return *this;
}

Shape& Shape::addC(const Geo& g, uint32_t packed, Vec3 pos, Vec3 rot, Vec3 scale) {
  uint32_t c = (packed & 0x00ffffffu) | (255u << 24);
  return addM(g, c, placeMatrix(pos, rot, scale));
}

Shape& Shape::add(const Geo& g, const char* hex, Vec3 pos, Vec3 rot, Vec3 scale) {
  return addC(g, hexToPacked(hex), pos, rot, scale);
}

Shape& Shape::cloth(const Geo& g, const char* hex, Pivot pivot, Vec3 pos, Vec3 rot) {
  return addM(g, hexToPacked(hex), placeMatrix(pos, rot, {1, 1, 1}), pivot);
}

Shape& Shape::ball(const char* hex, Vec3 pos, Vec3 scale, Vec3 rot, int detail) {
  float m = std::max(std::fabs(scale.x), std::max(std::fabs(scale.y), std::fabs(scale.z)));
  int d = detail >= 3 ? 3 : m < 0.07f ? (detail >= 2 ? 1 : 0) : m < 0.6f ? std::min(detail, 1) : detail;
  return add(icosaGeo(1, d), hex, pos, rot, scale);
}

Shape& Shape::box(const char* hex, Vec3 pos, Vec3 size, Vec3 rot) {
  return add(boxGeo(size.x, size.y, size.z), hex, pos, rot);
}

Shape& Shape::tube(const char* hex, Vec3 pos, float rTop, float rBot, float h, int seg, Vec3 rot, Vec3 scale) {
  return add(cylinderGeo(rTop, rBot, h, seg), hex, pos, rot, scale);
}

Shape& Shape::cone(const char* hex, Vec3 pos, float r, float h, int seg, Vec3 rot, Vec3 scale) {
  return add(coneGeo(r, h, seg), hex, pos, rot, scale);
}

Shape& Shape::rod(const char* hex, Vec3 a, Vec3 b, float rA, float rB, int seg) {
  Vec3 d = b - a;
  float len = d.length();
  Quat q = Quat::fromUnitVectors({0, 1, 0}, d.normalized());
  Mat4 m = Mat4::compose((a + b) * 0.5f, q, {1, 1, 1});
  return addM(cylinderGeo(rB, rA, len, seg), hexToPacked(hex), m);
}

void Shape::append(const Shape& o) { verts.insert(verts.end(), o.verts.begin(), o.verts.end()); }

void Shape::reshape(void (*fn)(Vec3&)) {
  for (auto& v : verts) fn(v.p);
}

namespace {
struct Key {
  int32_t x, y, z;
  uint32_t c;
  int32_t n;
  bool operator==(const Key& o) const { return x == o.x && y == o.y && z == o.z && c == o.c && n == o.n; }
};
struct KeyHash {
  size_t operator()(const Key& k) const {
    uint64_t h = (uint64_t)(uint32_t)k.x * 73856093u ^ (uint64_t)(uint32_t)k.y * 19349663u ^ (uint64_t)(uint32_t)k.z * 83492791u ^ (uint64_t)k.c * 2654435761u ^ (uint64_t)(uint32_t)k.n * 40503u;
    return (size_t)(h ^ (h >> 29));
  }
};
}  // namespace

MeshData Shape::build(bool smooth) const {
  MeshData d;
  d.fmt = smooth ? VFmt::P3C4N4 : VFmt::P3C4;
  std::unordered_map<Key, uint32_t, KeyHash> map;
  map.reserve(verts.size());
  std::vector<VtxP3C4> flat;
  std::vector<VtxP3C4N4> sm;
  for (const V& v : verts) {
    int8_t nx = 0, ny = 0, nz = 0;
    if (smooth) {
      nx = (int8_t)std::lround(clampf(v.n.x, -1, 1) * 127);
      ny = (int8_t)std::lround(clampf(v.n.y, -1, 1) * 127);
      nz = (int8_t)std::lround(clampf(v.n.z, -1, 1) * 127);
    }
    Key k{(int32_t)std::lround(v.p.x * 2000), (int32_t)std::lround(v.p.y * 2000), (int32_t)std::lround(v.p.z * 2000), v.c,
          smooth ? ((int32_t)(uint8_t)nx | ((int32_t)(uint8_t)ny << 8) | ((int32_t)(uint8_t)nz << 16)) : 0};
    auto it = map.find(k);
    uint32_t id;
    if (it == map.end()) {
      id = (uint32_t)(smooth ? sm.size() : flat.size());
      map.emplace(k, id);
      if (smooth) {
        VtxP3C4N4 o;
        o.p[0] = v.p.x; o.p[1] = v.p.y; o.p[2] = v.p.z;
        o.c = v.c;
        o.n[0] = nx; o.n[1] = ny; o.n[2] = nz; o.n[3] = 0;
        sm.push_back(o);
      } else {
        VtxP3C4 o;
        o.p[0] = v.p.x; o.p[1] = v.p.y; o.p[2] = v.p.z;
        o.c = v.c;
        flat.push_back(o);
      }
      d.bmin = Vec3(std::min(d.bmin.x, v.p.x), std::min(d.bmin.y, v.p.y), std::min(d.bmin.z, v.p.z));
      d.bmax = Vec3(std::max(d.bmax.x, v.p.x), std::max(d.bmax.y, v.p.y), std::max(d.bmax.z, v.p.z));
    } else id = it->second;
    d.idx.push_back(id);
  }
  if (smooth) {
    d.vertexCount = (int)sm.size();
    d.verts.resize(sm.size() * sizeof(VtxP3C4N4));
    if (!sm.empty()) std::memcpy(d.verts.data(), sm.data(), d.verts.size());
  } else {
    d.vertexCount = (int)flat.size();
    d.verts.resize(flat.size() * sizeof(VtxP3C4));
    if (!flat.empty()) std::memcpy(d.verts.data(), flat.data(), d.verts.size());
  }
  return d;
}
