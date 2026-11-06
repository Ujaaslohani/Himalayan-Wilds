#include "bakeao.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace {
const Vec3* posPtr(const MeshData& m, int i, Vec3& tmp) {
  const float* f;
  if (m.fmt == VFmt::GLB) f = reinterpret_cast<const VtxGlb*>(m.verts.data())[i].p;
  else if (m.fmt == VFmt::P3C4) f = reinterpret_cast<const VtxP3C4*>(m.verts.data())[i].p;
  else if (m.fmt == VFmt::P3C4N4) f = reinterpret_cast<const VtxP3C4N4*>(m.verts.data())[i].p;
  else return nullptr;
  tmp = Vec3(f[0], f[1], f[2]);
  return &tmp;
}
// 24 directions spread evenly over the sphere (Fibonacci), fixed so the bake is deterministic
struct Dirs {
  Vec3 d[24];
  Dirs() {
    for (int i = 0; i < 24; i++) {
      float y = 1 - (i + 0.5f) * 2.0f / 24, r = std::sqrt(std::max(0.0f, 1 - y * y)), a = i * 2.39996323f;
      d[i] = Vec3(std::cos(a) * r, y, std::sin(a) * r);
    }
  }
} DIRS;
}  // namespace

void AOGrid::build(int maxRes, float minCell) {
  Vec3 lo(1e9f, 1e9f, 1e9f), hi(-1e9f, -1e9f, -1e9f);
  Vec3 t;
  for (auto* m : meshes)
    for (int i = 0; i < m->vertexCount; i++) {
      const Vec3* p = posPtr(*m, i, t);
      if (!p) break;
      lo = Vec3(std::min(lo.x, p->x), std::min(lo.y, p->y), std::min(lo.z, p->z));
      hi = Vec3(std::max(hi.x, p->x), std::max(hi.y, p->y), std::max(hi.z, p->z));
    }
  if (lo.x > hi.x) return;
  float ext = std::max(hi.x - lo.x, std::max(hi.y - lo.y, hi.z - lo.z));
  cell = std::max(minCell, ext / maxRes);
  o = lo - Vec3(cell * 2, cell * 2, cell * 2);
  nx = (int)((hi.x - lo.x) / cell) + 5;
  ny = (int)((hi.y - lo.y) / cell) + 5;
  nz = (int)((hi.z - lo.z) / cell) + 5;
  v.assign((size_t)nx * ny * nz, 0);
  auto mark = [&](Vec3 p) {
    int i = (int)((p.x - o.x) / cell), j = (int)((p.y - o.y) / cell), k = (int)((p.z - o.z) / cell);
    if (i >= 0 && j >= 0 && k >= 0 && i < nx && j < ny && k < nz) v[((size_t)k * ny + j) * nx + i] = 1;
  };
  if (groundAt)  // terrain: fill each column below the ground once, so marching rays never call groundAt
    for (int k = 0; k < nz; k++)
      for (int i = 0; i < nx; i++) {
        float h = groundAt(o.x + (i + 0.5f) * cell, o.z + (k + 0.5f) * cell);
        int jt = std::min(ny, (int)((h - o.y) / cell));
        for (int j = 0; j < jt; j++) v[((size_t)k * ny + j) * nx + i] = 1;
      }
  for (auto* m : meshes) {
    Vec3 ta, tb, tc;
    for (size_t q = 0; q + 2 < m->idx.size(); q += 3) {
      const Vec3* a = posPtr(*m, (int)m->idx[q], ta);
      const Vec3* b = posPtr(*m, (int)m->idx[q + 1], tb);
      const Vec3* c = posPtr(*m, (int)m->idx[q + 2], tc);
      if (!a || !b || !c) break;
      float L = std::max((*b - *a).length(), std::max((*c - *a).length(), (*c - *b).length()));
      int n = std::min(64, (int)(L / (cell * 0.7f)) + 1);
      for (int u = 0; u <= n; u++)
        for (int w = 0; w <= n - u; w++) {
          float fu = (float)u / n, fw = (float)w / n;
          mark(*a + (*b - *a) * fu + (*c - *a) * fw);
        }
    }
  }
}

bool AOGrid::solid(Vec3 p) const {
  if (!groundAt && p.y < groundY) return true;
  int i = (int)((p.x - o.x) / cell), j = (int)((p.y - o.y) / cell), k = (int)((p.z - o.z) / cell);
  if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return false;
  return v[((size_t)k * ny + j) * nx + i] != 0;
}

float AOGrid::occlusion(Vec3 p, Vec3 n, bool sphere) const {
  if (v.empty()) return 0;
  Vec3 org = p + n * (cell * 1.5f);
  int rays = 0;
  float occ = 0;
  for (const Vec3& d : DIRS.d) {
    float c = d.dot(n);
    if (!sphere && c < 0.08f) continue;
    rays++;
    for (float s = cell; s < maxDist; s += cell) {
      if (solid(org + d * s)) { occ += 1.0f - 0.5f * s / maxDist; break; }  // near hits count more
    }
  }
  return rays ? occ / rays : 0;
}

void AOGrid::applyGLB(MeshData& m, float strength, bool sphere) const {
  if (m.fmt != VFmt::GLB) return;
  VtxGlb* vs = reinterpret_cast<VtxGlb*>(m.verts.data());
  for (int i = 0; i < m.vertexCount; i++) {
    Vec3 p(vs[i].p[0], vs[i].p[1], vs[i].p[2]);
    Vec3 n = Vec3(vs[i].n[0], vs[i].n[1], vs[i].n[2]) * (1.0f / 127);
    if (n.length() < 0.1f) n = Vec3(0, 1, 0);
    float a = std::min(1.0f, occlusion(p, n.normalized(), sphere) * strength);
    vs[i].n[3] = (int8_t)std::lround(a * 127);
  }
}

void AOGrid::applyColour(MeshData& m, float strength) const {
  if (m.fmt != VFmt::P3C4) return;
  VtxP3C4* vs = reinterpret_cast<VtxP3C4*>(m.verts.data());
  std::vector<Vec3> nrm(m.vertexCount, Vec3(0, 0, 0));
  for (size_t q = 0; q + 2 < m.idx.size(); q += 3) {
    uint32_t a = m.idx[q], b = m.idx[q + 1], c = m.idx[q + 2];
    Vec3 pa(vs[a].p[0], vs[a].p[1], vs[a].p[2]), pb(vs[b].p[0], vs[b].p[1], vs[b].p[2]), pc(vs[c].p[0], vs[c].p[1], vs[c].p[2]);
    Vec3 fn = (pb - pa).cross(pc - pa);
    nrm[a] = nrm[a] + fn; nrm[b] = nrm[b] + fn; nrm[c] = nrm[c] + fn;
  }
  for (int i = 0; i < m.vertexCount; i++) {
    uint32_t c = vs[i].c;
    if ((c & 0xff) > 250 && ((c >> 8) & 0xff) > 250 && ((c >> 16) & 0xff) > 250) continue;  // white = tinted at draw time
    if (nrm[i].length() < 1e-6f) continue;
    float a = std::min(1.0f, occlusion(Vec3(vs[i].p[0], vs[i].p[1], vs[i].p[2]), nrm[i].normalized(), false) * strength);
    float k = std::sqrt(1 - a);  // colours are stored gamma-encoded (~sqrt)
    auto ch = [&](int sh) { return (uint32_t)std::lround(((c >> sh) & 0xff) * k) << sh; };
    vs[i].c = ch(0) | ch(8) | ch(16) | (c & 0xff000000u);
  }
}
