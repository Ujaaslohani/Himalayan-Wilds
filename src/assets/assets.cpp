#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "assets.h"
#include "../platform/platform.h"
#include "stb_image.h"
#include <unordered_map>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <cstring>
#include <cctype>

namespace {

struct CpuPart {
  MeshData data;
  int matIndex = 0;
  Mat4 local;
  bool identity = true;
};
struct CpuMat {
  std::string name;
  uint64_t texKey = 0;
  Color base{1, 1, 1};
  bool cutout = false;
};
struct CpuAsset {
  AssetRequest req;
  std::vector<CpuPart> parts;
  std::vector<CpuMat> mats;
  bool ok = false;
};
struct CpuTex {
  TexLevels levels;
  bool ready = false;
  std::vector<uint8_t> alpha;
  int aw = 0, ah = 0;
};

std::map<std::string, std::unique_ptr<Asset>> g_assets;
std::unordered_map<uint64_t, std::unique_ptr<Texture>> g_textures;
std::vector<std::string> g_missing;

uint64_t fnv(const uint8_t* d, size_t n) {
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < n; i++) { h ^= d[i]; h *= 1099511628211ull; }
  return h ^ n;
}

std::string lower(std::string s) {
  for (auto& c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}

uint8_t linToSrgb8(float c) {
  return (uint8_t)std::lround(clampf(Color::linearToSrgb(clampf(c, 0, 1)), 0, 1) * 255.0f);
}

void resizeHalf(std::vector<uint8_t>& img, int& w, int& h) {
  int nw = std::max(1, w / 2), nh = std::max(1, h / 2);
  std::vector<uint8_t> out((size_t)nw * nh * 4);
  for (int y = 0; y < nh; y++)
    for (int x = 0; x < nw; x++)
      for (int c = 0; c < 4; c++) {
        int x0 = std::min(w - 1, x * 2), x1 = std::min(w - 1, x * 2 + 1), y0 = std::min(h - 1, y * 2), y1 = std::min(h - 1, y * 2 + 1);
        int s = img[((size_t)y0 * w + x0) * 4 + c] + img[((size_t)y0 * w + x1) * 4 + c] + img[((size_t)y1 * w + x0) * 4 + c] + img[((size_t)y1 * w + x1) * 4 + c];
        out[((size_t)y * nw + x) * 4 + c] = (uint8_t)((s + 2) / 4);
      }
  img.swap(out);
  w = nw;
  h = nh;
}

struct Loader {
  std::mutex mtx;
  std::unordered_map<uint64_t, std::shared_ptr<CpuTex>> texJobs;
  std::vector<std::pair<uint64_t, std::pair<std::vector<uint8_t>, bool>>> pendingTex;

  void loadOne(CpuAsset& out) {
    std::string path = platform::findAsset(out.req.file);
    if (path.empty()) return;
    std::vector<uint8_t> bytes;
    if (!platform::readFile(path, bytes)) return;
    cgltf_options opt{};
    cgltf_data* data = nullptr;
    if (cgltf_parse(&opt, bytes.data(), bytes.size(), &data) != cgltf_result_success) return;
    if (cgltf_load_buffers(&opt, data, nullptr) != cgltf_result_success) { cgltf_free(data); return; }
    std::unordered_map<const cgltf_material*, int> matIndex;
    for (cgltf_size n = 0; n < data->nodes_count; n++) {
      cgltf_node* node = &data->nodes[n];
      if (!node->mesh) continue;
      float wm[16];
      cgltf_node_transform_world(node, wm);
      Mat4 world;
      std::memcpy(world.m, wm, sizeof wm);
      bool ident = true;
      Mat4 I;
      for (int k = 0; k < 16; k++) if (std::fabs(world.m[k] - I.m[k]) > 1e-6f) ident = false;
      for (cgltf_size p = 0; p < node->mesh->primitives_count; p++) {
        cgltf_primitive* prim = &node->mesh->primitives[p];
        if (prim->type != cgltf_primitive_type_triangles) continue;
        const cgltf_accessor *apos = nullptr, *anrm = nullptr, *auv = nullptr, *acol = nullptr;
        for (cgltf_size a = 0; a < prim->attributes_count; a++) {
          auto& at = prim->attributes[a];
          if (at.type == cgltf_attribute_type_position) apos = at.data;
          else if (at.type == cgltf_attribute_type_normal) anrm = at.data;
          else if (at.type == cgltf_attribute_type_texcoord && at.index == 0) auv = at.data;
          else if (at.type == cgltf_attribute_type_color && at.index == 0) acol = at.data;
        }
        if (!apos) continue;
        int mi = 0;
        auto it = matIndex.find(prim->material);
        if (it != matIndex.end()) mi = it->second;
        else {
          CpuMat m;
          if (prim->material) {
            const cgltf_material* cm = prim->material;
            m.name = cm->name ? cm->name : "";
            const float* bf = cm->pbr_metallic_roughness.base_color_factor;
            m.base = Color(bf[0], bf[1], bf[2]);
            m.cutout = cm->alpha_mode != cgltf_alpha_mode_opaque;
            const cgltf_texture* tx = cm->pbr_metallic_roughness.base_color_texture.texture;
            if (tx && tx->image && tx->image->buffer_view) {
              const cgltf_buffer_view* bv = tx->image->buffer_view;
              const uint8_t* src = (const uint8_t*)bv->buffer->data + bv->offset;
              uint64_t key = fnv(src, bv->size) ^ (m.cutout ? 0x9e3779b97f4a7c15ull : 0);
              m.texKey = key;
              std::lock_guard<std::mutex> lk(mtx);
              if (!texJobs.count(key)) {
                texJobs[key] = std::make_shared<CpuTex>();
                pendingTex.push_back({key, {std::vector<uint8_t>(src, src + bv->size), m.cutout}});
              }
            }
          }
          std::string ln = lower(m.name);
          if (ln.find("grass") != std::string::npos || ln.find("rock") != std::string::npos || ln.find("mushroom") != std::string::npos || ln.find("bark") != std::string::npos) m.cutout = false;
          mi = (int)out.mats.size();
          out.mats.push_back(m);
          matIndex[prim->material] = mi;
        }
        size_t nv = apos->count;
        CpuPart part;
        part.matIndex = mi;
        part.local = Mat4();
        part.identity = true;
        part.data.fmt = VFmt::GLB;
        part.data.vertexCount = (int)nv;
        part.data.verts.resize(nv * sizeof(VtxGlb));
        VtxGlb* vv = (VtxGlb*)part.data.verts.data();
        float floorC = out.req.colorFloor;
        for (size_t i = 0; i < nv; i++) {
          float p3[3] = {0, 0, 0}, n3[3] = {0, 1, 0}, uv[2] = {0, 0}, c4[4] = {1, 1, 1, 1};
          cgltf_accessor_read_float(apos, i, p3, 3);
          if (anrm) cgltf_accessor_read_float(anrm, i, n3, 3);
          if (auv) cgltf_accessor_read_float(auv, i, uv, 2);
          if (acol) cgltf_accessor_read_float(acol, i, c4, acol->type == cgltf_type_vec4 ? 4 : 3);
          if (floorC >= 0 && acol) for (int c = 0; c < 3; c++) c4[c] = floorC + c4[c] * (1 - floorC);
          if (!ident) {
            Vec3 wp = world.transformPoint(Vec3(p3[0], p3[1], p3[2]));
            Vec3 wn = world.transformDir(Vec3(n3[0], n3[1], n3[2])).normalized();
            p3[0] = wp.x; p3[1] = wp.y; p3[2] = wp.z;
            n3[0] = wn.x; n3[1] = wn.y; n3[2] = wn.z;
          }
          VtxGlb& o = vv[i];
          o.p[0] = p3[0]; o.p[1] = p3[1]; o.p[2] = p3[2];
          for (int c = 0; c < 3; c++) o.n[c] = (int8_t)std::lround(clampf(n3[c], -1, 1) * 127);
          o.n[3] = 0;
          o.uv[0] = floatToHalf(uv[0]);
          o.uv[1] = floatToHalf(uv[1]);
          o.c = (uint32_t)linToSrgb8(c4[0]) | ((uint32_t)linToSrgb8(c4[1]) << 8) | ((uint32_t)linToSrgb8(c4[2]) << 16) | (255u << 24);
          part.data.bmin = Vec3(std::min(part.data.bmin.x, p3[0]), std::min(part.data.bmin.y, p3[1]), std::min(part.data.bmin.z, p3[2]));
          part.data.bmax = Vec3(std::max(part.data.bmax.x, p3[0]), std::max(part.data.bmax.y, p3[1]), std::max(part.data.bmax.z, p3[2]));
        }
        if (prim->indices) {
          part.data.idx.resize(prim->indices->count);
          for (size_t i = 0; i < prim->indices->count; i++) part.data.idx[i] = (uint32_t)cgltf_accessor_read_index(prim->indices, i);
        }
        out.parts.push_back(std::move(part));
      }
    }
    cgltf_free(data);
    out.ok = !out.parts.empty();
  }

  void decodeTex(uint64_t key, const std::vector<uint8_t>& enc, bool cutout) {
    int w, h, ch;
    uint8_t* px = stbi_load_from_memory(enc.data(), (int)enc.size(), &w, &h, &ch, 4);
    if (!px) return;
    std::vector<uint8_t> img(px, px + (size_t)w * h * 4);
    stbi_image_free(px);
    int maxS = std::max(64, g_settings.textureSize);
    while (w > maxS || h > maxS) resizeHalf(img, w, h);
    auto job = texJobs.at(key);
    if (cutout) {
      int aw = w, ah = h;
      std::vector<uint8_t> a(img);
      while (aw > 256 || ah > 256) resizeHalf(a, aw, ah);
      job->alpha.resize((size_t)aw * ah);
      for (size_t i = 0; i < job->alpha.size(); i++) job->alpha[i] = a[i * 4 + 3];
      job->aw = aw;
      job->ah = ah;
    }
    job->levels = prepareTexture(img.data(), w, h, true, true, cutout);
    job->ready = true;
  }
};

}  // namespace


namespace {
uint32_t lodHash(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}


double g_trimA = 0, g_trimQ = 0;
float halfToF(uint16_t h) {
  uint32_t s = (h >> 15) & 1, e = (h >> 10) & 31, m = h & 1023, f;
  if (e == 0) { if (m == 0) f = s << 31; else { e = 1; while (!(m & 1024)) { m <<= 1; e--; } m &= 1023; f = (s << 31) | ((e + 112) << 23) | (m << 13); } }
  else if (e == 31) f = (s << 31) | 0x7f800000 | (m << 13);
  else f = (s << 31) | ((e + 112) << 23) | (m << 13);
  float r; std::memcpy(&r, &f, 4); return r;
}

struct P2 { float x, y; };
float cross2(P2 o, P2 a, P2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }

std::vector<P2> hull2(std::vector<P2> p) {
  std::sort(p.begin(), p.end(), [](P2 a, P2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
  if (p.size() < 3) return p;
  std::vector<P2> h(2 * p.size());
  size_t k = 0;
  for (size_t i = 0; i < p.size(); i++) { while (k >= 2 && cross2(h[k - 2], h[k - 1], p[i]) <= 0) k--; h[k++] = p[i]; }
  for (size_t i = p.size() - 1, t = k + 1; i > 0; i--) { while (k >= t && cross2(h[k - 2], h[k - 1], p[i - 1]) <= 0) k--; h[k++] = p[i - 1]; }
  h.resize(k - 1);
  return h;
}

float polyArea(const std::vector<P2>& p) {
  float a = 0;
  for (size_t i = 0; i < p.size(); i++) { const P2& u = p[i]; const P2& v = p[(i + 1) % p.size()]; a += u.x * v.y - v.x * u.y; }
  return std::fabs(a) * 0.5f;
}

bool lineHit(P2 a, P2 b, P2 c, P2 d, P2& o) {
  float rx = b.x - a.x, ry = b.y - a.y, sx = d.x - c.x, sy = d.y - c.y;
  float den = rx * sy - ry * sx;
  if (std::fabs(den) < 1e-12f) return false;
  float qx = c.x - a.x, qy = c.y - a.y;
  float t = (qx * sy - qy * sx) / den, u = (qx * ry - qy * rx) / den;
  if (t < 1.0f || u < 1.0f) return false;
  o = {a.x + rx * t, a.y + ry * t};
  return true;
}

std::vector<P2> reduceTo(std::vector<P2> h, size_t maxN) {
  while (h.size() > maxN) {
    size_t n = h.size(), best = n;
    float bestAdd = 1e30f;
    P2 bestP{};
    for (size_t i = 0; i < n; i++) {
      P2 a = h[(i + n - 1) % n], b = h[i], c = h[(i + 1) % n], d = h[(i + 2) % n], o;
      if (!lineHit(a, b, d, c, o)) continue;
      float add = std::fabs(cross2(b, o, c)) * 0.5f;
      if (add < bestAdd) { bestAdd = add; best = i; bestP = o; }
    }
    if (best == n) break;
    h[best] = bestP;
    h.erase(h.begin() + (long)((best + 1) % n));
  }
  return h;
}

std::vector<P2> clipConvex(const std::vector<P2>& poly, const std::vector<P2>& clip) {
  std::vector<P2> out = poly;
  float sgn = 0;
  for (size_t i = 0; i < clip.size(); i++) sgn += cross2(clip[i], clip[(i + 1) % clip.size()], clip[(i + 2) % clip.size()]);
  for (size_t i = 0; i < clip.size() && !out.empty(); i++) {
    P2 a = clip[i], b = clip[(i + 1) % clip.size()];
    std::vector<P2> in = out;
    out.clear();
    auto inside = [&](P2 p) { return cross2(a, b, p) * sgn >= -1e-7f; };
    for (size_t j = 0; j < in.size(); j++) {
      P2 c = in[j], d = in[(j + 1) % in.size()];
      bool ci = inside(c), di = inside(d);
      if (ci) out.push_back(c);
      if (ci != di) {
        float t = cross2(a, b, c) / (cross2(a, b, c) - cross2(a, b, d));
        out.push_back({c.x + (d.x - c.x) * t, c.y + (d.y - c.y) * t});
      }
    }
  }
  return out;
}

int trimCards(MeshData& d, const std::vector<uint8_t>& alpha, int aw, int ah, int& removedPx) {
  if (d.fmt != VFmt::GLB || d.idx.empty() || alpha.empty()) return 0;
  const VtxGlb* v = (const VtxGlb*)d.verts.data();
  size_t nv = d.vertexCount, nt = d.idx.size() / 3;
  std::vector<uint32_t> parent(nv);
  for (size_t i = 0; i < nv; i++) parent[i] = (uint32_t)i;
  auto find = [&](uint32_t x) { while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; } return x; };
  for (size_t t = 0; t < nt; t++) {
    uint32_t a = find(d.idx[t * 3]);
    parent[find(d.idx[t * 3 + 1])] = a;
    parent[find(d.idx[t * 3 + 2])] = find(a);
  }
  std::unordered_map<uint32_t, std::vector<size_t>> comps;
  for (size_t t = 0; t < nt; t++) comps[find(d.idx[t * 3])].push_back(t);
  std::vector<VtxGlb> ov(v, v + nv);
  std::vector<uint32_t> oi;
  int trimmed = 0;
  for (auto& kv : comps) {
    auto& tris = kv.second;
    bool done = false;
    if (tris.size() == 2) {
      std::vector<uint32_t> ids;
      for (size_t t : tris) for (int k = 0; k < 3; k++) { uint32_t q = d.idx[t * 3 + k]; if (std::find(ids.begin(), ids.end(), q) == ids.end()) ids.push_back(q); }
      if (ids.size() == 4) {
        P2 uv[4];
        Vec3 ps[4];
        for (int i = 0; i < 4; i++) { uv[i] = {halfToF(v[ids[i]].uv[0]), halfToF(v[ids[i]].uv[1])}; ps[i] = Vec3(v[ids[i]].p[0], v[ids[i]].p[1], v[ids[i]].p[2]); }
        float m00 = uv[1].x - uv[0].x, m01 = uv[2].x - uv[0].x, m10 = uv[1].y - uv[0].y, m11 = uv[2].y - uv[0].y;
        float det = m00 * m11 - m01 * m10;
        bool inRange = true;
        for (auto& q : uv) if (q.x < -0.001f || q.x > 1.001f || q.y < -0.001f || q.y > 1.001f) inRange = false;
        if (std::fabs(det) > 1e-8f && inRange) {
          auto toPos = [&](P2 q) {
            float du = q.x - uv[0].x, dv = q.y - uv[0].y;
            float a = (m11 * du - m01 * dv) / det, b = (-m10 * du + m00 * dv) / det;
            return ps[0] + (ps[1] - ps[0]) * a + (ps[2] - ps[0]) * b;
          };
          Vec3 p3 = toPos(uv[3]);
          float size = (ps[1] - ps[0]).length() + (ps[2] - ps[0]).length();
          if ((p3 - ps[3]).length() < size * 0.02f) {
            std::vector<P2> quad = hull2({uv[0], uv[1], uv[2], uv[3]});
            float x0 = 1, y0 = 1, x1 = 0, y1 = 0;
            for (auto& q : quad) { x0 = std::min(x0, q.x); y0 = std::min(y0, q.y); x1 = std::max(x1, q.x); y1 = std::max(y1, q.y); }
            std::vector<P2> pts;
            float tx = 1.0f / aw, ty = 1.0f / ah;
            int ix0 = std::max(0, (int)std::floor(x0 * aw)), ix1 = std::min(aw - 1, (int)std::ceil(x1 * aw));
            int iy0 = std::max(0, (int)std::floor(y0 * ah)), iy1 = std::min(ah - 1, (int)std::ceil(y1 * ah));
            const float pad = 1.5f;
            for (int y = iy0; y <= iy1; y++) {
              int lo = -1, hi = -1;
              for (int x = ix0; x <= ix1; x++)
                if (alpha[(size_t)y * aw + x] >= 90) { if (lo < 0) lo = x; hi = x; }
              if (lo < 0) continue;
              float cy = (y + 0.5f) * ty;
              for (int x : {lo, hi}) {
                float cx = (x + 0.5f) * tx;
                pts.push_back({cx - tx * pad, cy - ty * pad});
                pts.push_back({cx + tx * pad, cy - ty * pad});
                pts.push_back({cx - tx * pad, cy + ty * pad});
                pts.push_back({cx + tx * pad, cy + ty * pad});
              }
            }
            float qa = polyArea(quad);
            if (pts.empty()) { done = true; trimmed++; removedPx += 1; }
            else {
              static std::unordered_map<uint64_t, std::vector<P2>> hullCache;
              uint64_t hk = ((uint64_t)(ix0 & 0xffff) << 48) ^ ((uint64_t)(iy0 & 0xffff) << 32) ^ ((uint64_t)(ix1 & 0xffff) << 16) ^ (uint64_t)(iy1 & 0xffff) ^ ((uint64_t)alpha.size() << 20) ^ (uint64_t)(uintptr_t)alpha.data();
              auto hc = hullCache.find(hk);
              std::vector<P2> h0 = hc != hullCache.end() ? hc->second : reduceTo(hull2(pts), 8);
              if (hc == hullCache.end()) hullCache[hk] = h0;
              std::vector<P2> h = h0.size() <= 8 ? clipConvex(h0, quad) : std::vector<P2>();
              if (h.size() >= 3 && h.size() <= 10 && polyArea(h) < qa * 0.9f) {
                g_trimA += polyArea(h); g_trimQ += qa;
                VtxGlb base = v[ids[0]];
                uint32_t start = (uint32_t)ov.size();
                for (auto& q : h) {
                  VtxGlb o = base;
                  Vec3 pp = toPos(q);
                  o.p[0] = pp.x; o.p[1] = pp.y; o.p[2] = pp.z;
                  o.uv[0] = floatToHalf(q.x);
                  o.uv[1] = floatToHalf(q.y);
                  ov.push_back(o);
                }
                bool flip = cross2(uv[0], uv[1], uv[2]) * cross2(h[0], h[1], h[2]) < 0;
                for (size_t i = 1; i + 1 < h.size(); i++) {
                  oi.push_back(start);
                  oi.push_back(start + (uint32_t)(flip ? i + 1 : i));
                  oi.push_back(start + (uint32_t)(flip ? i : i + 1));
                }
                done = true;
                trimmed++;
              }
            }
          }
        }
      }
    }
    if (!done) for (size_t t : tris) for (int k = 0; k < 3; k++) oi.push_back(d.idx[t * 3 + k]);
  }
  if (!trimmed) return 0;
  d.verts.assign((const uint8_t*)ov.data(), (const uint8_t*)(ov.data() + ov.size()));
  d.vertexCount = (int)ov.size();
  d.idx = oi;
  return trimmed;
}


// Thin a leaf canopy: drop most cards hidden deep inside the crown, and thin + enlarge outer cards.
void thinCanopy(MeshData& d, float innerKeep, float outerKeep, float grow) {
  if (d.fmt != VFmt::GLB || d.idx.empty()) return;
  VtxGlb* v = (VtxGlb*)d.verts.data();
  size_t nv = d.vertexCount, nt = d.idx.size() / 3;
  std::vector<uint32_t> parent(nv);
  for (size_t i = 0; i < nv; i++) parent[i] = (uint32_t)i;
  auto find = [&](uint32_t x) { while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; } return x; };
  for (size_t t = 0; t < nt; t++) {
    uint32_t a = find(d.idx[t * 3]), b = find(d.idx[t * 3 + 1]), c = find(d.idx[t * 3 + 2]);
    parent[b] = a; parent[find(c)] = find(a);
  }
  std::unordered_map<uint32_t, std::vector<size_t>> comps;
  for (size_t t = 0; t < nt; t++) comps[find(d.idx[t * 3])].push_back(t);
  if (comps.size() < 24) return;
  Vec3 mn = d.bmin, mx = d.bmax;
  Vec3 ctr = (mn + mx) * 0.5f;
  Vec3 he{std::max(1e-3f, (mx.x - mn.x) * 0.5f), std::max(1e-3f, (mx.y - mn.y) * 0.5f), std::max(1e-3f, (mx.z - mn.z) * 0.5f)};
  std::vector<uint32_t> oi;
  oi.reserve(d.idx.size());
  std::vector<char> scaled(nv, 0);
  for (auto& kv : comps) {
    Vec3 c{0, 0, 0}; int n = 0;
    for (size_t t : kv.second) for (int k = 0; k < 3; k++) { const VtxGlb& q = v[d.idx[t * 3 + k]]; c = c + Vec3(q.p[0], q.p[1], q.p[2]); n++; }
    c = c * (1.0f / std::max(1, n));
    Vec3 r{(c.x - ctr.x) / he.x, (c.y - ctr.y) / he.y, (c.z - ctr.z) / he.z};
    float rad = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
    float keep = rad < 0.6f ? innerKeep : outerKeep;
    if ((lodHash(kv.first * 2246822519u + 101) & 1023) >= keep * 1024) continue;
    for (size_t t : kv.second)
      for (int k = 0; k < 3; k++) {
        uint32_t src = d.idx[t * 3 + k];
        if (!scaled[src]) { scaled[src] = 1; for (int j = 0; j < 3; j++) v[src].p[j] = (&c.x)[j] + (v[src].p[j] - (&c.x)[j]) * grow; }
        oi.push_back(src);
      }
  }
  d.idx.swap(oi);
}

MeshData makeLod(const MeshData& d, bool cutout, bool under) {
  MeshData out;
  out.fmt = d.fmt;
  if (d.fmt != VFmt::GLB || d.idx.empty()) return out;
  const VtxGlb* v = (const VtxGlb*)d.verts.data();
  size_t nv = d.vertexCount, nt = d.idx.size() / 3;
  std::vector<VtxGlb> ov;
  std::vector<uint32_t> oi;
  if (cutout) {
    std::vector<uint32_t> parent(nv);
    for (size_t i = 0; i < nv; i++) parent[i] = (uint32_t)i;
    std::function<uint32_t(uint32_t)> find = [&](uint32_t x) { while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; } return x; };
    for (size_t t = 0; t < nt; t++) {
      uint32_t a = find(d.idx[t * 3]), b = find(d.idx[t * 3 + 1]), c = find(d.idx[t * 3 + 2]);
      parent[b] = a;
      parent[find(c)] = find(a);
    }
    std::unordered_map<uint32_t, std::vector<size_t>> comps;
    for (size_t t = 0; t < nt; t++) comps[find(d.idx[t * 3])].push_back(t);
    bool perTri = comps.size() < 24;
    if (perTri && under) return out;
    const float keep = under ? 0.62f : 0.45f, grow = under ? 1.2f : 1.35f;
    auto emitScaled = [&](const std::vector<size_t>& tris) {
      Vec3 c{0, 0, 0};
      int n = 0;
      for (size_t t : tris) for (int k = 0; k < 3; k++) { const VtxGlb& q = v[d.idx[t * 3 + k]]; c = c + Vec3(q.p[0], q.p[1], q.p[2]); n++; }
      c = c * (1.0f / std::max(1, n));
      std::unordered_map<uint32_t, uint32_t> remap;
      for (size_t t : tris)
        for (int k = 0; k < 3; k++) {
          uint32_t src = d.idx[t * 3 + k];
          auto it = remap.find(src);
          if (it == remap.end()) {
            VtxGlb q = v[src];
            for (int j = 0; j < 3; j++) q.p[j] = (&c.x)[j] + (q.p[j] - (&c.x)[j]) * grow;
            it = remap.emplace(src, (uint32_t)ov.size()).first;
            ov.push_back(q);
          }
          oi.push_back(it->second);
        }
    };
    if (perTri) {
      for (size_t t = 0; t < nt; t++)
        if ((lodHash((uint32_t)t * 2654435761u) & 1023) < keep * 1024) emitScaled({t});
    } else {
      for (auto& kv : comps)
        if ((lodHash(kv.first * 2654435761u + 17) & 1023) < keep * 1024) emitScaled(kv.second);
    }
  } else {
    Vec3 mn = d.bmin, mx = d.bmax;
    float ext = std::max(mx.x - mn.x, std::max(mx.y - mn.y, mx.z - mn.z));
    float cell = std::max(1e-3f, ext / 14.0f);
    std::unordered_map<uint64_t, uint32_t> cellIdx;
    std::vector<uint32_t> map(nv);
    for (size_t i = 0; i < nv; i++) {
      int64_t cx = (int64_t)std::floor((v[i].p[0] - mn.x) / cell), cy = (int64_t)std::floor((v[i].p[1] - mn.y) / cell), cz = (int64_t)std::floor((v[i].p[2] - mn.z) / cell);
      uint64_t key = (uint64_t)(cx & 0xFFFFF) | ((uint64_t)(cy & 0xFFFFF) << 20) | ((uint64_t)(cz & 0xFFFFF) << 40);
      auto it = cellIdx.find(key);
      if (it == cellIdx.end()) { it = cellIdx.emplace(key, (uint32_t)ov.size()).first; ov.push_back(v[i]); }
      map[i] = it->second;
    }
    for (size_t t = 0; t < nt; t++) {
      uint32_t a = map[d.idx[t * 3]], b = map[d.idx[t * 3 + 1]], c = map[d.idx[t * 3 + 2]];
      if (a == b || b == c || a == c) continue;
      oi.insert(oi.end(), {a, b, c});
    }
  }
  if (oi.empty()) return out;
  out.verts.resize(ov.size() * sizeof(VtxGlb));
  std::memcpy(out.verts.data(), ov.data(), out.verts.size());
  out.idx = std::move(oi);
  out.vertexCount = (int)ov.size();
  for (auto& q : ov) {
    out.bmin = {std::min(out.bmin.x, q.p[0]), std::min(out.bmin.y, q.p[1]), std::min(out.bmin.z, q.p[2])};
    out.bmax = {std::max(out.bmax.x, q.p[0]), std::max(out.bmax.y, q.p[1]), std::max(out.bmax.z, q.p[2])};
  }
  return out;
}
}  // namespace

namespace assets {

SeasonKind kindFor(const std::string& name, const std::string& layer) {
  std::string n = lower(name);
  if (n.find("bark") != std::string::npos) return SeasonKind::Bark;
  if (n.find("rock") != std::string::npos) return SeasonKind::Rock;
  if (n.find("mushroom") != std::string::npos) return SeasonKind::Mush;
  if (n.find("flower") != std::string::npos) return SeasonKind::Flower;
  if (n.find("grass") != std::string::npos) return SeasonKind::Grass;
  if (n.find("pine") != std::string::npos) return SeasonKind::Pine;
  if (n.rfind("leaves_", 0) == 0 || (n.find("leaves") != std::string::npos && layer == "tree")) return SeasonKind::Leaf;
  return SeasonKind::Plant;
}

void loadAll(const std::vector<AssetRequest>& reqs) {
  Loader L;
  std::vector<CpuAsset> cpu(reqs.size());
  for (size_t i = 0; i < reqs.size(); i++) cpu[i].req = reqs[i];
  unsigned nt = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
  {
    std::atomic<size_t> next{0};
    std::vector<std::thread> th;
    for (unsigned t = 0; t < nt; t++)
      th.emplace_back([&] {
        for (size_t i; (i = next++) < cpu.size();) L.loadOne(cpu[i]);
      });
    for (auto& t : th) t.join();
  }
  {
    std::atomic<size_t> next{0};
    std::vector<std::thread> th;
    for (unsigned t = 0; t < nt; t++)
      th.emplace_back([&] {
        for (size_t i; (i = next++) < L.pendingTex.size();) {
          auto& pt = L.pendingTex[i];
          L.decodeTex(pt.first, pt.second.first, pt.second.second);
          std::vector<uint8_t>().swap(pt.second.first);
        }
      });
    for (auto& t : th) t.join();
  }
  for (auto& kv : L.texJobs) {
    if (!kv.second->ready || g_textures.count(kv.first)) continue;
    g_textures[kv.first] = std::make_unique<Texture>(uploadPrepared(kv.second->levels, true));
    kv.second->levels = TexLevels();
  }
  for (auto& c : cpu) {
    if (!c.ok) {
      g_missing.push_back(c.req.file);
      continue;
    }
    auto a = std::make_unique<Asset>();
    a->file = c.req.file;
    for (auto& p : c.parts) {
      AssetPart ap;
      const CpuMat& lm = c.mats[p.matIndex];
      MeshData lodSrc = p.data;
      if (lm.cutout && (c.req.layer == "tree" || c.req.layer == "under")) {
        auto tj = L.texJobs.find(lm.texKey);
        if (tj != L.texJobs.end() && !tj->second->alpha.empty()) {
          int dropped = 0;
          int n = trimCards(p.data, tj->second->alpha, tj->second->aw, tj->second->ah, dropped);
          if (n && std::getenv("HW_TRIM")) std::printf("trim %s: %d cards trimmed (%d empty) area %.0f%%\n", c.req.file.c_str(), n, dropped, 100.0 * g_trimA / std::max(1e-9, g_trimQ));
        }
      }
      if (lm.cutout && c.req.layer == "tree" && c.req.file.find("Pine") == std::string::npos && !std::getenv("HW_NOTHIN")) thinCanopy(p.data, 0.3f, 0.7f, 1.18f);
      ap.mesh = uploadMesh(p.data);
      if (c.req.layer == "tree" || c.req.layer == "rock" || c.req.layer == "under") {
        MeshData ld = makeLod(lodSrc, lm.cutout, c.req.layer == "under");
        if (!ld.idx.empty()) ap.lod = uploadMesh(ld);
      }
      ap.local = p.local;
      ap.identity = p.identity;
      const CpuMat& m = c.mats[p.matIndex];
      ap.mat.name = m.name;
      ap.mat.base = m.base;
      ap.mat.cutout = m.cutout;
      ap.mat.kind = kindFor(m.name, c.req.layer);
      auto it = g_textures.find(m.texKey);
      ap.mat.tex = it != g_textures.end() ? it->second.get() : nullptr;
      a->tris += (int)(p.data.idx.empty() ? p.data.vertexCount : p.data.idx.size()) / 3;
      for (int k = 0; k < 8; k++) {
        Vec3 corner((k & 1) ? p.data.bmax.x : p.data.bmin.x, (k & 2) ? p.data.bmax.y : p.data.bmin.y, (k & 4) ? p.data.bmax.z : p.data.bmin.z);
        Vec3 w = p.local.transformPoint(corner);
        a->bmin = Vec3(std::min(a->bmin.x, w.x), std::min(a->bmin.y, w.y), std::min(a->bmin.z, w.z));
        a->bmax = Vec3(std::max(a->bmax.x, w.x), std::max(a->bmax.y, w.y), std::max(a->bmax.z, w.z));
      }
      a->parts.push_back(std::move(ap));
    }
    g_assets[c.req.file] = std::move(a);
  }
}

const Asset* get(const std::string& file) {
  auto it = g_assets.find(file);
  return it == g_assets.end() ? nullptr : it->second.get();
}

std::vector<std::string> missing() { return g_missing; }

void shutdown() {
  for (auto& a : g_assets)
    for (auto& p : a.second->parts) { p.mesh.destroy(); p.lod.destroy(); }
  g_assets.clear();
  for (auto& t : g_textures) t.second->destroy();
  g_textures.clear();
}

}  // namespace assets

// ---------------------------------------------------------------------------------------------
// Procedural trees: leaf-cluster textures, bark textures and branching geometry, built from math.
// ---------------------------------------------------------------------------------------------
namespace {

struct PRng {
  uint32_t s;
  explicit PRng(uint32_t seed) : s(seed * 2654435761u + 0x9e3779b9u) {}
  float f() { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.0f; }
  float r(float a, float b) { return a + (b - a) * f(); }
};

float phash(int x, int y, int seed) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 2147483647u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return ((h ^ (h >> 16)) & 0xffffff) / 16777216.0f;
}
float pnoise(float x, float y, int seed, int px = 0, int py = 0) {
  int ix = (int)std::floor(x), iy = (int)std::floor(y);
  float fx = x - ix, fy = y - iy;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  auto H = [&](int a, int b) { if (px) a = ((a % px) + px) % px; if (py) b = ((b % py) + py) % py; return phash(a, b, seed); };
  float a = H(ix, iy), b = H(ix + 1, iy), c = H(ix, iy + 1), d = H(ix + 1, iy + 1);
  return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}
float pfbm(float x, float y, int seed, int oct, int px = 0, int py = 0) {
  float s = 0, a = 0.5f, n = 0;
  for (int i = 0; i < oct; i++) { s += a * pnoise(x, y, seed + i * 31, px, py); n += a; x *= 2; y *= 2; px *= 2; py *= 2; a *= 0.5f; }
  return s / n;
}

enum TreeSp { T_OAK, T_MAPLE, T_BIRCH, T_CEDAR, T_RHODO, T_COUNT };

struct LeafImg {
  int N;
  std::vector<float> rgb, a;
  std::vector<int> order;
  LeafImg(int n) : N(n), rgb((size_t)n * n * 3, 0), a((size_t)n * n, 0), order((size_t)n * n, -1) {}
};

// half-width profile of a leaf at u in [0,1] (fraction of length)
float leafProfile(TreeSp sp, float u, float lat) {
  float s = std::sin(u * PI);
  switch (sp) {
    case T_OAK: {  // rounded lobes
      float lobe = 0.62f + 0.38f * std::fabs(std::sin(u * PI * 4.5f + 0.4f));
      return 0.34f * std::pow(s, 0.55f) * lobe * (1.0f - 0.25f * u);
    }
    case T_BIRCH: {  // ovate, pointed tip, serrated
      float w = 0.36f * std::pow(s, 0.7f) * (1.15f - 0.55f * u);
      return w * (1.0f + 0.07f * std::sin(u * 70.0f + lat));
    }
    case T_RHODO: return 0.2f * std::pow(s, 0.5f) * (1.0f + 0.15f * u);
    default: return 0.3f * s;
  }
}

void drawPixel(LeafImg& L, int x, int y, float r, float g, float b, int ord) {
  if (x < 0 || y < 0 || x >= L.N || y >= L.N) return;
  size_t k = (size_t)y * L.N + x;
  L.rgb[k * 3] = r; L.rgb[k * 3 + 1] = g; L.rgb[k * 3 + 2] = b; L.a[k] = 1; L.order[k] = ord;
}

void drawTwig(LeafImg& L, float x0, float y0, float x1, float y1, float w0, float w1, Color c, int ord) {
  float len = std::hypot(x1 - x0, y1 - y0);
  int steps = (int)(len * 2) + 1;
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / steps;
    float x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t, w = w0 + (w1 - w0) * t;
    for (int dy = (int)-w - 1; dy <= (int)w + 1; dy++)
      for (int dx = (int)-w - 1; dx <= (int)w + 1; dx++)
        if (dx * dx + dy * dy <= w * w) {
          float sh = 0.8f + 0.25f * (float)(-dx) / std::max(1.0f, w);
          drawPixel(L, (int)(x + dx), (int)(y + dy), c.r * sh, c.g * sh, c.b * sh, ord);
        }
  }
}

// one leaf: base at (bx,by), pointing angle ang (radians, 0 = up), length len px
void drawLeaf(LeafImg& L, TreeSp sp, float bx, float by, float ang, float len, Color base, float shade, int ord, PRng& R) {
  float dx = std::sin(ang), dy = -std::cos(ang);
  float px = -dy, py = dx;
  float maxW = len * 0.6f;
  int x0 = (int)std::floor(std::min(bx, bx + dx * len) - maxW), x1 = (int)std::ceil(std::max(bx, bx + dx * len) + maxW);
  int y0 = (int)std::floor(std::min(by, by + dy * len) - maxW), y1 = (int)std::ceil(std::max(by, by + dy * len) + maxW);
  float hueJ = R.r(-0.1f, 0.1f), lat = R.r(0, 6);
  shade *= R.r(0.82f, 1.15f);
  float tilt = R.r(-1, 1);  // which half faces the light
  for (int y = std::max(0, y0); y <= std::min(L.N - 1, y1); y++)
    for (int x = std::max(0, x0); x <= std::min(L.N - 1, x1); x++) {
      float rx = x + 0.5f - bx, ry = y + 0.5f - by;
      float along = rx * dx + ry * dy, across = rx * px + ry * py;
      float u = along / len;
      float hw;
      if (sp == T_MAPLE) {
        // palmate: polar star around the petiole tip
        float cx = along - len * 0.38f, cy = across;
        float r = std::hypot(cx, cy) / len, th = std::atan2(cy, cx);
        if (std::fabs(th) > 2.7f) continue;
        float lobes = 0.5f + 0.5f * std::pow(std::fabs(std::cos(th * 2.5f)), 0.6f);
        float R0 = 0.62f * lobes * (1.0f - 0.15f * std::fabs(th)) * (1.0f + 0.05f * std::sin(th * 22.0f + lat));
        if (r > R0) continue;
        u = 0.38f + r * std::cos(th);
        hw = R0;
        float vein = std::min(std::fabs(std::sin(th * 2.5f)) * r * len, 1e9f);
        float edge = 1.0f - r / R0;
        float sideL = (cy * tilt > 0) ? 1.06f : 0.92f;
        float k = (edge < 0.04f ? 0.55f : 1.0f) * shade * sideL * (0.86f + 0.22f * std::sqrt(edge)) * (vein < 1.3f ? 1.13f : 1.0f) * (0.92f + 0.16f * pfbm(x * 0.05f, y * 0.05f, ord, 3));
        drawPixel(L, x, y, base.r * k * (1 + hueJ), base.g * k, base.b * k * (1 - hueJ), ord);
        continue;
      }
      if (u < 0 || u > 1) continue;
      hw = leafProfile(sp, u, lat) * len;
      if (std::fabs(across) > hw) continue;
      float e = 1.0f - std::fabs(across) / std::max(hw, 0.5f);
      float rim = (std::fabs(across) > hw - 1.6f || u > 0.985f) ? 0.55f : 1.0f;
      float mid = std::fabs(across) < std::max(0.8f, len * 0.012f) ? 1.0f : 0.0f;
      float vt = u * 9.0f - std::fabs(across) / len * 4.0f;
      float vein = std::fabs(vt - std::round(vt)) < 0.06f ? 1.0f : 0.0f;
      float sideL = (across * tilt > 0) ? 1.07f : 0.9f;
      float gloss = sp == T_RHODO ? 0.12f * std::pow(e, 3.0f) * (across * tilt > 0) : 0.0f;
      float k = rim * shade * sideL * (0.84f + 0.24f * std::sqrt(e)) * (1.0f + 0.14f * mid + 0.07f * vein) * (0.92f + 0.16f * pfbm(x * 0.06f, y * 0.06f, ord, 3)) * (0.95f + 0.1f * u);
      drawPixel(L, x, y, std::min(1.0f, base.r * k * (1 + hueJ) + gloss), std::min(1.0f, base.g * k + gloss), std::min(1.0f, base.b * k * (1 - hueJ) + gloss), ord);
    }
}

void drawNeedleSpray(LeafImg& L, float bx, float by, float ang, float len, Color base, float shade, int ord, PRng& R) {
  float dx = std::sin(ang), dy = -std::cos(ang);
  Color twig = Color::hex("4a3524");
  drawTwig(L, bx, by, bx + dx * len, by + dy * len, 2.2f, 1.0f, twig, ord);
  int n = (int)(len / 3.2f);
  for (int i = 0; i < n; i++) {
    float t = (float)i / n;
    float x = bx + dx * len * t, y = by + dy * len * t;
    for (int s = -1; s <= 1; s += 2) {
      float a2 = ang + s * R.r(0.5f, 0.9f) - 0.1f * s;
      float nl = len * 0.16f * (1.0f - 0.55f * t) * R.r(0.8f, 1.2f);
      float k = shade * R.r(0.85f, 1.12f) * (0.9f + 0.2f * t);
      Color c{base.r * k, base.g * k, base.b * k};
      float ex = x + std::sin(a2) * nl, ey = y - std::cos(a2) * nl;
      int st = (int)(nl * 1.5f) + 1;
      for (int j = 0; j <= st; j++) {
        float u = (float)j / st;
        float kk = 0.85f + 0.3f * u;
        drawPixel(L, (int)(x + (ex - x) * u), (int)(y + (ey - y) * u), c.r * kk, c.g * kk, c.b * kk, ord);
        drawPixel(L, (int)(x + (ex - x) * u) + 1, (int)(y + (ey - y) * u), c.r * kk * 0.9f, c.g * kk * 0.9f, c.b * kk * 0.9f, ord);
      }
    }
  }
}

Color g_leafOverride{-1, 0, 0};
int g_flowers = 0;
LeafImg makeLeafCluster(TreeSp sp, int N) {
  LeafImg L(N);
  PRng R(101 + sp * 7 + g_flowers * 3);
  Color twig = Color::hex("4b3626");
  Color base = g_leafOverride.r >= 0 ? g_leafOverride : sp == T_OAK ? Color::hex("4f7a2c") : sp == T_MAPLE ? Color::hex("5d8a2e") : sp == T_BIRCH ? Color::hex("74a03a") : sp == T_CEDAR ? Color::hex("5a8a4c") : Color::hex("2f5a26");
  float cx = N * 0.5f, by = N * 0.97f;
  int ord = 0;
  if (sp == T_CEDAR) {
    drawTwig(L, cx, by, cx, N * 0.08f, 4.0f, 1.5f, twig, ord++);
    for (int i = 0; i < 9; i++) {
      float t = 0.1f + i * 0.095f;
      float y = by + (N * 0.08f - by) * t;
      for (int s = -1; s <= 1; s += 2) drawNeedleSpray(L, cx, y, s * R.r(0.85f, 1.15f), N * 0.5f * (1.0f - 0.5f * t), base, 0.8f + 0.25f * t, ord++, R);
    }
    drawNeedleSpray(L, cx, N * 0.12f, 0, N * 0.12f, base, 1.05f, ord++, R);
  } else if (sp == T_RHODO) {
    drawTwig(L, cx, by, cx - N * 0.12f, N * 0.42f, 4, 2.5f, twig, ord++);
    drawTwig(L, cx, by, cx + N * 0.16f, N * 0.36f, 4, 2.5f, twig, ord++);
    float rx[3] = {cx - N * 0.13f, cx + N * 0.17f, cx}, ry[3] = {N * 0.42f, N * 0.36f, N * 0.6f};
    for (int r = 0; r < 3; r++)
      for (int i = 0; i < 10; i++) {
        float a = i * 0.63f + R.r(-0.15f, 0.15f) + r;
        float len = N * R.r(0.22f, 0.3f);
        drawLeaf(L, sp, rx[r], ry[r], a, len, base, 0.78f + 0.3f * ((i % 5) / 4.0f), ord++, R);
      }
  } else {
    // a fan of twigs from the base, each carrying alternating leaves: fills the whole card
    int twigs = sp == T_MAPLE ? 4 : 5;
    int per = sp == T_OAK ? 4 : sp == T_MAPLE ? 2 : 7;
    float lenA = sp == T_OAK ? 0.22f : sp == T_MAPLE ? 0.34f : 0.15f;
    std::vector<std::array<float, 4>> leaves;
    for (int w = 0; w < twigs; w++) {
      float fan = (twigs == 1 ? 0.0f : -0.75f + 1.5f * w / (twigs - 1)) + R.r(-0.08f, 0.08f);
      float tl = N * (0.78f - 0.28f * std::fabs(fan));
      float ex = cx + std::sin(fan) * tl, ey = by - std::cos(fan) * tl;
      drawTwig(L, cx, by, ex, ey, 3.5f, 1.2f, twig, ord++);
      for (int i = 0; i < per; i++) {
        float t = 0.3f + 0.7f * (i + 1) / per;
        float x = cx + (ex - cx) * t, y = by + (ey - by) * t;
        float side = (i % 2) ? 1.0f : -1.0f;
        float a = fan + side * R.r(0.5f, 1.1f) * (1.0f - 0.4f * t);
        float len = N * lenA * R.r(0.8f, 1.15f);
        leaves.push_back({x, y, a, len});
      }
      leaves.push_back({ex, ey, fan + R.r(-0.2f, 0.2f), N * lenA});
    }
    for (size_t i = 0; i < leaves.size(); i++) {
      size_t j = (i * 7919) % leaves.size();
      auto& l = leaves[j];
      drawLeaf(L, sp, l[0], l[1], l[2], l[3], base, 0.72f + 0.38f * (float)i / leaves.size(), ord++, R);
    }
    for (int f = 0; f < g_flowers; f++) {  // small five-petal blossoms over the foliage
      float fx = R.r(N * 0.12f, N * 0.88f), fy = R.r(N * 0.1f, N * 0.7f), fr = N * R.r(0.025f, 0.04f);
      Color pc = (f % 3 == 0) ? Color::hex("f3f0f4") : Color::hex("e98ab6");
      for (int y = (int)(fy - fr * 1.3f); y <= (int)(fy + fr * 1.3f); y++)
        for (int x = (int)(fx - fr * 1.3f); x <= (int)(fx + fr * 1.3f); x++) {
          float dx = x - fx, dy = y - fy, r = std::hypot(dx, dy) / fr, th = std::atan2(dy, dx);
          float pet = 0.75f + 0.45f * std::fabs(std::cos(th * 2.5f));
          if (r > pet) continue;
          float k = r < 0.28f ? 0.0f : 0.85f + 0.2f * (1.0f - r / pet);
          if (r < 0.28f) drawPixel(L, x, y, 0.95f, 0.78f, 0.2f, ord);
          else drawPixel(L, x, y, pc.r * k, pc.g * k, pc.b * k, ord);
        }
      ord++;
    }
  }
  return L;
}

LeafImg makeSingleLeaf(TreeSp sp, int N) {
  LeafImg L(N);
  PRng R(301 + sp);
  Color base = sp == T_OAK ? Color::hex("4f7a2c") : sp == T_MAPLE ? Color::hex("5d8a2e") : sp == T_BIRCH ? Color::hex("74a03a") : Color::hex("2f5a26");
  drawTwig(L, N * 0.5f, N * 0.99f, N * 0.5f, N * 0.9f, 2.5f, 2.0f, Color::hex("5a4a2a"), 0);
  drawLeaf(L, sp, N * 0.5f, N * 0.92f, 0, N * 0.9f, base, 1.0f, 1, R);
  return L;
}

std::vector<uint8_t> toRGBA(const LeafImg& L) {
  std::vector<uint8_t> o((size_t)L.N * L.N * 4);
  for (size_t k = 0; k < (size_t)L.N * L.N; k++) {
    for (int c = 0; c < 3; c++) o[k * 4 + c] = (uint8_t)(255 * std::sqrt(std::min(1.0f, std::max(0.0f, L.rgb[k * 3 + c]))));
    o[k * 4 + 3] = L.a[k] > 0.5f ? 255 : 0;
  }
  return o;
}

std::vector<uint8_t> makeBark(TreeSp sp, int N) {
  std::vector<uint8_t> o((size_t)N * N * 4);
  Color dark = sp == T_BIRCH ? Color::hex("2a2522") : sp == T_CEDAR ? Color::hex("3a2418") : Color::hex("2b2118");
  Color light = sp == T_BIRCH ? Color::hex("e4e0d6") : sp == T_CEDAR ? Color::hex("7a5038") : sp == T_MAPLE ? Color::hex("6b6258") : Color::hex("5e4c3c");
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      float u = (float)x / N, v = (float)y / N;
      float val;
      if (sp == T_BIRCH) {
        float len = pfbm(u * 4, v * 40, 7, 3, 4, 40);
        float lent = (pnoise(u * 6, v * 70, 9, 6, 70) > 0.78f && len > 0.45f) ? 0.15f : 1.0f;
        float patchd = pfbm(u * 3, v * 5, 11, 4, 3, 5) > 0.68f ? 0.45f : 1.0f;
        val = (0.85f + 0.15f * pfbm(u * 16, v * 16, 3, 3, 16, 16)) * lent * patchd;
      } else {
        float fis = pfbm(u * 10 + pfbm(u * 3, v * 2, 5, 3, 3, 2) * 1.2f, v * 1.5f, 4, 4, 10, 2);
        float ridge = 1.0f - std::fabs(fis * 2 - 1);
        float plates = pfbm(u * 6, v * 9, 8, 3, 6, 9);
        val = std::pow(ridge, 1.6f) * (0.65f + 0.5f * plates) * (0.85f + 0.3f * pfbm(u * 40, v * 40, 2, 2, 40, 40));
      }
      float moss = sp != T_BIRCH ? std::max(0.0f, pfbm(u * 5, v * 5, 13, 3, 5, 5) - 0.62f) * 2.5f : 0.0f;
      Color c = dark.lerp(light, std::min(1.0f, std::max(0.0f, val))).lerp(Color::hex("4f6b2a"), std::min(0.6f, moss));
      size_t k = ((size_t)y * N + x) * 4;
      o[k] = (uint8_t)(255 * std::sqrt(std::min(1.0f, c.r)));
      o[k + 1] = (uint8_t)(255 * std::sqrt(std::min(1.0f, c.g)));
      o[k + 2] = (uint8_t)(255 * std::sqrt(std::min(1.0f, c.b)));
      o[k + 3] = 255;
    }
  return o;
}

struct GeoB {
  std::vector<VtxGlb> v;
  std::vector<uint32_t> idx;
  void vert(Vec3 p, Vec3 n, float u, float vv, float ao) {
    VtxGlb o;
    o.p[0] = p.x; o.p[1] = p.y; o.p[2] = p.z;
    n = n.normalized();
    o.n[0] = (int8_t)(n.x * 127); o.n[1] = (int8_t)(n.y * 127); o.n[2] = (int8_t)(n.z * 127); o.n[3] = 0;
    o.uv[0] = floatToHalf(u); o.uv[1] = floatToHalf(vv);
    uint8_t c = (uint8_t)(255 * std::sqrt(std::min(1.0f, std::max(0.05f, ao))));
    o.c = c | (c << 8) | (c << 16) | (255u << 24);
    v.push_back(o);
  }
  MeshData data() const {
    MeshData d;
    d.fmt = VFmt::GLB;
    d.vertexCount = (int)v.size();
    d.verts.resize(v.size() * sizeof(VtxGlb));
    std::memcpy(d.verts.data(), v.data(), d.verts.size());
    d.idx = idx;
    for (auto& q : v) {
      d.bmin = {std::min(d.bmin.x, q.p[0]), std::min(d.bmin.y, q.p[1]), std::min(d.bmin.z, q.p[2])};
      d.bmax = {std::max(d.bmax.x, q.p[0]), std::max(d.bmax.y, q.p[1]), std::max(d.bmax.z, q.p[2])};
    }
    return d;
  }
};

uint32_t packCol(Color c, float k) {
  auto b = [&](float v) { return (uint32_t)(255 * std::sqrt(std::min(1.0f, std::max(0.0f, v * k)))); };
  return b(c.r) | (b(c.g) << 8) | (b(c.b) << 16) | (255u << 24);
}
Vec3 rotAxis(Vec3 v, Vec3 k, float a) {
  float c = std::cos(a), s = std::sin(a);
  return v * c + k.cross(v) * s + k * (k.dot(v) * (1 - c));
}
Vec3 anyPerp(Vec3 d) { return std::fabs(d.y) < 0.9f ? d.cross(Vec3(0, 1, 0)).normalized() : d.cross(Vec3(1, 0, 0)).normalized(); }

void tube(GeoB& g, Vec3 a, Vec3 b, float ra, float rb, int sides, float v0, float vScale) {
  Vec3 d = (b - a).normalized(), p = anyPerp(d), q = d.cross(p);
  uint32_t base = (uint32_t)g.v.size();
  float len = (b - a).length();
  for (int i = 0; i <= sides; i++) {
    float t = (float)i / sides * TAU;
    Vec3 n = p * std::cos(t) + q * std::sin(t);
    g.vert(a + n * ra, n, (float)i / sides, v0, 1.0f);
    g.vert(b + n * rb, n, (float)i / sides, v0 + len * vScale, 1.0f);
  }
  for (int i = 0; i < sides; i++) {
    uint32_t k = base + i * 2;
    g.idx.insert(g.idx.end(), {k, k + 2, k + 1, k + 1, k + 2, k + 3});
  }
}

// branch built from several segments that bend progressively (gravity sag + random kinks)
Vec3 curvedTube(GeoB& g, Vec3 a, Vec3 dir, float len, float ra, float rb, int segs, int sides, PRng& R, float sag, float kink, Vec3* endDir = nullptr) {
  Vec3 p = a, d = dir.normalized();
  for (int i = 0; i < segs; i++) {
    float t0 = (float)i / segs, t1 = (float)(i + 1) / segs;
    Vec3 nd = (d + Vec3(R.r(-kink, kink), R.r(-kink, kink) * 0.6f - sag, R.r(-kink, kink))).normalized();
    Vec3 e = p + nd * (len / segs);
    tube(g, p, e, ra + (rb - ra) * t0, ra + (rb - ra) * t1, sides, 0, 0.5f);
    p = e; d = nd;
  }
  if (endDir) *endDir = d;
  return p;
}

struct SpecDef {
  float trunkH, trunkR, crownStart, branchAng, lenK, upBias, cardSize, cardsPer;
  int prim, sec;
};
const SpecDef SPD[T_COUNT] = {
    {4.2f, 0.58f, 0.45f, 0.95f, 0.62f, 0.25f, 1.7f, 4.2f, 7, 4},   // oak: wide spreading crown
    {4.8f, 0.48f, 0.5f, 0.7f, 0.55f, 0.45f, 1.8f, 4.0f, 6, 4},     // maple: rounded, upright
    {9.0f, 0.3f, 0.35f, 0.55f, 0.32f, 0.2f, 1.3f, 3.6f, 11, 3},   // birch: tall, slim, light crown
    {12.5f, 0.5f, 0.12f, 1.45f, 0.0f, -0.15f, 2.3f, 2.0f, 16, 0},  // cedar: conical whorls
    {1.6f, 0.23f, 0.2f, 0.75f, 0.9f, 0.5f, 1.3f, 4.0f, 5, 3},      // rhododendron: shrubby small tree
};

struct Built { MeshData bark, leaves; };

LeafImg makeSingleLeaf(TreeSp sp, int N);
Built growTree(TreeSp sp, uint32_t seed, bool indiv) {
  const SpecDef& S = SPD[sp];
  PRng R(seed);
  GeoB bark, leaf;
  std::vector<std::pair<Vec3, Vec3>> tips;  // (position, branch direction) for leaf cards
  Vec3 crownC{0, 0, 0};
  // trunk with gentle bends
  Vec3 p{0, -0.3f, 0}, d{0, 1, 0};
  int tSeg = sp == T_CEDAR ? 5 : 3;
  float tLen = S.trunkH / tSeg, r = S.trunkR, vacc = 0;
  std::vector<std::pair<Vec3, Vec3>> trunkPts;
  for (int i = 0; i < tSeg; i++) {
    Vec3 nd = (d + Vec3(R.r(-0.12f, 0.12f), 0, R.r(-0.12f, 0.12f))).normalized();
    if (sp == T_CEDAR) nd = Vec3(0, 1, 0);
    Vec3 e = p + nd * tLen;
    float r2 = r * (sp == T_CEDAR ? 0.78f : 0.82f);
    tube(bark, p, e, r, r2, sp == T_RHODO ? 5 : 7, vacc, 0.35f);
    trunkPts.push_back({p, e});
    vacc += tLen * 0.35f; p = e; d = nd; r = r2;
  }
  Vec3 top = p;
  float crownH = sp == T_CEDAR ? 0.0f : S.trunkH * (sp == T_BIRCH ? 0.45f : 0.9f);
  crownC = top + Vec3(0, crownH * 0.4f, 0);
  if (sp == T_CEDAR) {
    // whorls of near-horizontal branches, shorter toward the top
    for (int w = 0; w < S.prim; w++) {
      float t = 0.12f + 0.86f * w / (S.prim - 1);
      Vec3 at = Vec3(0, -0.3f + S.trunkH * t, 0);
      float bl = 3.4f * (1.0f - t) + 0.5f;
      int arms = 5;
      for (int k = 0; k < arms; k++) {
        float az = k * TAU / arms + w * 0.9f + R.r(-0.3f, 0.3f);
        Vec3 bd = Vec3(std::cos(az), -0.18f + 0.1f * t, std::sin(az)).normalized();
        Vec3 bdEnd;
        Vec3 e = curvedTube(bark, at, bd, bl, 0.14f * (1.2f - t), 0.05f, 5, 4, R, 0.14f, 0.2f, &bdEnd);
        int nc = std::max(1, (int)(bl / 0.75f));
        for (int c = 0; c < nc; c++) tips.push_back({at + (e - at) * (0.35f + 0.65f * (c + 1) / nc), bdEnd});
      }
    }
    tips.push_back({top + Vec3(0, 0.4f, 0), Vec3(0, 1, 0)});
    crownC = Vec3(0, S.trunkH * 0.45f, 0);
  } else {
    if (sp == T_RHODO) {
      // multi-stem shrub: extra stems from the base
      for (int k = 0; k < 2; k++) {
        float az = R.r(0, TAU);
        Vec3 sd = Vec3(std::cos(az) * 0.45f, 1, std::sin(az) * 0.45f).normalized();
        Vec3 e = Vec3(0, -0.3f, 0) + sd * 1.8f;
        tube(bark, Vec3(0, -0.3f, 0), e, 0.14f, 0.08f, 5, 0, 0.5f);
        trunkPts.push_back({Vec3(0, -0.3f, 0), e});
      }
    }
    for (int i = 0; i < S.prim; i++) {
      // pick an attachment point on the upper trunk
      auto& seg = trunkPts[trunkPts.size() - 1 - (i % std::min<size_t>(trunkPts.size(), sp == T_BIRCH ? 3 : 2))];
      float t = R.r(S.crownStart, 1.0f);
      Vec3 at = seg.first + (seg.second - seg.first) * t;
      if (sp == T_BIRCH) at = Vec3(0, -0.3f + S.trunkH * R.r(0.35f, 0.98f), 0);
      float az = i * 2.39996f + R.r(-0.3f, 0.3f);
      Vec3 out{std::cos(az), 0, std::sin(az)};
      Vec3 bd = (out * std::sin(S.branchAng) + Vec3(0, std::cos(S.branchAng), 0)).normalized();
      bd = (bd + Vec3(0, S.upBias * 0.3f, 0)).normalized();
      float bl = (S.trunkH * S.lenK + 1.6f) * R.r(0.8f, 1.15f);
      if (sp == T_BIRCH) bl *= 1.0f - 0.5f * (at.y / S.trunkH);
      float br = S.trunkR * (sp == T_BIRCH ? 0.46f : 0.62f);
      Vec3 bdEnd;
      float sag = sp == T_BIRCH ? 0.14f : sp == T_OAK ? 0.09f : 0.05f;
      Vec3 e = curvedTube(bark, at, bd, bl, br, br * 0.55f, 4, 5, R, sag, 0.22f, &bdEnd);
      if (S.sec == 0) { tips.push_back({e, bd}); continue; }
      for (int j = 0; j < S.sec; j++) {
        float tt = 0.45f + 0.55f * (j + 1) / S.sec;
        Vec3 sa = at + (e - at) * tt;
        Vec3 sd = rotAxis(bd, anyPerp(bd), R.r(0.5f, 0.9f) * ((j % 2) ? 1 : -1));
        sd = rotAxis(sd, bd, R.r(0, TAU));
        if (sp == T_BIRCH) sd = (sd + Vec3(0, -0.6f, 0)).normalized();  // weeping tips
        else sd = (sd + Vec3(0, S.upBias, 0)).normalized();
        float sl = bl * R.r(0.4f, 0.6f);
        Vec3 sdEnd;
        Vec3 smid = curvedTube(bark, sa, sd, sl * 0.5f, br * 0.52f, br * 0.38f, 2, 4, R, sag, 0.3f, &sdEnd);
        Vec3 se = curvedTube(bark, smid, sdEnd, sl * 0.5f, br * 0.38f, br * 0.22f, 2, 4, R, sag, 0.3f, &sdEnd);
        tips.push_back({smid, sd});
        tips.push_back({se, sdEnd});
      }
      tips.push_back({e, bdEnd});
    }
  }
  // leaf cards: oriented by branch direction, normals from the crown centre (soft rounded shading)
  float crownR = 0.1f;
  for (auto& t : tips) crownR = std::max(crownR, (t.first - crownC).length());
  if (indiv && sp != T_CEDAR) {
    // individual leaves on thin twigs: every leaf is its own quad attached to a twig
    float leafL = sp == T_OAK ? 0.62f : sp == T_MAPLE ? 0.72f : sp == T_BIRCH ? 0.45f : 0.52f;
    int twigsPer = sp == T_OAK ? 6 : 5, leavesPer = sp == T_BIRCH ? 8 : sp == T_MAPLE ? 7 : 9;
    for (auto& t : tips) {
      for (int w = 0; w < twigsPer; w++) {
        Vec3 td = rotAxis(t.second, anyPerp(t.second), R.r(0.4f, 1.1f));
        td = rotAxis(td, t.second, R.r(0, TAU));
        td = (td + Vec3(0, sp == T_BIRCH ? -0.5f : 0.15f, 0)).normalized();
        float tl = R.r(0.7f, 1.2f) * (sp == T_BIRCH ? 1.3f : 1.0f);
        Vec3 a = t.first;
        size_t v0 = bark.v.size();
        Vec3 tdEnd;
        Vec3 e = curvedTube(bark, a, td, tl, 0.009f, 0.004f, 2, 3, R, 0.12f, 0.25f, &tdEnd);
        for (size_t q = v0; q < bark.v.size(); q++) bark.v[q].c = packCol(Color::hex("8a7258"), 1.0f);  // dark twig, not white bark
        for (int l = 0; l < leavesPer; l++) {
          float u = 0.08f + 0.92f * (l + 1) / leavesPer;
          Vec3 at = a + (e - a) * u;
          float sideSign = (l % 2) ? 1.0f : -1.0f;
          Vec3 sideAx = td.cross(Vec3(0, 1, 0));
          if (sideAx.length() < 0.1f) sideAx = Vec3(1, 0, 0);
          sideAx = sideAx.normalized();
          Vec3 up = (td * 0.55f + sideAx * sideSign * 0.8f + Vec3(0, R.r(-0.1f, 0.35f), 0)).normalized();
          if (l == leavesPer - 1) up = (td + Vec3(0, 0.2f, 0)).normalized();
          Vec3 side = up.cross(Vec3(0, 1, 0));
          if (side.length() < 0.1f) side = Vec3(1, 0, 0);
          side = rotAxis(side.normalized(), up, R.r(-0.5f, 0.5f));
          float sz = leafL * R.r(0.8f, 1.15f);
          Vec3 nn = ((at - crownC).normalized() + Vec3(0, 0.6f, 0)).normalized();
          float ao = 0.6f + 0.45f * std::min(1.0f, (at - crownC).length() / crownR);
          uint32_t k = (uint32_t)leaf.v.size();
          float hw = sz * 0.5f;
          leaf.vert(at - side * hw, nn, 0, 1, ao);
          leaf.vert(at + side * hw, nn, 1, 1, ao);
          leaf.vert(at + side * hw + up * sz, nn, 1, 0, ao);
          leaf.vert(at - side * hw + up * sz, nn, 0, 0, ao);
          leaf.idx.insert(leaf.idx.end(), {k, k + 1, k + 2, k, k + 2, k + 3});
        }
      }
    }
    return {bark.data(), leaf.data()};
  }
  bool broad = sp != T_CEDAR;
  for (auto& t : tips) {
    // broadleaf: more, smaller cluster cards, each turned to face outward from the crown so it is never seen edge-on
    float per = broad ? S.cardsPer * 1.9f : S.cardsPer;
    int n = (int)per;
    if (R.f() < per - n) n++;
    for (int c = 0; c < n; c++) {
      Vec3 up = (t.second + Vec3(R.r(-0.5f, 0.5f), R.r(-0.2f, 0.6f), R.r(-0.5f, 0.5f))).normalized();
      if (sp == T_CEDAR) up = (Vec3(t.second.x, R.r(-0.35f, 0.15f), t.second.z) + Vec3(R.r(-0.4f, 0.4f), 0, R.r(-0.4f, 0.4f))).normalized();
      float sz = S.cardSize * R.r(0.8f, 1.2f) * (broad ? 0.68f : 1.0f);
      float spread = sp == T_CEDAR ? 0.3f : S.cardSize * 0.75f;
      Vec3 off(R.r(-spread, spread), R.r(-spread * 0.7f, spread * 0.7f), R.r(-spread, spread));
      Vec3 side = anyPerp(up);
      side = rotAxis(side, up, R.r(0, TAU));
      if (broad) {
        Vec3 radial = t.first + off - crownC;
        radial.y *= 0.35f;
        radial = radial.length() > 0.01f ? radial.normalized() : Vec3(1, 0, 0);
        // twig base points inward/down, leafy end outward/up: like a real spray of leaves
        up = (radial * 0.75f + Vec3(0, 0.75f, 0) + t.second * 0.3f + Vec3(R.r(-0.25f, 0.25f), R.r(-0.15f, 0.2f), R.r(-0.25f, 0.25f))).normalized();
        side = up.cross(radial);
        if (side.length() < 0.05f) side = anyPerp(up);
        side = rotAxis(side.normalized(), up, R.r(-0.45f, 0.45f) + ((c % 2) ? 0.9f : 0.0f));  // every other card crossed for volume
      }
      Vec3 base = t.first + off - up * (sz * 0.15f);
      Vec3 nn = (base + up * sz * 0.5f - crownC);
      float dist = nn.length() / crownR;
      nn = (nn.normalized() + Vec3(0, 0.6f, 0)).normalized();
      float ao = (sp == T_CEDAR ? 0.75f : 0.6f) + 0.45f * std::min(1.0f, dist);
      uint32_t k = (uint32_t)leaf.v.size();
      float hw = sz * 0.5f;
      leaf.vert(base - side * hw, nn, 0, 1, ao);
      leaf.vert(base + side * hw, nn, 1, 1, ao);
      leaf.vert(base + side * hw + up * sz, nn, 1, 0, ao);
      leaf.vert(base - side * hw + up * sz, nn, 0, 0, ao);
      leaf.idx.insert(leaf.idx.end(), {k, k + 1, k + 2, k, k + 2, k + 3});
    }
  }
  if (broad) {
    // underside canopy: near-horizontal cluster cards below each tip so the crown reads full from beneath
    for (auto& t : tips) {
      for (int c = 0; c < 2; c++) {
        Vec3 radial = t.first - crownC;
        radial.y = 0;
        radial = radial.length() > 0.01f ? radial.normalized() : Vec3(1, 0, 0);
        radial = rotAxis(radial, Vec3(0, 1, 0), R.r(-0.9f, 0.9f));
        Vec3 up = (radial + Vec3(0, R.r(-0.25f, 0.1f), 0)).normalized();   // lies almost flat, leaf tips outward
        Vec3 side = up.cross(Vec3(0, 1, 0)).normalized();
        side = rotAxis(side, up, R.r(-0.35f, 0.35f));
        float sz = S.cardSize * R.r(0.7f, 1.0f) * 0.75f;
        Vec3 base = t.first + Vec3(R.r(-0.4f, 0.4f), -R.r(0.25f, 0.7f) * S.cardSize, R.r(-0.4f, 0.4f)) - up * (sz * 0.45f);
        Vec3 nn = ((base - crownC).normalized() + Vec3(0, 0.6f, 0)).normalized();
        float ao = 0.55f + 0.3f * std::min(1.0f, (base - crownC).length() / crownR);  // underside sits in shade
        uint32_t k = (uint32_t)leaf.v.size();
        float hw = sz * 0.5f;
        leaf.vert(base - side * hw, nn, 0, 1, ao);
        leaf.vert(base + side * hw, nn, 1, 1, ao);
        leaf.vert(base + side * hw + up * sz, nn, 1, 0, ao);
        leaf.vert(base - side * hw + up * sz, nn, 0, 0, ao);
        leaf.idx.insert(leaf.idx.end(), {k, k + 1, k + 2, k, k + 2, k + 3});
      }
    }
  }
  return {bark.data(), leaf.data()};
}

std::vector<std::unique_ptr<Texture>> g_procTex;

// ---- small plants: texture painters ----
LeafImg makeFern(int N) {
  LeafImg L(N);
  PRng R(5);
  Color g = Color::hex("4f8a34"), rach = Color::hex("5c6b2a");
  int ord = 0;
  float x0 = N * 0.5f, y0 = N * 0.98f;
  int pairs = 16;
  std::vector<std::array<float, 2>> pts;
  for (int i = 0; i <= 40; i++) { float t = i / 40.0f; pts.push_back({x0 + std::sin(t * 1.2f) * N * 0.06f, y0 - t * N * 0.93f}); }
  for (int i = 0; i < 40; i++) drawTwig(L, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], 3.2f - 2.2f * i / 40.0f, 3.2f - 2.2f * (i + 1) / 40.0f, rach, ord);
  ord++;
  for (int i = 0; i < pairs; i++) {
    float t = 0.08f + 0.9f * i / pairs;
    auto& p = pts[(int)(t * 40)];
    float len = N * 0.4f * std::sin(std::min(1.0f, t * 1.15f) * PI * 0.9f + 0.25f) * (1.0f - 0.6f * t) + N * 0.03f;
    for (int s = -1; s <= 1; s += 2) {
      float ang = s * (1.25f - 0.35f * t);
      float dx = std::sin(ang), dy = -std::cos(ang);
      int nl = std::max(3, (int)(len / 9));
      for (int k = 0; k < nl; k++) {  // pinnules along each pinna
        float u = (k + 0.5f) / nl;
        float px = p[0] + dx * len * u, py = p[1] + dy * len * u;
        float pl = len * 0.22f * (1.0f - 0.6f * u) + 3;
        drawLeaf(L, T_RHODO, px, py, ang - s * 0.9f, pl, g, 0.85f + 0.2f * u, ord++, R);
        drawLeaf(L, T_RHODO, px, py, ang + s * 0.9f - s * 3.14f + s * 3.14f * 0.0f + s * 1.8f, pl, g, 0.8f + 0.2f * u, ord++, R);
      }
    }
  }
  return L;
}
LeafImg makeBroadPlant(int N, Color base, int leaves, float lenK, uint32_t seed) {
  LeafImg L(N);
  PRng R(seed);
  int ord = 0;
  for (int i = 0; i < leaves; i++) {
    float a = -1.2f + 2.4f * (i + 0.5f) / leaves + R.r(-0.12f, 0.12f);
    float len = N * lenK * R.r(0.75f, 1.1f) * (1.0f - 0.3f * std::fabs(a));
    drawTwig(L, N * 0.5f, N * 0.98f, N * 0.5f + std::sin(a) * len * 0.35f, N * 0.98f - std::cos(a) * len * 0.35f, 3, 2, Color::hex("5a7a2e"), ord++);
    drawLeaf(L, T_BIRCH, N * 0.5f + std::sin(a) * len * 0.3f, N * 0.98f - std::cos(a) * len * 0.3f, a, len * 0.75f, base, 0.75f + 0.35f * ((i * 3) % leaves) / (float)leaves, ord++, R);
  }
  return L;
}
void drawBloom(LeafImg& L, float cx, float cy, float r, Color pet, Color mid, int petals, int ord, PRng& R) {
  // individual petals: each a tapered ellipse from the centre, drawn back-to-front with small gaps
  float rot = R.r(0, TAU);
  float width = petals >= 10 ? 0.16f : petals >= 6 ? 0.3f : 0.42f;  // daisies thin, poppies/violets broad
  float squash = R.r(0.8f, 1.0f);                                    // slight perspective tilt
  for (int pi = 0; pi < petals; pi++) {
    float a = rot + pi * TAU / petals + R.r(-0.08f, 0.08f);
    float len = r * R.r(0.85f, 1.05f);
    float dx = std::cos(a), dy = std::sin(a) * squash;
    float shade = 0.82f + 0.22f * ((pi * 5) % petals) / (float)petals;
    int x0 = (int)(cx - r - 2), x1 = (int)(cx + r + 2), y0 = (int)(cy - r - 2), y1 = (int)(cy + r + 2);
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        float px = x + 0.5f - cx, py = (y + 0.5f - cy) / squash;
        float along = px * dx + py * std::sin(a), across = -px * std::sin(a) + py * dx;
        float u = along / len;
        if (u < 0.12f || u > 1.0f) continue;
        float hw = len * width * std::pow(std::sin(std::min(1.0f, u * 1.1f) * PI * 0.92f), 0.7f) * (petals < 6 ? 1.0f + 0.3f * u : 1.0f);
        if (petals < 10) hw *= 1.0f - 0.25f * std::pow(std::max(0.0f, u - 0.85f) / 0.15f, 2.0f) * (std::fabs(across) < hw * 0.25f);  // notch at tip
        if (std::fabs(across) > hw) continue;
        float e = 1.0f - std::fabs(across) / hw;
        float vein = std::fabs(across) < std::max(0.6f, hw * 0.08f) ? 0.9f : 1.0f;
        float k = shade * (0.7f + 0.38f * u) * (0.85f + 0.2f * std::sqrt(e)) * vein * (0.94f + 0.12f * pnoise(x * 0.4f, y * 0.4f, pi));
        drawPixel(L, x, y, std::min(1.0f, pet.r * k), std::min(1.0f, pet.g * k), std::min(1.0f, pet.b * k), ord);
      }
  }
  float cr = r * (petals >= 10 ? 0.3f : 0.22f);
  for (int y = (int)(cy - cr - 1); y <= (int)(cy + cr + 1); y++)
    for (int x = (int)(cx - cr - 1); x <= (int)(cx + cr + 1); x++) {
      float d = std::hypot(x + 0.5f - cx, (y + 0.5f - cy) / squash) / cr;
      if (d > 1) continue;
      float seed = phash(x / 2, y / 2, 7) > 0.5f ? 1.08f : 0.86f;
      float k = (1.05f - 0.35f * d) * seed;
      drawPixel(L, x, y, std::min(1.0f, mid.r * k), std::min(1.0f, mid.g * k), std::min(1.0f, mid.b * k), ord);
    }
}
LeafImg makeFlowers(int N, Color pet, Color mid, int stems, int petals, uint32_t seed) {
  LeafImg L(N);
  PRng R(seed);
  int ord = 0;
  Color stemc = Color::hex("4e7a2a"), leafc = Color::hex("5b8f36");
  for (int i = 0; i < stems; i++) {
    float bx = N * 0.5f + R.r(-0.08f, 0.08f) * N, tx = N * (0.15f + 0.7f * (i + 0.5f) / stems) + R.r(-0.05f, 0.05f) * N;
    float ty = N * R.r(0.1f, 0.4f);
    drawTwig(L, bx, N * 0.99f, tx, ty, 2.2f, 1.6f, stemc, ord++);
    for (int k = 0; k < 2; k++) {
      float t = R.r(0.45f, 0.85f);
      float sx = bx + (tx - bx) * t, sy = N * 0.99f + (ty - N * 0.99f) * t;
      drawLeaf(L, T_RHODO, sx, sy, (k ? 1 : -1) * R.r(0.6f, 1.0f), N * R.r(0.1f, 0.16f), leafc, 0.9f, ord++, R);
    }
    drawBloom(L, tx, ty, N * R.r(0.06f, 0.09f), pet, mid, petals, ord++, R);
  }
  return L;
}

struct PlantTex { const Texture* tex; std::vector<uint8_t> alpha; };
PlantTex uploadPlant(const LeafImg& L, std::vector<std::unique_ptr<Texture>>& store) {
  std::vector<uint8_t> rgba = toRGBA(L);
  if (const char* dd = std::getenv("HW_DUMPLEAF")) {
    static int n = 0;
    std::string fn = std::string(dd) + "/plant" + std::to_string(n++) + ".ppm";
    if (FILE* f = std::fopen(fn.c_str(), "wb")) { std::fprintf(f, "P6 %d %d 255\n", L.N, L.N); for (size_t k = 0; k < (size_t)L.N * L.N; k++) { uint8_t c[3]; for (int q = 0; q < 3; q++) c[q] = rgba[k * 4 + 3] ? rgba[k * 4 + q] : 200; std::fwrite(c, 1, 3, f); } std::fclose(f); }
  }
  store.push_back(std::make_unique<Texture>(uploadTextureRGBA(rgba.data(), L.N, L.N, true, true, true, false)));
  PlantTex p{store.back().get(), std::vector<uint8_t>(256 * 256)};
  int sc = L.N / 256;
  for (int y = 0; y < 256; y++)
    for (int x = 0; x < 256; x++) {
      int m = 0;
      for (int dy = 0; dy < sc; dy++) for (int dx = 0; dx < sc; dx++) m = std::max(m, (int)rgba[(((size_t)(y * sc + dy) * L.N) + x * sc + dx) * 4 + 3]);
      p.alpha[(size_t)y * 256 + x] = (uint8_t)m;
    }
  return p;
}

// rosette of upright cards; arch>0 bends each card into two segments (ferns)
MeshData buildRosette(uint32_t seed, int cards, float H, float W, float tilt, float arch) {
  PRng R(seed);
  GeoB g;
  for (int i = 0; i < cards; i++) {
    float az = i * TAU / cards + R.r(-0.3f, 0.3f);
    Vec3 out{std::cos(az), 0, std::sin(az)}, side{-std::sin(az), 0, std::cos(az)};
    float h = H * R.r(0.8f, 1.1f), w = W * R.r(0.85f, 1.15f), tl = tilt + R.r(-0.12f, 0.12f);
    Vec3 nn = (out * 0.5f + Vec3(0, 1, 0)).normalized();
    Vec3 b0 = out * (w * 0.05f);
    if (arch <= 0) {
      Vec3 up = (Vec3(0, 1, 0) * std::cos(tl) + out * std::sin(tl));
      uint32_t k = (uint32_t)g.v.size();
      g.vert(b0 - side * w * 0.5f, nn, 0, 1, 0.75f);
      g.vert(b0 + side * w * 0.5f, nn, 1, 1, 0.75f);
      g.vert(b0 + side * w * 0.5f + up * h, nn, 1, 0, 1.0f);
      g.vert(b0 - side * w * 0.5f + up * h, nn, 0, 0, 1.0f);
      g.idx.insert(g.idx.end(), {k, k + 1, k + 2, k, k + 2, k + 3});
    } else {
      Vec3 u1 = (Vec3(0, 1, 0) * std::cos(tl) + out * std::sin(tl)), u2 = (Vec3(0, 1, 0) * std::cos(tl + arch) + out * std::sin(tl + arch));
      Vec3 m = b0 + u1 * h * 0.5f, t = m + u2 * h * 0.5f;
      uint32_t k = (uint32_t)g.v.size();
      g.vert(b0 - side * w * 0.5f, nn, 0, 1, 0.7f);
      g.vert(b0 + side * w * 0.5f, nn, 1, 1, 0.7f);
      g.vert(m + side * w * 0.5f, nn, 1, 0.5f, 0.9f);
      g.vert(m - side * w * 0.5f, nn, 0, 0.5f, 0.9f);
      g.vert(t + side * w * 0.5f, nn, 1, 0, 1.0f);
      g.vert(t - side * w * 0.5f, nn, 0, 0, 1.0f);
      g.idx.insert(g.idx.end(), {k, k + 1, k + 2, k, k + 2, k + 3, k + 3, k + 2, k + 4, k + 3, k + 4, k + 5});
    }
  }
  return g.data();
}

MeshData buildToadstool(uint32_t seed, float H, Color cap, MeshData& stemOut) {
  PRng R(seed);
  GeoB s, c;
  for (int m = 0; m < 3; m++) {
    float ox = m ? R.r(-0.12f, 0.12f) : 0, oz = m ? R.r(-0.12f, 0.12f) : 0, sc = m ? R.r(0.5f, 0.75f) : 1.0f;
    Vec3 b{ox, -0.02f, oz}, t{ox + R.r(-0.02f, 0.02f), H * 0.7f * sc, oz};
    tube(s, b, t, 0.035f * sc, 0.028f * sc, 6, 0, 1.0f);
    float cr = 0.11f * sc;
    uint32_t k = (uint32_t)c.v.size();
    c.vert(t + Vec3(0, H * 0.32f * sc, 0), Vec3(0, 1, 0), 0, 0, 1);
    for (int i = 0; i <= 10; i++) {
      float a = i * TAU / 10;
      Vec3 dir{std::cos(a), 0, std::sin(a)};
      c.vert(t + dir * cr * 0.75f + Vec3(0, H * 0.22f * sc, 0), (dir + Vec3(0, 1.4f, 0)).normalized(), 0, 0, 1);
      c.vert(t + dir * cr - Vec3(0, H * 0.02f, 0), (dir + Vec3(0, 0.3f, 0)).normalized(), 0, 0, 0.85f);
    }
    for (int i = 0; i < 10; i++) {
      uint32_t a = k + 1 + i * 2, b2 = a + 2;
      c.idx.insert(c.idx.end(), {k, b2, a, a, b2, b2 + 1, a, b2 + 1, a + 1});
    }
  }
  for (auto& v : s.v) v.c = packCol(Color::hex("e9e1cf"), 1.0f);
  for (size_t i = 0; i < c.v.size(); i++) c.v[i].c = packCol(cap, 0.85f + 0.3f * phash((int)i, 3, 5));
  stemOut = s.data();
  return c.data();
}

// ---- geometric flowers: every petal and leaf is its own quad ----
// atlas (512x512): petal (white, tinted by vertex colour) top-left, leaf top-right, solid white bottom
LeafImg makePartsAtlas(Color leafCol, uint32_t seed) {
  LeafImg L(512);
  PRng R(seed);
  for (int y = 0; y < 256; y++)
    for (int x = 0; x < 256; x++) {
      float u = 1.0f - (y + 0.5f) / 256.0f, across = (x + 0.5f - 128) / 128.0f;
      float hw = 0.8f * std::pow(std::sin(std::min(1.0f, u * 1.08f) * PI * 0.95f), 0.6f) * (0.55f + 0.45f * u);
      if (u > 0.88f) hw *= 1.0f - 0.35f * (std::fabs(across) < 0.12f) * (u - 0.88f) / 0.12f;
      if (u < 0.02f || std::fabs(across) > hw) continue;
      float e = 1.0f - std::fabs(across) / hw;
      float ang = std::atan2(across, u + 0.15f);
      float vein = 0.92f + 0.08f * std::cos(ang * 26.0f);
      float k = (0.62f + 0.42f * std::pow(u, 0.6f)) * (0.82f + 0.2f * std::sqrt(e)) * vein;
      drawPixel(L, x, y, k, k, k, 0);
    }
  drawLeaf(L, T_BIRCH, 384, 252, 0, 248, leafCol, 1.0f, 1, R);
  for (int y = 300; y < 512; y++)
    for (int x = 0; x < 512; x++) { float k = 0.92f + 0.08f * phash(x, y, 3); drawPixel(L, x, y, k, k, k, 2); }
  return L;
}

struct PartsGeo {
  GeoB g;
  void quad(Vec3 base, Vec3 up, Vec3 side, float len, float wid, Vec3 n, float u0, float u1, float v0, float v1, uint32_t col0, uint32_t col1) {
    uint32_t k = (uint32_t)g.v.size();
    g.vert(base - side * wid * 0.5f, n, u0, v1, 1); g.v.back().c = col0;
    g.vert(base + side * wid * 0.5f, n, u1, v1, 1); g.v.back().c = col0;
    g.vert(base + side * wid * 0.5f + up * len, n, u1, v0, 1); g.v.back().c = col1;
    g.vert(base - side * wid * 0.5f + up * len, n, u0, v0, 1); g.v.back().c = col1;
    g.idx.insert(g.idx.end(), {k, k + 1, k + 2, k, k + 2, k + 3});
  }
  void leaf(Vec3 base, Vec3 dir, float len, float wid, float droop, PRng& R, float bright) {
    Vec3 side = Vec3(-dir.z, 0, dir.x).normalized();
    if (side.length() < 0.1f) side = Vec3(1, 0, 0);
    Vec3 d1 = (dir + Vec3(0, 0.6f, 0)).normalized(), d2 = (dir + Vec3(0, -droop, 0)).normalized();
    Vec3 n1 = d1.cross(side).normalized(); if (n1.y < 0) n1 = n1 * -1.0f;
    Vec3 n2 = d2.cross(side).normalized(); if (n2.y < 0) n2 = n2 * -1.0f;
    uint32_t c = packCol(Color{1, 1, 1}, bright * R.r(0.88f, 1.08f));
    Vec3 m = base + d1 * len * 0.5f;
    quad(base, d1, side, len * 0.5f, wid, n1, 0.5f, 1.0f, 0.25f, 0.5f, c, c);
    quad(m, d2, side, len * 0.5f, wid, n2, 0.5f, 1.0f, 0.0f, 0.25f, c, c);
  }
  void solidTube(Vec3 a, Vec3 b, float ra, float rb, Color col) {
    size_t s0 = g.v.size();
    tube(g, a, b, ra, rb, 4, 0, 1);
    for (size_t i = s0; i < g.v.size(); i++) { g.v[i].uv[0] = floatToHalf(0.5f); g.v[i].uv[1] = floatToHalf(0.8f); g.v[i].c = packCol(col, 0.9f + 0.15f * (i % 2)); }
  }
  void bloom(Vec3 c, int petals, float plen, float pwid, float cup, Color pc, Color cc, float cr, PRng& R) {
    float rot = R.r(0, TAU);
    for (int layer = 0; layer < (petals >= 10 ? 2 : 1); layer++)
      for (int i = 0; i < petals; i++) {
        float a = rot + (i + layer * 0.5f) * TAU / petals + R.r(-0.06f, 0.06f);
        Vec3 out{std::cos(a), 0, std::sin(a)};
        float tilt = cup + R.r(-0.08f, 0.08f) + layer * 0.15f;
        Vec3 up = (out * std::cos(tilt) + Vec3(0, std::sin(tilt), 0)).normalized();
        Vec3 side{-out.z, 0, out.x};
        Vec3 n = up.cross(side).normalized(); if (n.y < 0) n = n * -1.0f;
        float k = R.r(0.88f, 1.06f);
        quad(c + up * (cr * 0.6f) + Vec3(0, layer * 0.002f, 0), up, side, plen * R.r(0.9f, 1.08f), pwid, n, 0.0f, 0.5f, 0.0f, 0.5f, packCol(pc, 0.8f * k), packCol(pc, 1.05f * k));
      }
    uint32_t k0 = (uint32_t)g.v.size();
    g.vert(c + Vec3(0, cr * 0.45f, 0), Vec3(0, 1, 0), 0.5f, 0.8f, 1); g.v.back().c = packCol(cc, 1.1f);
    for (int i = 0; i <= 8; i++) {
      float a = i * TAU / 8;
      g.vert(c + Vec3(std::cos(a) * cr, 0, std::sin(a) * cr), Vec3(std::cos(a), 1.2f, std::sin(a)).normalized(), 0.5f, 0.8f, 1);
      g.v.back().c = packCol(cc, 0.75f);
    }
    for (int i = 0; i < 8; i++) g.idx.insert(g.idx.end(), {k0, k0 + 2 + i, k0 + 1 + i});
  }
};

MeshData buildFlowerPlant(uint32_t seed, int stems, float H, int petals, float plen, float pwid, float cup, Color pc, Color cc, float cr, int leavesPer) {
  PRng R(seed);
  PartsGeo P;
  Color stemc = Color::hex("4f7d2b");
  for (int s = 0; s < stems; s++) {
    float az = R.r(0, TAU), lean = R.r(0.05f, 0.25f);
    Vec3 base{R.r(-0.04f, 0.04f) * H, 0, R.r(-0.04f, 0.04f) * H};
    float h = H * R.r(0.7f, 1.0f);
    Vec3 top = base + Vec3(std::cos(az) * lean * h, h, std::sin(az) * lean * h);
    Vec3 mid = (base + top) * 0.5f + Vec3(std::cos(az + 1) * 0.03f * H, 0, std::sin(az + 1) * 0.03f * H);
    P.solidTube(base, mid, H * 0.012f, H * 0.01f, stemc);
    P.solidTube(mid, top, H * 0.01f, H * 0.008f, stemc);
    for (int l = 0; l < leavesPer; l++) {
      float t = R.r(0.1f, 0.55f), la = R.r(0, TAU);
      Vec3 at = base + (mid - base) * (t * 2);
      P.leaf(at, Vec3(std::cos(la), 0, std::sin(la)), H * R.r(0.22f, 0.32f), H * R.r(0.08f, 0.11f), 0.5f, R, 0.95f);
    }
    P.bloom(top, petals, plen * H, pwid * H, cup, pc, cc, cr * H, R);
  }
  return P.g.data();
}

MeshData buildLeafRosette(uint32_t seed, int leaves, float len, float wid, float droop, float bright) {
  PRng R(seed);
  PartsGeo P;
  for (int i = 0; i < leaves; i++) {
    float a = i * 2.39996f + R.r(-0.2f, 0.2f);
    float l = len * R.r(0.75f, 1.1f) * (i < leaves / 3 ? 0.75f : 1.0f);
    P.leaf(Vec3(0, 0.01f + i * 0.004f, 0), Vec3(std::cos(a), 0, std::sin(a)), l, wid * R.r(0.85f, 1.1f), droop * R.r(0.7f, 1.2f), R, bright);
  }
  return P.g.data();
}

MeshData buildBushLeaves(uint32_t seed, float R0, float H, int cards, float cardSize) {
  PRng R(seed);
  GeoB g;
  Vec3 c{0, H * 0.45f, 0};
  for (int i = 0; i < cards; i++) {
    float az = R.r(0, TAU), el = std::acos(R.r(0.05f, 1.0f));
    Vec3 dir{std::sin(el) * std::cos(az), std::cos(el), std::sin(el) * std::sin(az)};
    Vec3 pos = Vec3(dir.x * R0 * R.r(0.45f, 1.0f), std::max(0.05f, dir.y * H * R.r(0.5f, 0.95f)), dir.z * R0 * R.r(0.45f, 1.0f));
    Vec3 up = (dir + Vec3(0, 0.7f, 0) + Vec3(R.r(-0.3f, 0.3f), 0, R.r(-0.3f, 0.3f))).normalized();
    Vec3 side = rotAxis(anyPerp(up), up, R.r(0, TAU));
    float sz = cardSize * R.r(0.8f, 1.2f);
    Vec3 base = pos - up * sz * 0.3f;
    Vec3 nn = ((pos - c).normalized() + Vec3(0, 0.7f, 0)).normalized();
    float ao = 0.6f + 0.45f * std::min(1.0f, (pos - c).length() / R0);
    uint32_t k = (uint32_t)g.v.size();
    float hw = sz * 0.5f;
    g.vert(base - side * hw, nn, 0, 1, ao);
    g.vert(base + side * hw, nn, 1, 1, ao);
    g.vert(base + side * hw + up * sz, nn, 1, 0, ao);
    g.vert(base - side * hw + up * sz, nn, 0, 0, ao);
    g.idx.insert(g.idx.end(), {k, k + 1, k + 2, k, k + 2, k + 3});
  }
  return g.data();
}


// Boulder: displaced icosphere, flattened, slightly sunk
MeshData buildRock(uint32_t seed) {
  std::vector<Vec3> P;
  std::vector<uint32_t> I;
  float t = (1 + std::sqrt(5.0f)) / 2;
  Vec3 v0[12] = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
  for (auto& v : v0) P.push_back(v.normalized());
  uint32_t f0[60] = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};
  I.assign(f0, f0 + 60);
  for (int s = 0; s < 2; s++) {
    std::map<uint64_t, uint32_t> mid;
    auto M = [&](uint32_t a, uint32_t b) {
      uint64_t key = a < b ? ((uint64_t)a << 32) | b : ((uint64_t)b << 32) | a;
      auto it = mid.find(key);
      if (it != mid.end()) return it->second;
      P.push_back(((P[a] + P[b]) * 0.5f).normalized());
      return mid[key] = (uint32_t)P.size() - 1;
    };
    std::vector<uint32_t> J;
    for (size_t i = 0; i < I.size(); i += 3) {
      uint32_t a = I[i], b = I[i + 1], c = I[i + 2], ab = M(a, b), bc = M(b, c), ca = M(c, a);
      J.insert(J.end(), {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca});
    }
    I.swap(J);
  }
  for (auto& p : P) {
    float d = 0.75f + 0.35f * pfbm(p.x * 1.7f + 3, p.z * 1.7f + p.y * 1.3f, (int)seed, 3);
    float facet = std::floor(pfbm(p.x * 2.2f, p.y * 2.2f + p.z, (int)seed + 9, 2) * 4) / 4;  // fractured planes
    d *= 0.9f + 0.12f * facet;
    p = Vec3(p.x * d * 1.25f, p.y * d * 0.72f, p.z * d * 1.0f);
    if (p.y < -0.1f) p.y = -0.1f + (p.y + 0.1f) * 0.3f;
    p.y += 0.25f;
  }
  // flat-shaded: unique vertices per face
  GeoB g;
  Color c = Color::hex("8a857c");
  for (size_t i = 0; i < I.size(); i += 3) {
    Vec3 a = P[I[i]], b = P[I[i + 1]], d = P[I[i + 2]];
    Vec3 n = (b - a).cross(d - a).normalized();
    float k = 0.85f + 0.2f * pfbm(a.x * 3, a.z * 3, 5, 2);
    uint32_t base = (uint32_t)g.v.size();
    for (Vec3 q : {a, b, d}) { g.vert(q, n, 0, 0, 1); g.v.back().c = packCol(c, k); }
    g.idx.insert(g.idx.end(), {base, base + 1, base + 2});
  }
  return g.data();
}

// Bracket fungus (Laetiporus) on a small mossy stump
void buildFungus(MeshData& stump, MeshData& shelves) {
  GeoB s, f;
  tube(s, Vec3(0, -0.1f, 0), Vec3(0, 0.45f, 0), 0.2f, 0.17f, 7, 0, 1.0f);
  for (auto& v : s.v) v.c = packCol(Color::hex("5b4634"), 1.0f);
  PRng R(77);
  for (int k = 0; k < 5; k++) {
    float az = R.r(-1.2f, 1.2f), y = 0.1f + k * 0.075f, rr = R.r(0.16f, 0.24f);
    Color col = Color::hex(k % 2 ? "f2a43a" : "e8852a");
    uint32_t c0 = (uint32_t)f.v.size();
    Vec3 ctr{std::sin(az) * 0.17f, y, std::cos(az) * 0.17f};
    f.vert(ctr + Vec3(0, 0.03f, 0), Vec3(0, 1, 0), 0, 0, 1);
    f.v.back().c = packCol(col, 1.05f);
    for (int i = 0; i <= 8; i++) {
      float a = az - 1.3f + 2.6f * i / 8;
      Vec3 o{std::sin(a) * (0.17f + rr), y - 0.02f, std::cos(a) * (0.17f + rr)};
      f.vert(o, (o - Vec3(0, y - 0.3f, 0)).normalized(), 0, 0, 1);
      f.v.back().c = packCol(Color::hex("f7d27a"), 1.0f);
    }
    for (int i = 0; i < 8; i++) f.idx.insert(f.idx.end(), {c0, c0 + 1 + i, c0 + 2 + i});
  }
  stump = s.data();
  shelves = f.data();
}

}  // namespace

namespace assets {

std::vector<uint8_t> loadImageRGBA(const std::string& rel, int& w, int& h) {
  std::vector<uint8_t> enc, out;
  w = h = 0;
  if (!platform::readFile(platform::assetPath(rel), enc) || enc.empty()) return out;
  int ch;
  uint8_t* px = stbi_load_from_memory(enc.data(), (int)enc.size(), &w, &h, &ch, 4);
  if (!px) { w = h = 0; return out; }
  out.assign(px, px + (size_t)w * h * 4);
  stbi_image_free(px);
  return out;
}

std::vector<std::string> procOverrides() { return {"Bush.glb", "Bush with Flowers.glb", "Rock Medium.glb", "Mushroom Laetiporus.glb", "Fern.glb", "Plant.glb", "Plant Big.glb", "Plant-xH5gNlQxAZ.glb", "Flower Group.glb", "Flower Group-LqTljN6Wg2.glb", "Flower Single.glb", "Mushroom.glb", "Flower Petal.glb", "Flower Petal-LqvxG9OBOU.glb", "Flower Petal-eVE0j49ux9.glb", "Flower Petal-niuBUEJdvM.glb", "Flower Petal-tzG4JcqYWs.glb"}; }
std::vector<std::string> procTreeFiles() { return {"ptree:oak", "ptree:maple", "ptree:birch", "ptree:Pine-cedar", "ptree:rhodo"}; }

// Leaf-cluster card texture built from the single-leaf photo: a fan of twigs carrying many copies of the
// photo leaf, back leaves darker. One card then reads as dozens of real leaves (the GTA-style illusion).
static float halfToFloatA(uint16_t h) {
  uint32_t s = (h >> 15) & 1, e = (h >> 10) & 31, m = h & 1023;
  float f = e == 0 ? m / 1024.0f * std::pow(2.0f, -14.0f) : (1 + m / 1024.0f) * std::pow(2.0f, (float)e - 15);
  return s ? -f : f;
}
std::vector<uint8_t> composeCluster(const std::vector<uint8_t>& ph, int pw, int phh, int N, TreeSp sp, uint32_t seed) {
  std::vector<uint8_t> out((size_t)N * N * 4, 0);
  PRng R(seed);
  auto put = [&](int x, int y, float r, float g, float b) {
    if (x < 0 || y < 0 || x >= N || y >= N) return;
    uint8_t* o = &out[((size_t)y * N + x) * 4];
    o[0] = (uint8_t)std::min(255.0f, r); o[1] = (uint8_t)std::min(255.0f, g); o[2] = (uint8_t)std::min(255.0f, b); o[3] = 255;
  };
  auto line = [&](float x0, float y0, float x1, float y1, float w0, float w1) {
    int steps = (int)(std::hypot(x1 - x0, y1 - y0) * 2) + 1;
    for (int i = 0; i <= steps; i++) {
      float t = (float)i / steps, x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t, w = w0 + (w1 - w0) * t;
      for (int dy = (int)-w; dy <= (int)w; dy++) for (int dx = (int)-w; dx <= (int)w; dx++) if (dx * dx + dy * dy <= w * w) put((int)x + dx, (int)y + dy, 74, 54, 38);
    }
  };
  struct LeafAt { float x, y, ang, sz; };
  std::vector<LeafAt> leaves;
  float bx = N * 0.5f, by = N * 0.97f;
  int twigs = sp == T_BIRCH ? 6 : 7;
  for (int w = 0; w < twigs; w++) {
    float fan = -0.95f + 1.9f * w / (twigs - 1) + R.r(-0.1f, 0.1f);
    float len = N * R.r(0.62f, 0.8f) * (1.0f - 0.25f * std::fabs(fan));
    float ex = bx + std::sin(fan) * len, ey = by - std::cos(fan) * len;
    line(bx, by, ex, ey, 3.5f, 1.2f);
    int per = sp == T_BIRCH ? 9 : 8;
    for (int l = 0; l < per; l++) {
      float u = 0.25f + 0.75f * (l + 1) / per;
      float x = bx + (ex - bx) * u, y = by + (ey - by) * u;
      float sideSign = (l % 2) ? 1.0f : -1.0f;
      float ang = fan + sideSign * R.r(0.6f, 1.0f);
      if (l == per - 1) ang = fan;
      leaves.push_back({x, y, ang, N * R.r(0.14f, 0.19f) * (sp == T_BIRCH ? 0.85f : 1.0f)});
    }
  }
  // back-to-front: shuffle so overlaps vary, earlier leaves drawn darker (inner foliage)
  for (size_t i = leaves.size(); i > 1; i--) std::swap(leaves[i - 1], leaves[(size_t)(R.f() * i) % i]);
  for (size_t li = 0; li < leaves.size(); li++) {
    const LeafAt& L = leaves[li];
    float shade = 0.62f + 0.45f * li / leaves.size();
    float ca = std::cos(L.ang), sa = std::sin(L.ang);
    float hwid = L.sz * 0.5f, len = L.sz;
    int r = (int)(len + 2);
    for (int dy = -r; dy <= r; dy++)
      for (int dx = -r; dx <= r; dx++) {
        // leaf space: v along the leaf (0 at stem -> 1 at tip), u across
        float lx = dx * ca + dy * sa, ly = -(-dx * sa + dy * ca);
        float u = lx / hwid * 0.5f + 0.5f, v = ly / len;
        if (u < 0 || u > 1 || v < 0 || v > 1) continue;
        int px = std::min(pw - 1, (int)(u * pw)), py = std::min(phh - 1, (int)((1 - v) * phh));
        const uint8_t* c = &ph[((size_t)py * pw + px) * 4];
        if (c[3] < 128) continue;
        put((int)L.x + dx, (int)L.y + dy, c[0] * shade, c[1] * shade, c[2] * shade);
      }
  }
  return out;
}

void buildProcTrees() {
  const char* matLeaf[T_COUNT] = {"leaves_oak", "leaves_maple", "leaves_birch", "pine_needles_cedar", "leaves_rhodo"};
  auto files = procTreeFiles();
  long long totalTris = 0;
  for (int s = 0; s < T_COUNT; s++) {
    TreeSp sp = (TreeSp)s;
    int N = 512;
    const char* leafPhoto[T_COUNT] = {"textures/leaf_broad.png", "textures/leaf_broad2.png", "textures/leaf_birch.png", "textures/leaf_cedar.png", "textures/leaf_broad.png"};
    int pw = 0, ph = 0;
    std::vector<uint8_t> photo = loadImageRGBA(leafPhoto[s], pw, ph);
    LeafImg L = photo.empty() ? (sp == T_CEDAR ? makeLeafCluster(sp, N) : makeSingleLeaf(sp, 256)) : LeafImg(2);
    std::vector<uint8_t> rgba = photo.empty() ? toRGBA(L) : photo;
    if (!photo.empty()) { L.N = pw; if (sp == T_CEDAR && pw != N) N = pw; }
    std::vector<uint8_t> cluster;
    int texW = 0;
    if (sp != T_CEDAR) {
      // atlas 1024x512: left = the single photo leaf (near trees, every leaf its own quad), right = leaf cluster (mid range)
      int sw = photo.empty() ? L.N : pw, sh = photo.empty() ? L.N : ph;
      cluster = composeCluster(rgba, sw, sh, 512, sp, 900 + s);
      std::vector<uint8_t> atlas((size_t)1024 * 512 * 4, 0);
      for (int y = 0; y < 512; y++)
        for (int x = 0; x < 512; x++) {
          const uint8_t* a = &rgba[(((size_t)(y * sh / 512)) * sw + x * sw / 512) * 4];
          std::memcpy(&atlas[((size_t)y * 1024 + x) * 4], a, 4);
          std::memcpy(&atlas[((size_t)y * 1024 + 512 + x) * 4], &cluster[((size_t)y * 512 + x) * 4], 4);
        }
      rgba = cluster;  // trimming alpha below reads the cluster half
      N = 512;
      texW = 1024;
      g_procTex.push_back(std::make_unique<Texture>(uploadTextureRGBA(atlas.data(), 1024, 512, true, true, true, false)));
    }
    if (const char* dumpDir = std::getenv("HW_DUMPLEAF")) {
      std::string fn = std::string(dumpDir) + "/leaf" + std::to_string(s) + ".ppm";
      if (FILE* f = std::fopen(fn.c_str(), "wb")) { std::fprintf(f, "P6 %d %d 255\n", N, N); for (size_t k = 0; k < (size_t)N * N; k++) { uint8_t c[3]; for (int q = 0; q < 3; q++) c[q] = rgba[k * 4 + 3] ? rgba[k * 4 + q] : 200; std::fwrite(c, 1, 3, f); } std::fclose(f); }
    }
    if (!texW) g_procTex.push_back(std::make_unique<Texture>(uploadTextureRGBA(rgba.data(), L.N, L.N, true, true, true, false)));
    const Texture* leafTex = g_procTex.back().get();
    const char* barkPhoto[T_COUNT] = {"textures/bark_oak.jpg", "textures/bark_maple.jpg", "", "textures/bark_cedar.jpg", "textures/bark_rhodo.jpg"};
    int bw = 256, bh = 256;
    std::vector<uint8_t> brk = barkPhoto[s][0] ? loadImageRGBA(barkPhoto[s], bw, bh) : std::vector<uint8_t>();
    if (brk.empty()) { brk = makeBark(sp, 256); bw = bh = 256; }
    g_procTex.push_back(std::make_unique<Texture>(uploadTextureRGBA(brk.data(), bw, bh, true, true, false, true)));
    const Texture* barkTex = g_procTex.back().get();
    Built B = growTree(sp, 7771 + s * 131, false);
    // trim leaf cards to the visible leaf area (8-sided hulls) using a 256px alpha map
    std::vector<uint8_t> alpha(256 * 256);
    for (int y = 0; y < 256; y++)
      for (int x = 0; x < 256; x++) {
        int m = 0;
        for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) m = std::max(m, (int)rgba[(((size_t)(y * 2 + dy) * N) + x * 2 + dx) * 4 + 3]);
        alpha[(size_t)y * 256 + x] = (uint8_t)m;
      }
    MeshData lodSrc = B.leaves;
    int dropped = 0;
    trimCards(B.leaves, alpha, 256, 256, dropped);
    if (texW) {
      auto remap = [](MeshData& m, float sc, float off) {
        for (int i = 0; i < m.vertexCount; i++) {
          VtxGlb* v = reinterpret_cast<VtxGlb*>(m.verts.data()) + i;
          v->uv[0] = floatToHalf(halfToFloatA(v->uv[0]) * sc + off);
        }
      };
      remap(B.leaves, 0.5f, 0.5f);
    }
    auto a = std::make_unique<Asset>();
    a->file = files[s];
    for (int part = 0; part < 2; part++) {
      // broadleaf: near = individual leaves on twigs, mid = full leaf-cluster cards
      MeshData& md = part == 0 ? B.bark : B.leaves;
      if (md.idx.empty()) continue;
      AssetPart ap;
      ap.mesh = uploadMesh(md);
      MeshData ld = part == 0 ? makeLod(md, false, false) : makeLod(lodSrc, true, false);
      if (texW && part == 1) { MeshData t = ld; for (int i = 0; i < t.vertexCount; i++) { VtxGlb* v = reinterpret_cast<VtxGlb*>(t.verts.data()) + i; v->uv[0] = floatToHalf(halfToFloatA(v->uv[0]) * 0.5f + 0.5f); } ld = t; }
      if (!ld.idx.empty()) ap.lod = uploadMesh(ld);
      ap.mat.name = part == 0 ? "bark_" + std::to_string(s) : matLeaf[s];
      ap.mat.cutout = part == 1;
      ap.mat.base = Color{1, 1, 1};
      ap.mat.tex = part == 0 ? barkTex : leafTex;
      ap.mat.kind = kindFor(ap.mat.name, "tree");
      a->tris += (int)md.idx.size() / 3;
      a->bmin = Vec3(std::min(a->bmin.x, md.bmin.x), std::min(a->bmin.y, md.bmin.y), std::min(a->bmin.z, md.bmin.z));
      a->bmax = Vec3(std::max(a->bmax.x, md.bmax.x), std::max(a->bmax.y, md.bmax.y), std::max(a->bmax.z, md.bmax.z));
      a->parts.push_back(std::move(ap));
    }
    totalTris += a->tris;
    if (std::getenv("HW_TRIS")) std::fprintf(stderr, "proctree %s: %d tris, h %.1f\n", files[s].c_str(), a->tris, a->bmax.y);
    g_assets[files[s]] = std::move(a);
  }
  // ---- procedural undergrowth & rocks (replace heavy GLBs) ----
  auto addAsset = [&](const std::string& name, std::vector<std::pair<MeshData, AssetMaterial>> parts, bool under, const std::vector<uint8_t>* alpha) {
    auto a = std::make_unique<Asset>();
    a->file = name;
    for (auto& pm : parts) {
      MeshData& md = pm.first;
      MeshData lodSrc = md;
      int dropped = 0;
      if (pm.second.cutout && alpha) trimCards(md, *alpha, 256, 256, dropped);
      AssetPart ap;
      ap.mesh = uploadMesh(md);
      MeshData ld = makeLod(lodSrc, pm.second.cutout, under);
      if (!ld.idx.empty()) ap.lod = uploadMesh(ld);
      ap.mat = pm.second;
      a->tris += (int)md.idx.size() / 3;
      a->bmin = Vec3(std::min(a->bmin.x, md.bmin.x), std::min(a->bmin.y, md.bmin.y), std::min(a->bmin.z, md.bmin.z));
      a->bmax = Vec3(std::max(a->bmax.x, md.bmax.x), std::max(a->bmax.y, md.bmax.y), std::max(a->bmax.z, md.bmax.z));
      a->parts.push_back(std::move(ap));
    }
    if (std::getenv("HW_TRIS")) std::fprintf(stderr, "procasset %s: %d tris\n", name.c_str(), a->tris);
    g_assets[name] = std::move(a);
  };
  for (int fl = 0; fl < 2; fl++) {
    g_leafOverride = Color::hex(fl ? "4f7d33" : "56823a");
    g_flowers = fl ? 9 : 0;
    LeafImg L = makeLeafCluster(T_BIRCH, 512);
    g_leafOverride = Color{-1, 0, 0};
    g_flowers = 0;
    std::vector<uint8_t> rgba = toRGBA(L);
    g_procTex.push_back(std::make_unique<Texture>(uploadTextureRGBA(rgba.data(), 512, 512, true, true, true, false)));
    std::vector<uint8_t> alpha(256 * 256);
    for (int y = 0; y < 256; y++)
      for (int x = 0; x < 256; x++) alpha[(size_t)y * 256 + x] = rgba[(((size_t)(y * 2) * 512) + x * 2) * 4 + 3];
    AssetMaterial m;
    m.name = "leaves_bush";
    m.cutout = true;
    m.tex = g_procTex.back().get();
    m.kind = SeasonKind::Leaf;
    addAsset(fl ? "Bush with Flowers.glb" : "Bush.glb", {{buildBushLeaves(31 + fl, 0.85f, 1.25f, 60, 0.55f), m}}, true, &alpha);
  }
  {
    AssetMaterial m;
    m.name = "rock";
    m.kind = SeasonKind::Rock;
    addAsset("Rock Medium.glb", {{buildRock(11), m}}, false, nullptr);
  }
  {
    MeshData st, sh;
    buildFungus(st, sh);
    AssetMaterial ms, mf;
    ms.name = "bark_stump"; ms.kind = SeasonKind::Bark;
    mf.name = "mushroom_shelf"; mf.kind = SeasonKind::Mush;
    addAsset("Mushroom Laetiporus.glb", {{st, ms}, {sh, mf}}, true, nullptr);
  }
  // ---- small plants, flowers, mushrooms, garden petals ----
  auto plantMat = [&](const char* name, SeasonKind k, const Texture* t) { AssetMaterial m; m.name = name; m.cutout = t != nullptr; m.tex = t; m.kind = k; return m; };
  {
    PlantTex fern = uploadPlant(makeFern(512), g_procTex);
    addAsset("Fern.glb", {{buildRosette(3, 9, 2.4f, 1.0f, 0.5f, 0.55f), plantMat("fern", SeasonKind::Plant, fern.tex)}}, true, &fern.alpha);
    PlantTex parts = uploadPlant(makePartsAtlas(Color::hex("5f9a3c"), 21), g_procTex);
    auto pm = [&](const char* nm, SeasonKind k) { return plantMat(nm, k, parts.tex); };
    addAsset("Plant.glb", {{buildLeafRosette(5, 9, 0.75f, 0.32f, 0.9f, 1.0f), pm("plant_broad", SeasonKind::Plant)}}, true, nullptr);
    addAsset("Plant Big.glb", {{buildLeafRosette(7, 8, 0.3f, 0.13f, 1.4f, 0.9f), pm("plant_low", SeasonKind::Plant)}}, true, nullptr);
    addAsset("Plant-xH5gNlQxAZ.glb", {{buildLeafRosette(9, 7, 0.28f, 0.12f, 1.5f, 0.85f), pm("plant_low2", SeasonKind::Plant)}}, true, nullptr);
    addAsset("Flower Group.glb", {{buildFlowerPlant(41, 6, 2.0f, 13, 0.115f, 0.038f, 0.12f, Color::hex("f6f3f2"), Color::hex("f2bd25"), 0.036f, 2), pm("flower_daisy", SeasonKind::Flower)}}, true, nullptr);
    addAsset("Flower Group-LqTljN6Wg2.glb", {{buildFlowerPlant(43, 7, 2.4f, 5, 0.105f, 0.08f, 0.45f, Color::hex("8a58d4"), Color::hex("f0d050"), 0.024f, 2), pm("flower_violet", SeasonKind::Flower)}}, true, nullptr);
    addAsset("Flower Single.glb", {{buildFlowerPlant(47, 3, 2.0f, 5, 0.13f, 0.105f, 0.55f, Color::hex("e03a2c"), Color::hex("2a1a10"), 0.03f, 3), pm("flower_poppy", SeasonKind::Flower)}}, true, nullptr);
    MeshData stem;
    MeshData cap = buildToadstool(17, 0.4f, Color::hex("b8422c"), stem);
    addAsset("Mushroom.glb", {{stem, plantMat("mushroom_stem", SeasonKind::Mush, nullptr)}, {cap, plantMat("mushroom_cap", SeasonKind::Mush, nullptr)}}, true, nullptr);
    const char* petalFiles[5] = {"Flower Petal.glb", "Flower Petal-LqvxG9OBOU.glb", "Flower Petal-eVE0j49ux9.glb", "Flower Petal-niuBUEJdvM.glb", "Flower Petal-tzG4JcqYWs.glb"};
    const char* petalCol[5] = {"f4c542", "e85d8a", "f2f0ea", "e8763a", "b46ad8"};
    for (int i = 0; i < 5; i++) {
      // loose offering petals lying on the ground: 4 big petals, each two quads with the tip curling up
      PRng R(60 + i);
      PartsGeo P;
      for (int k = 0; k < 4; k++) {
        float a = R.r(0, TAU), r = R.r(0.1f, 0.55f), L = R.r(0.6f, 0.8f), W = L * R.r(0.55f, 0.7f);
        Vec3 c(std::cos(a) * r, 0.015f + k * 0.004f, std::sin(a) * r);
        float h = R.r(0, TAU);
        Vec3 d(std::cos(h), 0, std::sin(h)), side(-d.z, 0, d.x);
        uint32_t col = packCol(Color::hex(petalCol[(i + (k == 3)) % 5]), R.r(0.85f, 1.05f));
        Vec3 tip = (d + Vec3(0, 0.45f, 0)).normalized();
        P.quad(c, d, side, L * 0.55f, W, Vec3(0, 1, 0), 0.0f, 0.5f, 0.25f, 0.5f, col, col);
        P.quad(c + d * L * 0.55f, tip, side, L * 0.45f, W, tip.cross(side).normalized().y < 0 ? tip.cross(side).normalized() * -1.0f : tip.cross(side).normalized(), 0.0f, 0.5f, 0.0f, 0.25f, col, col);
      }
      addAsset(petalFiles[i], {{P.g.data(), pm("flower_petal", SeasonKind::Flower)}}, true, nullptr);
    }
  }
  if (std::getenv("HW_TRIS"))
    for (auto& kv : g_assets) std::fprintf(stderr, "asset %s: %d tris h %.1f\n", kv.first.c_str(), kv.second->tris, kv.second->bmax.y);
}

}  // namespace assets
