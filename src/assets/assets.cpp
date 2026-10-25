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
      if (lm.cutout && c.req.layer == "tree" && !std::getenv("HW_NOTHIN")) thinCanopy(p.data, 0.3f, 0.7f, 1.18f);
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
