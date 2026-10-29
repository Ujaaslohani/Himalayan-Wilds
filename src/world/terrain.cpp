#include <array>
#include <unordered_map>
#include "terrain.h"
#include "../core/noise.h"
#include <thread>
#include <functional>
#include <cstdio>

namespace world {

std::vector<Summit> SUMMITS;
std::vector<Road> ROADS;
std::vector<Route> ROUTES;
std::vector<Step> STEPS;
std::vector<Gap> GAPS;
std::vector<Shrine> SHRINES;
std::vector<Lake> LAKES;
River RIVER;
P2 BRIDGE_SITE{-32, 10};
float BRIDGE_ROT = 0;
P2 FORD_SITE{-30, -135};

namespace {

constexpr float CLIFF_HEIGHT = 36.0f;
constexpr float STAIR_RISER = 0.8f;
constexpr float STAIR_TREAD = 1.3f;
constexpr float LIP = 4.5f;
constexpr float PILLAR = 3.6f;
constexpr float CHASM = 10.0f;
constexpr float REACH = 15.0f;
constexpr float ROUTE_REACH = 34.0f;
constexpr float RIVER_REACH = 46.0f;
constexpr float CELL = 16.0f;
constexpr int NC = (int)(GRID_SIZE / CELL) + 1;

std::vector<float> g_grid;
std::vector<uint32_t> g_colors;
float NORTH = 0;

struct Mesa { float x, z, r, h; std::vector<float> ramps; float rw; };
struct Hollow { float x, z, r, d; };
struct Ridge { std::vector<P2> pts; float h, w; };
struct Terrace { float x, z, r, step; };
struct Tor { float x, z, r, h; };
struct Escarp { float x, z, radius, a0, a1, h, w; };

std::vector<Mesa> MESAS = {
  {170, -60, 36, 16, {2.5f, -0.5f}, 0.36f},
  {-210, 75, 30, 14, {-0.9f, 2.4f}, 0.42f},
  {95, -245, 28, 19, {2.2f}, 0.4f},
  {-135, -175, 22, 12, {0.4f, 2.8f}, 0.42f},
  {88, 215, 30, 15, {3.4f, 1.2f}, 0.45f},
  {240, 70, 26, 13, {3.0f}, 0.4f},
  {-175, 250, 24, 16, {-0.6f, 1.8f}, 0.4f},
};
std::vector<Hollow> HOLLOWS = {
  {150, 20, 40, 10}, {88, 3, 30, -6}, {-150, 140, 30, 5}, {205, -170, 34, 8}, {40, -330, 30, 7},
};
std::vector<Ridge> RIDGES = {
  Ridge{std::vector<P2>{P2{60, -130}, P2{120, -158}, P2{180, -146}, P2{235, -175}}, 14, 20},
  Ridge{std::vector<P2>{P2{-165, -40}, P2{-200, 8}, P2{-245, 34}}, 12, 17},
  Ridge{std::vector<P2>{P2{40, 115}, P2{92, 152}, P2{142, 196}, P2{190, 210}}, 10, 15},
  Ridge{std::vector<P2>{P2{-120, 330}, P2{-170, 300}, P2{-230, 320}}, 13, 18},
};
std::vector<Terrace> TERRACES = {
  {125, -100, 58, 1.7f}, {-115, 185, 46, 1.5f}, {62, 170, 40, 1.6f}, {-235, -20, 44, 1.8f},
};
std::vector<Tor> TORS = {
  {205, -12, 7, 12}, {-92, -238, 6, 10}, {122, 262, 8, 14}, {-252, 152, 7, 11}, {255, -110, 6, 9},
};
std::vector<Escarp> ESCARPS = {
  {0, 0, 175, 0.25f, 1.25f, 8, 4},
  {0, 0, 158, 2.6f, 3.45f, 7, 4},
};

struct PeakDef { const char* name; float a, h, r; };
PeakDef PEAKS[] = {
  {"Gauri Peak", -0.42f, 180, 140},
  {"Bhairav Dhar", 0.62f, 204, 150},
  {"Neel Tibba", 2.45f, 150, 128},
  {"Chandra Shikhar", 3.55f, 218, 154},
  {"Shila Kot", -2.35f, 168, 134},
};

std::vector<P2> catmull(const std::vector<P2>& ctrl, float spacing) {
  std::vector<P2> P;
  P.push_back(ctrl[0]);
  for (auto& c : ctrl) P.push_back(c);
  P.push_back(ctrl.back());
  std::vector<P2> dense;
  for (size_t i = 1; i + 2 < P.size(); i++) {
    const P2 &p0 = P[i - 1], &p1 = P[i], &p2 = P[i + 1], &p3 = P[i + 2];
    int n = std::max(4, (int)std::ceil(hypot2(p2[0] - p1[0], p2[1] - p1[1]) / 0.5f));
    for (int k = 0; k < n; k++) {
      float t = (float)k / n, t2 = t * t, t3 = t2 * t;
      auto f = [&](float a, float b, float c, float d) { return 0.5f * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3); };
      dense.push_back({f(p0[0], p1[0], p2[0], p3[0]), f(p0[1], p1[1], p2[1], p3[1])});
    }
  }
  dense.push_back(ctrl.back());
  std::vector<P2> out{dense[0]};
  float acc = 0;
  for (size_t i = 1; i < dense.size(); i++) {
    acc += hypot2(dense[i][0] - dense[i - 1][0], dense[i][1] - dense[i - 1][1]);
    if (acc >= spacing) { out.push_back(dense[i]); acc = 0; }
  }
  const P2& last = dense.back();
  if (hypot2(out.back()[0] - last[0], out.back()[1] - last[1]) > spacing * 0.4f) out.push_back(last);
  else out.back() = last;
  return out;
}

std::vector<float> cumulative(const std::vector<P2>& pts) {
  std::vector<float> c{0};
  for (size_t i = 1; i < pts.size(); i++) c.push_back(c.back() + hypot2(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]));
  return c;
}

struct SegIndex {
  std::vector<int32_t> cellStart;
  std::vector<uint32_t> items;
  void build(const std::vector<std::array<float, 5>>& segs, float reachMul) {
    std::vector<std::vector<uint32_t>> tmp((size_t)NC * NC);
    for (size_t k = 0; k < segs.size(); k++) {
      const auto& s = segs[k];
      float R = s[4] * reachMul;
      int x0 = (int)std::floor((std::min(s[0], s[2]) - R - GRID_MIN) / CELL), x1 = (int)std::floor((std::max(s[0], s[2]) + R - GRID_MIN) / CELL);
      int z0 = (int)std::floor((std::min(s[1], s[3]) - R - GRID_MIN) / CELL), z1 = (int)std::floor((std::max(s[1], s[3]) + R - GRID_MIN) / CELL);
      for (int cz = std::max(0, z0); cz <= std::min(NC - 1, z1); cz++)
        for (int cx = std::max(0, x0); cx <= std::min(NC - 1, x1); cx++) tmp[(size_t)cz * NC + cx].push_back((uint32_t)k);
    }
    cellStart.assign((size_t)NC * NC + 1, 0);
    for (size_t c = 0; c < tmp.size(); c++) cellStart[c + 1] = cellStart[c] + (int32_t)tmp[c].size();
    items.resize(cellStart.back());
    for (size_t c = 0; c < tmp.size(); c++) std::copy(tmp[c].begin(), tmp[c].end(), items.begin() + cellStart[c]);
  }
  bool cell(float x, float z, int32_t& a, int32_t& b) const {
    int cx = (int)std::floor((x - GRID_MIN) / CELL), cz = (int)std::floor((z - GRID_MIN) / CELL);
    if (cx < 0 || cz < 0 || cx >= NC || cz >= NC || cellStart.empty()) return false;
    a = cellStart[(size_t)cz * NC + cx];
    b = cellStart[(size_t)cz * NC + cx + 1];
    return true;
  }
};

struct PathSeg { uint8_t kind; uint16_t id; uint32_t i; };
std::vector<PathSeg> g_pathSegs;
SegIndex g_pathIndex;
SegIndex g_riverIndex;
std::vector<std::vector<int>> g_stepCells;

float riverBaseHalfWidth(float s) {
  return 6.4f + 1.2f * std::sin(s * 0.017f) + 0.9f * std::sin(s * 0.041f + 1.0f);
}

void buildRiver() {
  std::vector<P2> ctrl = {
    {{-40, -440}}, {{-28, -402}}, {{-50, -352}}, {{-84, -302}}, {{-72, -246}}, {{-40, -196}}, {{-30, -150}},
    {{-58, -102}}, {{-62, -56}}, {{-40, -18}}, {{-32, 10}}, {{-46, 46}}, {{-78, 92}}, {{-84, 142}},
    {{-54, 192}}, {{-30, 242}}, {{-48, 302}}, {{-92, 352}}, {{-102, 412}}, {{-72, 470}}, {{-56, 530}},
    {{-62, 592}}, {{-70, 660}},
  };
  River& R = RIVER;
  R.pts = catmull(ctrl, 2.0f);
  R.cum = cumulative(R.pts);
  R.len = R.cum.back();
  R.head = R.pts[0];
  size_t n = R.pts.size();
  R.wy.resize(n); R.hw.resize(n); R.depth.resize(n); R.rapid.resize(n);
  const float keys[][2] = {{0, 4.6f}, {0.035f, 4.4f}, {0.06f, 3.8f}, {0.11f, 2.3f}, {0.2f, 1.6f}, {0.32f, 0.6f}, {0.44f, -0.7f},
                           {0.5f, -1.0f}, {0.62f, -1.3f}, {0.72f, -1.7f}, {0.79f, -2.9f}, {0.84f, -3.2f}, {1.0f, -3.6f}};
  const int K = sizeof(keys) / sizeof(keys[0]);
  float sBridge = 0, sFord = 0, bd = 1e9f, fd = 1e9f;
  for (size_t i = 0; i < n; i++) {
    float d1 = hypot2(R.pts[i][0] + 32, R.pts[i][1] - 10);
    if (d1 < bd) { bd = d1; sBridge = R.cum[i]; BRIDGE_SITE = R.pts[i]; }
    float d2 = hypot2(R.pts[i][0] + 30, R.pts[i][1] + 135);
    if (d2 < fd) { fd = d2; sFord = R.cum[i]; FORD_SITE = R.pts[i]; }
  }
  for (size_t i = 0; i < n; i++) {
    float f = R.cum[i] / R.len, s = R.cum[i];
    float w = keys[K - 1][1];
    for (int k = 0; k + 1 < K; k++) {
      if (f <= keys[k + 1][0]) {
        float t = (f - keys[k][0]) / (keys[k + 1][0] - keys[k][0]);
        w = lerpf(keys[k][1], keys[k + 1][1], t);
        break;
      }
    }
    R.wy[i] = w;
    float hw = riverBaseHalfWidth(s);
    size_t a = i > 3 ? i - 3 : 0, b = std::min(n - 1, i + 3);
    float ax = R.pts[i][0] - R.pts[a][0], az = R.pts[i][1] - R.pts[a][1];
    float bx = R.pts[b][0] - R.pts[i][0], bz = R.pts[b][1] - R.pts[i][1];
    float curv = std::fabs(std::atan2(ax * bz - az * bx, ax * bx + az * bz));
    hw += curv * 6.0f;
    float depth = 0.72f + 0.3f * std::sin(s * 0.023f + 0.4f) + curv * 1.2f;
    if (s < 34) { hw = lerpf(9.5f, hw, sstep(s, 18, 34)); depth = lerpf(1.3f, depth, sstep(s, 22, 34)); }
    float kb = sstep(std::fabs(s - sBridge), 6, 34);
    hw = lerpf(5.6f, hw, kb);
    depth = lerpf(0.85f, depth, kb);
    float kf = sstep(std::fabs(s - sFord), 5, 26);
    hw = lerpf(8.6f, hw, kf);
    depth = lerpf(0.3f, depth, kf);
    R.hw[i] = hw;
    R.depth[i] = std::min(1.3f, depth);
  }
  for (size_t i = 0; i < n; i++) {
    size_t a = i > 0 ? i - 1 : 0, b = std::min(n - 1, i + 1);
    float ds = std::max(0.5f, R.cum[b] - R.cum[a]);
    float slope = (R.wy[a] - R.wy[b]) / ds;
    R.rapid[i] = clampf((slope - 0.006f) * 55.0f, 0, 1);
    R.depth[i] = lerpf(R.depth[i], 0.42f, R.rapid[i] * 0.8f);
  }
  {
    size_t i = 0;
    float best = 1e9f;
    for (size_t k = 0; k < n; k++) { float d = std::fabs(R.cum[k] - sBridge); if (d < best) { best = d; i = k; } }
    size_t a = i > 2 ? i - 2 : 0, b = std::min(n - 1, i + 2);
    float tx = R.pts[b][0] - R.pts[a][0], tz = R.pts[b][1] - R.pts[a][1];
    float l = hypot2(tx, tz);
    tx /= l; tz /= l;
    BRIDGE_ROT = std::atan2(-tx, -tz);
  }
  std::vector<std::array<float, 5>> segs;
  for (size_t i = 1; i < n; i++) segs.push_back({R.pts[i - 1][0], R.pts[i - 1][1], R.pts[i][0], R.pts[i][1], R.hw[i] + RIVER_REACH});
  g_riverIndex.build(segs, 1.0f);
}

}  // namespace

RiverHit riverQuery(float x, float z) {
  RiverHit h;
  int32_t a, b;
  if (!g_riverIndex.cell(x, z, a, b)) return h;
  const River& R = RIVER;
  int bestI = -1;
  float bestT = 0;
  for (int32_t n = a; n < b; n++) {
    uint32_t k = g_riverIndex.items[n];
    size_t i = k + 1;
    float ax = R.pts[i - 1][0], az = R.pts[i - 1][1], bx = R.pts[i][0], bz = R.pts[i][1];
    float vx = bx - ax, vz = bz - az;
    float L2 = vx * vx + vz * vz;
    if (L2 < 1e-6f) L2 = 1e-6f;
    float t = clampf(((x - ax) * vx + (z - az) * vz) / L2, 0, 1);
    float d = hypot2(x - ax - vx * t, z - az - vz * t);
    if (d < h.d) { h.d = d; bestI = (int)i; bestT = t; }
  }
  if (bestI < 0) return h;
  size_t i = (size_t)bestI;
  float t = bestT;
  h.s = R.cum[i - 1] + (R.cum[i] - R.cum[i - 1]) * t;
  h.wy = lerpf(R.wy[i - 1], R.wy[i], t);
  h.hw = lerpf(R.hw[i - 1], R.hw[i], t);
  h.depth = lerpf(R.depth[i - 1], R.depth[i], t);
  h.flow = lerpf(R.rapid[i - 1], R.rapid[i], t);
  float vx = R.pts[i][0] - R.pts[i - 1][0], vz = R.pts[i][1] - R.pts[i - 1][1];
  float l = std::max(1e-4f, hypot2(vx, vz));
  h.tx = vx / l; h.tz = vz / l;
  h.cx = R.pts[i - 1][0] + vx * t;
  h.cz = R.pts[i - 1][1] + vz * t;
  if (i == 1 && t <= 0.0f) {
    float px = x - R.head[0], pz = z - R.head[1];
    if (px * h.tx + pz * h.tz < -1.0f) h.d = 1e9f;
  }
  return h;
}

float riverMask(float x, float z) {
  RiverHit q = riverQuery(x, z);
  if (q.d > 1e8f) return 0;
  return 1.0f - sstep(q.d, q.hw - 2.0f, q.hw + 5.0f);
}

int lakeAt(float x, float z, float margin) {
  for (size_t i = 0; i < LAKES.size(); i++) {
    const Lake& L = LAKES[i];
    if (hypot2(x - L.x, z - L.z) < L.r + margin) return (int)i;
  }
  return -1;
}

float waterHeightAt(float x, float z) {
  float w = -1e9f;
  RiverHit q = riverQuery(x, z);
  if (q.d < q.hw + 0.5f) w = q.wy;
  for (const Lake& L : LAKES) if (hypot2(x - L.x, z - L.z) < L.r + 0.5f) w = std::max(w, L.level);
  return w;
}

float valleyRadius(float x, float z) {
  float t = std::atan2(z, x);
  float n = angDiff(t, NORTH) / 0.26f;
  float sAng = std::atan2(600.0f, -62.0f);
  float s = angDiff(t, sAng) / 0.2f;
  return 300 + std::sin(t * 3 + 1) * 24 + std::sin(t * 5 + 2) * 14 + std::sin(t * 7 + 0.5f) * 8 + 128 * std::exp(-n * n) + 90 * std::exp(-s * s);
}

float mountainFactor(float x, float z) {
  float rv = valleyRadius(x, z);
  return sstep(hypot2(x, z), rv - 14, rv + 95);
}

float forestDensity(float x, float z) {
  float n = fbm(x * 0.008f + 40, z * 0.008f - 12, 2, 7);
  float d = sstep(n, -0.35f, 0.3f);
  d *= sstep(hypot2(x, z), SETTLEMENT_RADIUS + 4, SETTLEMENT_RADIUS + 30);
  return d;
}

namespace {

float cliffMask(float x, float z) {
  const P2& h = RIVER.head;
  float dirx = 0.08f, dirz = -1.0f;
  float px = x - h[0], pz = z - h[1];
  float behind = px * dirx + pz * dirz - 4.0f;
  if (behind < -6) return 0;
  float side = std::fabs(px * -dirz + pz * dirx);
  return sstep(behind, -4, 10) * (1 - sstep(side, 26, 78));
}

float mesaLift(float x, float z) {
  float add = 0;
  for (const Mesa& M : MESAS) {
    float dx = x - M.x, dz = z - M.z;
    float d = hypot2(dx, dz);
    if (d > M.r + 48) continue;
    float a = std::atan2(dz, dx);
    float soft = 0;
    for (float ra : M.ramps) { float q = angDiff(a, ra) / M.rw; soft = std::max(soft, std::exp(-q * q)); }
    float edge = M.r + valueNoise(std::cos(a) * 2.6f + M.x, std::sin(a) * 2.6f + M.z, 91) * 3.5f;
    float w = 1.2f + soft * 42;
    float k = 1 - sstep(d, edge - w * 0.12f, edge + w * 0.88f);
    float top = M.h + valueNoise(x * 0.09f, z * 0.09f, 93) * 0.8f * k;
    add = std::max(add, top * k);
  }
  for (const Hollow& H : HOLLOWS) {
    float d = hypot2(x - H.x, z - H.z);
    if (d < H.r) add -= H.d * (1 - sstep(d, H.r * 0.25f, H.r));
  }
  return add;
}

float polyDist(const std::vector<P2>& pts, float x, float z, float& along) {
  float best = 1e9f, acc = 0;
  along = 0;
  for (size_t i = 1; i < pts.size(); i++) {
    float ax = pts[i - 1][0], az = pts[i - 1][1], vx = pts[i][0] - ax, vz = pts[i][1] - az;
    float L = hypot2(vx, vz), L2 = std::max(1e-6f, L * L);
    float t = clampf(((x - ax) * vx + (z - az) * vz) / L2, 0, 1);
    float d = hypot2(x - ax - vx * t, z - az - vz * t);
    if (d < best) { best = d; along = acc + L * t; }
    acc += L;
  }
  return best;
}

float featureLift(float x, float z, float r) {
  float add = 0;
  for (const Ridge& R : RIDGES) {
    float along;
    float d = polyDist(R.pts, x, z, along);
    if (d >= R.w) continue;
    float prof = 0.5f + 0.5f * std::cos(PI * d / R.w);
    float var = 0.7f + 0.3f * valueNoise(along * 0.035f, R.h, 61);
    float saddle = 1 - 0.55f * std::exp(-std::pow((along - 0.45f * 140) / 18.0f, 2.0f));
    add += R.h * prof * var * saddle;
  }
  for (const Tor& T : TORS) {
    float d = hypot2(x - T.x, z - T.z);
    if (d >= T.r * 1.6f) continue;
    float k = std::pow(std::max(0.0f, 1 - d / (T.r * 1.6f)), 1.4f);
    float crag = 0.82f + 0.18f * valueNoise(x * 0.5f, z * 0.5f, 67);
    add += T.h * k * crag;
  }
  float a = std::atan2(z, x);
  for (const Escarp& E : ESCARPS) {
    float rr = hypot2(x - E.x, z - E.z);
    float a0 = E.a0, a1 = E.a1;
    float mid = (a0 + a1) * 0.5f, half = (a1 - a0) * 0.5f;
    float da = std::fabs(angDiff(a, mid));
    float am = 1 - sstep(da, half - 0.12f, half + 0.12f);
    if (am <= 0) continue;
    float edge = E.radius + valueNoise(a * 9.0f, E.h, 71) * 6.0f;
    add += E.h * am * sstep(rr, edge - E.w, edge + E.w);
  }
  float zone = sstep(r, 70, 100) * (1 - sstep(r, 250, 290));
  if (zone > 0) {
    float g = ridged(x * 0.011f + 5, z * 0.011f - 9, 2, 77);
    add -= 6.0f * zone * sstep(g, 0.8f, 0.94f);
  }
  return add;
}

float terraceZone(float x, float z, float& step) {
  float best = 0;
  step = 1.6f;
  for (const Terrace& T : TERRACES) {
    float d = hypot2(x - T.x, z - T.z);
    float k = 1 - sstep(d, T.r * 0.6f, T.r);
    if (k > best) { best = k; step = T.step; }
  }
  return best;
}

void mountainLift(float x, float z, float r, float rv, float& u, float& m) {
  u = sstep(r, rv - 14, rv + 95);
  m = 0;
  if (u <= 0) return;
  float rn = ridged(x * 0.0085f + 3, z * 0.0085f - 7, 4, 9);
  float crag = ridged(x * 0.04f, z * 0.04f, 2, 21);
  float fine = ridged(x * 0.11f + 7, z * 0.11f - 3, 2, 37);
  m = std::pow(u, 1.4f) * (62 + 92 * rn) + u * u * crag * 13 + u * fine * 2.0f;
  m += sstep(r, rv + 80, MAP_HALF + 20) * 46;
  for (const Summit& s : SUMMITS) {
    float d = hypot2(x - s.x, z - s.z);
    if (d >= s.r * 1.3f) continue;
    float an = std::atan2(z - s.z, x - s.x);
    float sp = valueNoise(std::cos(an) * 2.1f + s.x * 0.1f, std::sin(an) * 2.1f + s.z * 0.1f, 55);
    float dd = d <= PLATEAU ? d : PLATEAU + (d - PLATEAU) * (1 + 0.32f * sp);
    float tt = clampf((dd - PLATEAU) / (s.r - PLATEAU), 0, 1);
    float k = 1 - std::pow(tt, 0.85f) * (1.15f - 0.15f * tt);
    float ph = s.h * std::pow(std::max(0.0f, k), 1.15f) + (d > PLATEAU + 4 ? (crag * 22 + fine * 4) * k * (1 - k) * 4 * sstep(d, PLATEAU + 4, PLATEAU + 14) : 0);
    float w = 1 - sstep(d, s.r * 0.45f, s.r * 1.3f);
    m = lerpf(m, std::max(ph, m * 0.62f), w);
  }
}

float applyWater(float x, float z, float h) {
  RiverHit q = riverQuery(x, z);
  if (q.d < q.hw + RIVER_REACH) {
    float d = q.d;
    float t = sstep(d, q.hw + 1.5f, q.hw + 34);
    float bank = q.wy + 0.5f + std::max(0.0f, d - q.hw) * 0.05f;
    if (h > bank) h = bank + (h - bank) * (0.1f + 0.9f * t);
    if (d > q.hw + 0.8f && h < q.wy + 0.35f) h = lerpf(q.wy + 0.35f, h, sstep(d, q.hw + 10, q.hw + 30));
    float u = clampf(d / std::max(0.5f, q.hw), 0, 1);
    float bed = q.wy - q.depth * (1 - u * u) - 0.06f;
    bed += valueNoise(x * 0.6f, z * 0.6f, 83) * 0.08f * (0.3f + q.flow);
    float k = 1 - sstep(d, q.hw - 0.4f, q.hw + 2.6f);
    h = lerpf(h, std::min(h, bed + std::max(0.0f, d - q.hw) * 0.35f), k);
  }
  for (const Lake& L : LAKES) {
    float d = hypot2(x - L.x, z - L.z);
    if (d > L.r + 30) continue;
    float t = sstep(d, L.r + 2, L.r + 28);
    float bank = L.level + 0.45f;
    if (h > bank) h = bank + (h - bank) * (0.18f + 0.82f * t);
    if (d >= L.r && h < L.level + 0.35f) h = lerpf(L.level + 0.35f, h, sstep(d, L.r + 6, L.r + 24));
    if (d < L.r + 1.5f) {
      float u = clampf(d / L.r, 0, 1);
      float bed = L.level - L.depth * (1 - u * u) - 0.15f;
      h = lerpf(h, std::min(h, bed), 1 - sstep(d, L.r - 2, L.r + 1.5f));
    }
  }
  return h;
}

float naturalHeight(float x, float z) {
  float r = hypot2(x, z);
  float rv = valleyRadius(x, z);
  float u, m;
  mountainLift(x, z, r, rv, u, m);
  float h = (fbm(x * 0.011f, z * 0.011f, 3, 1) * 0.5f + 0.5f) * 5 + fbm(x * 0.05f + 17, z * 0.05f - 9, 2, 3) * 1.6f + valueNoise(x * 0.12f, z * 0.12f, 41) * 0.45f;
  float hills = sstep(r, SETTLEMENT_RADIUS + 14, SETTLEMENT_RADIUS + 80) * (1 - u);
  if (hills > 0) {
    float roll = fbm(x * 0.0052f + 3.1f, z * 0.0052f - 5.3f, 3, 5) * 0.5f + 0.5f;
    float knob = ridged(x * 0.016f + 1, z * 0.016f - 2, 2, 33);
    float swell = fbm(x * 0.0021f - 7.7f, z * 0.0021f + 2.2f, 2, 13) * 0.5f + 0.5f;
    float dune = fbm(x * 0.013f - 4.4f, z * 0.013f + 8.1f, 2, 71);
    float hill = roll * roll * 42 + knob * 10 + swell * swell * 22 + dune * 5.5f;
    float step;
    float tz = terraceZone(x, z, step);
    if (tz > 0) {
      float q = hill / step, fl = std::floor(q), fr = q - fl;
      float stepped = (fl + sstep(fr, 0.72f, 1.0f)) * step;
      hill = lerpf(hill, stepped, tz);
    }
    h += hills * hill;
  }
  h += (mesaLift(x, z) + featureLift(x, z, r)) * (1 - u) + m;
  float cm = cliffMask(x, z);
  if (cm > 0) {
    const P2& hd = RIVER.head;
    float side = std::fabs(x - hd[0]);
    float notch = 1 - sstep(side, 3, 6);
    h += cm * CLIFF_HEIGHT - notch * cm * 1.6f;
  }
  h = lerpf(h, 0.1f, 1 - sstep(r, SETTLEMENT_RADIUS, SETTLEMENT_RADIUS + 22));
  h = applyWater(x, z, h);
  return h;
}

P2 trailhead(const Summit& s) {
  float a = std::atan2(-s.z, -s.x);
  float d = hypot2(s.x, s.z) - (valleyRadius(s.x, s.z) - 6);
  return {s.x + std::cos(a) * d, s.z + std::sin(a) * d};
}

void routePointRaw(const std::vector<P2>& pts, const std::vector<float>& cum, float s, float& x, float& z, float& dx, float& dz) {
  size_t lo = 1, hi = cum.size() - 1;
  while (lo < hi) { size_t mid = (lo + hi) >> 1; if (cum[mid] < s) lo = mid + 1; else hi = mid; }
  size_t seg = std::max<size_t>(1, lo);
  float L = cum[seg] - cum[seg - 1];
  if (L <= 0) L = 1;
  float t = clampf((s - cum[seg - 1]) / L, 0, 1);
  float ax = pts[seg - 1][0], az = pts[seg - 1][1], bx = pts[seg][0], bz = pts[seg][1];
  x = ax + (bx - ax) * t;
  z = az + (bz - az) * t;
  dx = (bx - ax) / L;
  dz = (bz - az) / L;
}

float featLen(const FeatSpec& f) {
  if (f.kind == FeatKind::Stairs) return f.n * STAIR_TREAD + 1.2f;
  if (f.kind == FeatKind::Gap) return LIP * 2 + f.w;
  return LIP * 2 + f.n * PILLAR + (f.n + 1) * f.w;
}

float headingSpread(const std::vector<P2>& pts, const std::vector<float>& cum, float s0, float s1) {
  float x, z, dx0, dz0, dx, dz;
  routePointRaw(pts, cum, s0, x, z, dx0, dz0);
  float worst = 0;
  for (float s = s0; s <= s1; s += 1.5f) {
    routePointRaw(pts, cum, s, x, z, dx, dz);
    worst = std::max(worst, std::fabs(std::atan2(dx * dz0 - dz * dx0, dx * dx0 + dz * dz0)));
  }
  return worst;
}

float clearance(const std::vector<P2>& pts, const std::vector<float>& cum, float s0, float s1) {
  float best = 1e9f;
  for (float s = s0; s <= s1; s += 2) {
    float x, z, dx, dz;
    routePointRaw(pts, cum, s, x, z, dx, dz);
    for (size_t i = 0; i < pts.size(); i++) {
      if (cum[i] > s0 - 16 && cum[i] < s1 + 16) continue;
      best = std::min(best, hypot2(pts[i][0] - x, pts[i][1] - z));
    }
  }
  return best;
}

std::vector<Feat> placeFeats(const std::vector<P2>& pts, const std::vector<float>& cum, float len, const std::vector<FeatSpec>& specs) {
  std::vector<Feat> feats;
  float next = 10;
  for (const FeatSpec& f : specs) {
    float L = featLen(f);
    float start = std::max(next, f.at * len);
    float best = start, bestV = 9;
    for (float s0 = start; s0 < start + len * 0.3f && s0 + L < len - 12; s0 += 1) {
      float v = headingSpread(pts, cum, std::max(0.0f, s0 - 6), std::min(len, s0 + L + 6)) + (f.kind != FeatKind::Stairs && clearance(pts, cum, s0, s0 + L) < 8.5f ? 5 : 0);
      if (v < bestV - 0.02f) { bestV = v; best = s0; }
      if (v < 0.3f) break;
    }
    if (best + L >= len - 12 || bestV > (f.kind == FeatKind::Stairs ? 0.9f : 0.62f)) continue;
    float rise = f.kind == FeatKind::Stairs ? f.n * STAIR_RISER : f.kind == FeatKind::Pillars ? (f.n + 1) * f.pr : 0;
    feats.push_back({f.kind, best, L, rise, f.n, f.w, f.pr});
    next = best + L + 7;
  }
  return feats;
}

Road makeRoad(const std::vector<P2>& ctrl, bool anchorEnd) {
  Road R;
  R.pts = catmull(ctrl, 3);
  R.cum = cumulative(R.pts);
  size_t n = R.pts.size();
  std::vector<float> nat(n);
  for (size_t i = 0; i < n; i++) nat[i] = naturalHeight(R.pts[i][0], R.pts[i][1]);
  std::vector<float> hs = nat;
  for (int pass = 0; pass < 3; pass++) {
    std::vector<float> nx = hs;
    for (size_t i = 1; i + 1 < n; i++) {
      float s = 0;
      int c = 0;
      for (int k = -4; k <= 4; k++) {
        long j = (long)i + k;
        if (j >= 0 && j < (long)n) { s += hs[(size_t)j]; c++; }
      }
      nx[i] = s / c;
    }
    hs = nx;
  }
  size_t last = anchorEnd ? n - 1 : n;
  for (size_t i = 0; i < n; i++) {
    float k = std::min(1.0f, std::min((float)i / 5.0f, anchorEnd ? (float)(n - 1 - i) / 5.0f : 1.0f));
    hs[i] = lerpf(nat[i], hs[i], k);
  }
  for (size_t i = 1; i < last; i++) hs[i] = std::min(hs[i], hs[i - 1] + 0.3f * (R.cum[i] - R.cum[i - 1]));
  for (size_t i = n - 1; i-- > 1;) hs[i] = std::min(hs[i], hs[i + 1] + 0.3f * (R.cum[i + 1] - R.cum[i]));
  for (size_t i = 0; i < n; i++) {
    RiverHit q = riverQuery(R.pts[i][0], R.pts[i][1]);
    float floor = -0.9f;
    if (q.d < q.hw + 40) floor = q.d < q.hw + 1.5f ? q.wy - q.depth * 0.6f : q.wy + 0.3f;
    hs[i] = std::max(hs[i], floor);
  }
  R.hs = hs;
  R.minX = R.minZ = 1e9f;
  R.maxX = R.maxZ = -1e9f;
  for (auto& p : R.pts) {
    R.minX = std::min(R.minX, p[0]); R.maxX = std::max(R.maxX, p[0]);
    R.minZ = std::min(R.minZ, p[1]); R.maxZ = std::max(R.maxZ, p[1]);
  }
  return R;
}

Route makeRoute(int peak, const std::vector<P2>& ctrl, std::vector<FeatSpec> specs, float target) {
  const Summit& s = SUMMITS[peak];
  float a0 = std::atan2(-s.z, -s.x);
  P2 head = trailhead(s);
  float D0 = hypot2(head[0] - s.x, head[1] - s.z);
  std::vector<P2> wpts;
  for (size_t i = 0; i < ctrl.size(); i++) {
    if (i == 0) { wpts.push_back(head); continue; }
    float th = ctrl[i][0], d = std::min(ctrl[i][1], D0);
    wpts.push_back({s.x + std::cos(a0 + th) * d, s.z + std::sin(a0 + th) * d});
  }
  Route R;
  R.name = s.name;
  R.peak = peak;
  R.pts = catmull(wpts, 2);
  R.cum = cumulative(R.pts);
  R.len = R.cum.back();
  R.top = R.len;
  for (size_t i = 0; i < R.pts.size(); i++) if (hypot2(R.pts[i][0] - s.x, R.pts[i][1] - s.z) <= PLATEAU + 5) { R.top = R.cum[i]; break; }
  const Road* end = nullptr;
  for (const Road& q : ROADS) if (hypot2(q.pts.back()[0] - head[0], q.pts.back()[1] - head[1]) < 1) end = &q;
  R.h0 = end ? end->hs.back() : naturalHeight(head[0], head[1]);
  R.g = 0;
  for (int it = 0; it < 160; it++) {
    R.feats = placeFeats(R.pts, R.cum, R.top, specs);
    float fl = 0, fr = 0;
    for (auto& f : R.feats) { fl += f.len; fr += f.rise; }
    R.g = (s.h - R.h0 - fr) / (R.top - fl);
    if (R.g <= target) break;
    std::vector<FeatSpec*> st;
    for (auto& f : specs) if (f.kind == FeatKind::Stairs && f.n < 12) st.push_back(&f);
    if (st.empty()) break;
    st[it % st.size()]->n += 1;
  }
  R.h1 = s.h;
  R.minX = R.minZ = 1e9f;
  R.maxX = R.maxZ = -1e9f;
  for (auto& p : R.pts) {
    R.minX = std::min(R.minX, p[0]); R.maxX = std::max(R.maxX, p[0]);
    R.minZ = std::min(R.minZ, p[1]); R.maxZ = std::max(R.maxZ, p[1]);
  }
  return R;
}

std::vector<P2> spiral(float turns, float side, float d1, int n) {
  std::vector<P2> out{{0, 99}};
  for (int i = 1; i <= n; i++) {
    float t = (float)i / n;
    out.push_back({side * t * PI * 2 * turns, lerpf(92, d1, std::pow(t, 0.8f))});
  }
  return out;
}

// winding ascent: uneven pace around the peak and in/out lobes so no two climbs feel alike
std::vector<P2> winding(float turns, float side, float d1, int n, float wob, float waves, float pace, float phase) {
  std::vector<P2> out{{0, 99}};
  float a = 0;
  for (int i = 1; i <= n; i++) {
    float t = (float)i / n;
    // pace modulates how fast it wraps around (long traverses, then tighter bends)
    float da = side * PI * 2 * turns / n * (1 + pace * std::sin(t * 9.0f + phase));
    a += da;
    float d = lerpf(92, d1, std::pow(t, 0.85f)) + wob * std::sin(t * waves * PI * 2 + phase) * (1 - t * 0.6f) * std::min(1.0f, t * 4);
    out.push_back({a, d});
  }
  return out;
}

std::vector<P2> zigzag(float start, float end, int legs, float width, float d0, float d1, float sign) {
  std::vector<P2> out;
  for (int i = 0; i < legs; i++) {
    float t = (float)i / (legs - 1);
    float d = lerpf(d0, d1, t);
    float side = (i % 2 ? -1.0f : 1.0f) * sign;
    float w = width * lerpf(1, 0.55f, t);
    float c = start * (1 - t) + end * t;
    out.push_back({side * w * 0.82f + c, d + 3.2f});
    out.push_back({side * w + c, d});
    out.push_back({side * w * 0.82f + c, d - 3.2f});
  }
  return out;
}

float carve(float x, float z, float h) {
  for (const Gap& G : GAPS) {
    float ox = x - G.x, oz = z - G.z;
    if (ox * ox + oz * oz > (G.hl + 12) * (G.hl + 12)) continue;
    float along = std::fabs(ox * G.dx + oz * G.dz), side = std::fabs(ox * G.dz - oz * G.dx);
    float m = (1 - sstep(along, G.hl, G.hl + 1.0f)) * (1 - sstep(side, 7.5f, 10.0f));
    if (m > 0) h = lerpf(h, std::min(h, G.base - CHASM + valueNoise(x * 0.5f, z * 0.5f, 7) * 1.5f), m);
  }
  return h;
}

float baseHeight(float x, float z) {
  float h = naturalHeight(x, z);
  PathHit q = pathQuery(x, z);
  if (q.td < ROUTE_REACH) {
    float cap = q.cap + std::max(0.0f, q.td - 7.0f) * 0.2f * (valueNoise(x * 0.05f, z * 0.05f, 79) - 0.5f);
    float x = h - cap;  // soft knee so the cut bank is a smooth curve, not a crease that zig-zags across the grid
    h = lerpf(h, cap + x * 0.08f, sstep(x, -10.0f, 16.0f));
  }
  if (q.td < REACH) {
    // between two switchback legs ramp from one level to the other instead of leaving a wall at the midline
    float tgt = q.th;
    if (q.td2 < 40) tgt = lerpf(q.th, q.th2, 0.5f * sstep(q.td / (q.td + q.td2), 0.22f, 0.5f));
    float w = 1 - sstep(q.td, 6.5f, 14.5f);
    if (q.td2 < 40) w = std::max(w, 1 - sstep(q.td + q.td2, 24.0f, 34.0f));
    h = lerpf(h, tgt + valueNoise(x * 0.3f, z * 0.3f, 77) * 0.12f, w);
  }
  if (q.rd < REACH && q.rd < q.td) h = lerpf(h, q.rh, 1 - sstep(q.rd, 2.8f, 8.5f));
  if (q.td < 17) h = carve(x, z, h);
  float r = hypot2(x, z);
  h = lerpf(h, 0.1f, 1 - sstep(r, SETTLEMENT_RADIUS, SETTLEMENT_RADIUS + 22));
  if (q.rd < 4) h -= (1 - sstep(q.rd, 1.2f, 3.2f)) * 0.1f;
  RiverHit rq = riverQuery(x, z);
  if (rq.d < rq.hw + 3) {
    float u = clampf(rq.d / std::max(0.5f, rq.hw), 0, 1);
    float bed = rq.wy - rq.depth * (1 - u * u) - 0.06f;
    h = lerpf(h, std::min(h, bed), 1 - sstep(rq.d, rq.hw - 0.5f, rq.hw + 2.5f));
  }
  for (const Lake& L : LAKES) {
    float d = hypot2(x - L.x, z - L.z);
    if (d < L.r + 1) {
      float u = clampf(d / L.r, 0, 1);
      h = std::min(h, lerpf(L.level - L.depth * (1 - u * u) - 0.15f, h, sstep(d, L.r - 1.5f, L.r + 1)));
    }
  }
  for (const Summit& s : SUMMITS) {
    float d = hypot2(x - s.x, z - s.z);
    if (d < PLATEAU + 6) h = lerpf(h, s.h, 1 - sstep(d, PLATEAU - 1, PLATEAU + 6));
  }
  return std::max(-6.0f, h);
}

void parallelFor(int n, const std::function<void(int, int)>& fn) {
  unsigned hc = std::max(1u, std::min(16u, std::thread::hardware_concurrency()));
  if (hc <= 1 || n < 64) { fn(0, n); return; }
  std::vector<std::thread> th;
  int chunk = (n + (int)hc - 1) / (int)hc;
  for (unsigned t = 0; t < hc; t++) {
    int a = (int)t * chunk, b = std::min(n, a + chunk);
    if (a >= b) break;
    th.emplace_back(fn, a, b);
  }
  for (auto& t : th) t.join();
}

}  // namespace

float naturalHeightAt(float x, float z) { return naturalHeight(x, z); }
float terrainHeightRaw(float x, float z) { return baseHeight(x, z); }

PathHit pathQuery(float x, float z) {
  PathHit Q;
  int32_t a, b;
  if (!g_pathIndex.cell(x, z, a, b)) return Q;
  float cd[48], ch[48], cs[48];
  int ci[48];
  float s1 = 0;
  int r1 = -1;
  int nc = 0;
  for (int32_t n = a; n < b; n++) {
    const PathSeg& S = g_pathSegs[g_pathIndex.items[n]];
    const std::vector<P2>& P = S.kind ? ROUTES[S.id].pts : ROADS[S.id].pts;
    float ax = P[S.i - 1][0], az = P[S.i - 1][1], bx = P[S.i][0], bz = P[S.i][1];
    float vx = bx - ax, vz = bz - az;
    float L2 = vx * vx + vz * vz;
    if (L2 < 1e-6f) L2 = 1;
    float t = clampf(((x - ax) * vx + (z - az) * vz) / L2, 0, 1);
    float d = hypot2(x - ax - vx * t, z - az - vz * t);
    if (S.kind) {
      if (d < 40) {
        const Route& r = ROUTES[S.id];
        float sa = r.cum[S.i - 1] + std::sqrt(L2) * t;
        float th = routeHeightAt(r, sa);
        if (d < Q.td) { Q.td = d; Q.th = th; s1 = sa; r1 = S.id; }
        Q.cap = std::min(Q.cap, th + 1.2f + std::max(0.0f, d - 7.0f) * 1.05f);  // lower envelope: no ridge left between switchbacks
        if (nc < 48) { cd[nc] = d; ch[nc] = th; cs[nc] = sa; ci[nc] = S.id; nc++; }
      }
    } else if (d < Q.rd) {
      const Road& r = ROADS[S.id];
      Q.rd = d;
      Q.rh = r.hs[S.i - 1] + (r.hs[S.i] - r.hs[S.i - 1]) * t;
    }
  }
  for (int i = 0; i < nc; i++)
    if ((ci[i] != r1 || std::fabs(cs[i] - s1) > 45.0f) && std::fabs(ch[i] - Q.th) > 0.8f && cd[i] < Q.td2) { Q.td2 = cd[i]; Q.th2 = ch[i]; }
  return Q;
}

float pathDistance(float x, float z) { return pathQuery(x, z).rd; }
float nearRoute(float x, float z) { return pathQuery(x, z).td; }
float pathMask(float x, float z) { return 1 - sstep(pathDistance(x, z), 2.2f, 3.6f); }

float routeHeightAt(const Route& r, float s) {
  s = std::min(s, r.top);
  float flat = 0, extra = 0;
  for (const Feat& f : r.feats) {
    if (s <= f.s) break;
    flat += std::min(f.len, s - f.s);
    if (f.kind == FeatKind::Stairs) extra += clampf((s - f.s - 0.6f) / (f.len - 0.6f), 0.0f, 1.0f) * (f.rise - 0.35f) + sstep(s, f.s + f.len - 0.4f, f.s + f.len) * 0.35f;  // ramp under the steps: no cliff for the terrain to overshoot
    else if (s >= f.s + f.len - LIP) extra += f.rise;
  }
  return r.h0 + r.g * (s - flat) + extra;
}

void routePoint(const Route& r, float s, float& x, float& z, float& dx, float& dz) { routePointRaw(r.pts, r.cum, s, x, z, dx, dz); }

void generate() {
  NORTH = std::atan2(-440.0f, -40.0f);
  buildRiver();
  SUMMITS.clear();
  for (auto& P : PEAKS) {
    float cx = std::cos(P.a), cz = std::sin(P.a);
    float rr = std::min(MAP_HALF - 70, valleyRadius(cx * 260, cz * 260) + 110);
    SUMMITS.push_back({P.name, cx * rr, cz * rr, P.h, P.r});
  }
  std::vector<Lake> lakeDefs = {{"Sarovar Lake", 178, 132, 30, 0, 2.6f}, {"Tarn of Chandra", -186, -112, 23, 0, 2.2f}};
  LAKES.clear();
  for (Lake& L : lakeDefs) {
    float lo = 1e9f;
    for (int k = 0; k < 24; k++) {
      float a = k * TAU / 24;
      lo = std::min(lo, naturalHeight(L.x + std::cos(a) * (L.r + 6), L.z + std::sin(a) * (L.r + 6)));
    }
    L.level = std::max(0.2f, lo - 0.6f);
  }
  LAKES = lakeDefs;
  std::vector<P2> TH;
  for (auto& s : SUMMITS) TH.push_back(trailhead(s));
  P2 FORK{90, 4}, WFORK{-132, 24};
  struct RD { std::vector<P2> c; bool anchor; };
  std::vector<RD> defs = {
    {{{{13, 2}}, {{45, 6}}, FORK}, true},
    {{FORK, {{115, -14}}, {{140, -40}}, {{166, -62}}, {{200, -80}}, {{234, -96}}, TH[0]}, false},
    {{FORK, {{122, 18}}, {{150, 42}}, {{178, 82}}, {{205, 122}}, TH[1]}, false},
    {{{{2, -14}}, {{8, -50}}, {{20, -100}}, {{30, -150}}, {{18, -200}}, {{0, -255}}, {{-10, -310}}, {{-14, -370}}, {{-6, -412}}}, false},
    {{{{30, -150}}, {{2, -142}}, FORD_SITE, {{-80, -150}}, {{-128, -176}}, {{-172, -204}}, TH[4]}, false},
    {{{{-13, 3}}, BRIDGE_SITE, {{-72, 16}}, WFORK}, true},
    {{WFORK, {{-150, 70}}, {{-186, 120}}, {{-216, 162}}, TH[2]}, false},
    {{WFORK, {{-166, 0}}, {{-206, -36}}, {{-250, -70}}, TH[3]}, false},
    {{{{3, 15}}, {{12, 60}}, {{8, 120}}, {{6, 190}}, {{12, 260}}, {{6, 330}}, {{22, 400}}, {{14, 470}}}, false},
  };
  ROADS.clear();
  for (auto& d : defs) ROADS.push_back(makeRoad(d.c, d.anchor));

  // spiral ascents: successive turns are ~45 m apart, so ledges never stack into walls
  // Gauri: long traverses across the face with three broad hairpins (no spiral)
  std::vector<P2> c0 = {{{0, 99}}, {{0.35f, 95}}, {{0.8f, 89}}, {{1.25f, 82}}, {{1.6f, 76}}, {{1.78f, 70}}, {{1.6f, 64}}, {{1.1f, 60}},
    {{0.5f, 57}}, {{-0.1f, 54}}, {{-0.65f, 50}}, {{-1.05f, 45}}, {{-1.2f, 39}}, {{-0.95f, 34}}, {{-0.45f, 31}}, {{0.15f, 28}},
    {{0.7f, 25}}, {{1.0f, 21}}, {{0.8f, 17}}, {{0.45f, 15}}};
  // Shila Kot: sweeping S-curves across both faces with hairpins at the ends
  std::vector<P2> c4 = {{{0, 99}}, {{-0.4f, 94}}, {{-0.9f, 89}}, {{-1.3f, 82}}, {{-1.2f, 74}}, {{-0.6f, 69}}, {{0.15f, 66}}, {{0.8f, 62}},
    {{1.25f, 55}}, {{1.2f, 47}}, {{0.6f, 42}}, {{-0.1f, 38}}, {{-0.75f, 33}}, {{-1.0f, 27}}, {{-0.7f, 21}}, {{-0.25f, 17}}, {{0, 15}}};
  using FK = FeatKind;
  ROUTES.clear();
  ROUTES.push_back(makeRoute(0, c0, {
    {FK::Gap, 0.06f, 5, 4.2f}, {FK::Stairs, 0.14f}, {FK::Stairs, 0.22f}, {FK::Gap, 0.31f, 5, 4.8f},
    {FK::Stairs, 0.42f}, {FK::Stairs, 0.53f}, {FK::Stairs, 0.64f}, {FK::Stairs, 0.75f}, {FK::Stairs, 0.87f}}, 0.42f));
  ROUTES.push_back(makeRoute(1, {{{0, 99}}, {{0.3f, 92}}, {{0.8f, 86}}, {{1.4f, 79}}, {{1.75f, 70}}, {{1.5f, 62}}, {{0.7f, 59}}, {{0, 58}}, {{-0.8f, 56}}, {{-1.5f, 51}},
    {{-1.8f, 44}}, {{-1.35f, 37}}, {{-0.7f, 34}}, {{0, 32}}, {{0.65f, 29}}, {{0.95f, 24}}, {{0.65f, 19}}, {{0.25f, 16}}, {{0, 15}}}, {
    {FK::Stairs, 0.06f}, {FK::Gap, 0.15f, 5, 4.6f}, {FK::Stairs, 0.24f}, {FK::Stairs, 0.33f}, {FK::Gap, 0.43f, 5, 5.0f}, {FK::Stairs, 0.52f},
    {FK::Gap, 0.62f, 5, 5.2f}, {FK::Stairs, 0.72f}, {FK::Stairs, 0.84f}}, 0.45f));
  ROUTES.push_back(makeRoute(2, std::vector<P2>{{{0, 99}}, {{0.7f, 92}}, {{1.5f, 86}}, {{2.1f, 78}}, {{2.2f, 68}}, {{1.7f, 61}}, {{0.9f, 57}}, {{0.1f, 52}},
    {{-0.6f, 46}}, {{-1.0f, 38}}, {{-0.8f, 30}}, {{-0.3f, 24}}, {{0.2f, 19}}, {{0.3f, 16}}}, {
    {FK::Stairs, 0.08f}, {FK::Gap, 0.2f, 5, 4.4f}, {FK::Stairs, 0.32f}, {FK::Pillars, 0.45f, 3, 2.8f, 0.5f}, {FK::Stairs, 0.58f},
    {FK::Gap, 0.7f, 5, 4.8f}, {FK::Stairs, 0.82f}}, 0.45f));
  ROUTES.push_back(makeRoute(3, {{{0, 99}}, {{0.4f, 93}}, {{0.9f, 88}}, {{1.5f, 84}}, {{2.1f, 80}}, {{2.7f, 75}}, {{3.2f, 68}}, {{3.65f, 60}}, {{4.0f, 52}}, {{4.25f, 44}},
    {{4.2f, 37}}, {{3.6f, 34}}, {{2.9f, 32}}, {{2.2f, 29}}, {{1.85f, 25}}, {{2.2f, 21}}, {{2.8f, 18}}, {{3.3f, 16}}, {{3.5f, 15}}}, {
    {FK::Stairs, 0.05f}, {FK::Pillars, 0.12f, 4, 2.6f, 0.5f}, {FK::Stairs, 0.23f, 10}, {FK::Stairs, 0.33f}, {FK::Gap, 0.43f, 5, 4.2f}, {FK::Stairs, 0.52f},
    {FK::Pillars, 0.62f, 3, 2.8f, 0.5f}, {FK::Stairs, 0.73f}, {FK::Stairs, 0.85f}}, 0.48f));
  ROUTES.push_back(makeRoute(4, c4, {
    {FK::Pillars, 0.06f, 3, 3.0f, 0}, {FK::Stairs, 0.17f}, {FK::Stairs, 0.28f}, {FK::Gap, 0.4f, 5, 4.8f}, {FK::Stairs, 0.51f},
    {FK::Stairs, 0.62f}, {FK::Stairs, 0.74f}, {FK::Stairs, 0.86f}}, 0.42f));

  g_pathSegs.clear();
  std::vector<std::array<float, 5>> segs;
  for (size_t id = 0; id < ROADS.size(); id++)
    for (size_t i = 1; i < ROADS[id].pts.size(); i++) {
      g_pathSegs.push_back({0, (uint16_t)id, (uint32_t)i});
      auto& P = ROADS[id].pts;
      segs.push_back({P[i - 1][0], P[i - 1][1], P[i][0], P[i][1], REACH});
    }
  for (size_t id = 0; id < ROUTES.size(); id++)
    for (size_t i = 1; i < ROUTES[id].pts.size(); i++) {
      g_pathSegs.push_back({1, (uint16_t)id, (uint32_t)i});
      auto& P = ROUTES[id].pts;
      segs.push_back({P[i - 1][0], P[i - 1][1], P[i][0], P[i][1], ROUTE_REACH});
    }
  g_pathIndex.build(segs, 1.0f);

  STEPS.clear();
  GAPS.clear();
  for (const Route& r : ROUTES) {
    for (const Feat& f : r.feats) {
      float base = routeHeightAt(r, f.s);
      auto put = [&](float sc, float hl, float hw, float top, float bottom) {
        float x, z, dx, dz;
        routePoint(r, sc, x, z, dx, dz);
        STEPS.push_back({x, z, dx, dz, hl, hw, top, bottom});
      };
      if (f.kind == FeatKind::Stairs) {
        for (int k = 0; k < f.n; k++) {
          bool last = k == f.n - 1;
          float sc = f.s + f.len - (f.n - k - 0.5f) * STAIR_TREAD + (last ? 1.3f : 0);
          float top = base + (k + 1) * STAIR_RISER;
          float x, z, dx, dz;
          routePoint(r, sc, x, z, dx, dz);
          // reach out to each side only while the ground is there: steps end where the slope drops away
          float ext[2];
          for (int sd = 0; sd < 2; sd++) {
            float sg = sd ? 1.0f : -1.0f, e = 2.6f;
            while (e < 7.2f && baseHeight(x + dz * sg * (e + 0.3f), z - dx * sg * (e + 0.3f)) > top - 0.9f) e += 0.3f;
            ext[sd] = e;
          }
          float off = (ext[1] - ext[0]) * 0.5f, hw = (ext[0] + ext[1]) * 0.5f;
          STEPS.push_back({x + dz * off, z - dx * off, dx, dz, STAIR_TREAD / 2 + (last ? 1.34f : 0.04f), hw, top, base - 0.6f});
        }
        continue;
      }
      float deep = base - CHASM - 1.5f;
      put(f.s + LIP / 2, LIP / 2 + 0.02f, 2.6f, base, deep);
      if (f.kind == FeatKind::Pillars)
        for (int i = 0; i < f.n; i++) put(f.s + LIP + f.w * (i + 1) + PILLAR * (i + 0.5f), PILLAR / 2, 1.8f, base + (i + 1) * f.pr, deep);
      put(f.s + f.len - LIP / 2, LIP / 2 + 0.02f, 2.6f, base + f.rise, deep);
      float mid = f.s + f.len / 2;
      float x, z, dx, dz, rx, rz, ddx, ddz;
      routePoint(r, mid, x, z, dx, dz);
      routePoint(r, f.s - 1.5f, rx, rz, ddx, ddz);
      GAPS.push_back({x, z, dx, dz, f.len / 2 - LIP + 0.3f, base, rx, rz, std::atan2(-ddx, -ddz), f.kind == FeatKind::Pillars});
    }
  }
  g_stepCells.assign((size_t)NC * NC, {});
  for (size_t i = 0; i < STEPS.size(); i++) {
    const Step& S = STEPS[i];
    for (int cz = (int)std::floor((S.z - 9 - GRID_MIN) / CELL); cz <= (int)std::floor((S.z + 9 - GRID_MIN) / CELL); cz++)
      for (int cx = (int)std::floor((S.x - 9 - GRID_MIN) / CELL); cx <= (int)std::floor((S.x + 9 - GRID_MIN) / CELL); cx++)
        if (cx >= 0 && cz >= 0 && cx < NC && cz < NC) g_stepCells[(size_t)cz * NC + cx].push_back((int)i);
  }

  SHRINES.clear();
  for (size_t i = 0; i < SUMMITS.size(); i++) {
    const Summit& s = SUMMITS[i];
    float yaw = std::atan2(-s.x, -s.z);
    for (const Route& r : ROUTES)
      if (r.peak == (int)i) {
        float px, pz, dx, dz;
        routePoint(r, r.len - 12, px, pz, dx, dz);
        yaw = std::atan2(px - s.x, pz - s.z);
      }
    SHRINES.push_back({s.name, s.x, s.z, s.h, yaw});
  }

  g_grid.assign((size_t)GRID_N * GRID_N, 0);
  parallelFor(GRID_N, [](int a, int b) {
    for (int iz = a; iz < b; iz++)
      for (int ix = 0; ix < GRID_N; ix++) g_grid[(size_t)iz * GRID_N + ix] = baseHeight(GRID_MIN + ix * GRID_STEP, GRID_MIN + iz * GRID_STEP);
  });
  // soften the height field so slopes and ridges read as curves, not facets (stronger on steep ground)
  // keep trails, steps, jump gaps, roads and shrine grounds exactly as carved
  std::vector<float> keep((size_t)GRID_N * GRID_N, 1.0f);
  parallelFor(GRID_N, [&](int a, int b) {
    for (int iz = a; iz < b; iz++)
      for (int ix = 0; ix < GRID_N; ix++) {
        float x = GRID_MIN + ix * GRID_STEP, z = GRID_MIN + iz * GRID_STEP;
        PathHit q = pathQuery(x, z);
        float k = sstep(std::min(q.td, q.rd), 6.0f, 7.0f);  // keep only the walking surface
        for (const Shrine& sh : SHRINES) k = std::min(k, sstep(hypot2(x - sh.x, z - sh.z), 24.0f, 34.0f));
        k = std::min(k, sstep(nearRoute(x, z), 4.0f, 9.0f));
        keep[(size_t)iz * GRID_N + ix] = k;
      }
  });
  std::vector<float> band((size_t)GRID_N * GRID_N, 0.0f);
  for (size_t i = 0; i < band.size(); i++) band[i] = keep[i] > 0.99f ? 0.0f : 0.0f;
  parallelFor(GRID_N, [&](int a, int b) {
    for (int iz = a; iz < b; iz++)
      for (int ix = 0; ix < GRID_N; ix++) {
        PathHit q = pathQuery(GRID_MIN + ix * GRID_STEP, GRID_MIN + iz * GRID_STEP);
        float d = std::min(q.td, q.rd);
        band[(size_t)iz * GRID_N + ix] = sstep(d, 3.0f, 3.8f) * (1 - sstep(d, 10.0f, 16.0f));
      }
  });
  for (int pass = 0; pass < 3; pass++) {
    std::vector<float> src = g_grid;
    bool extra = pass >= 3;  // later passes only round off the trail banks
    parallelFor(GRID_N, [&](int a, int b) {
      for (int iz = a; iz < b; iz++)
        for (int ix = 1; ix < GRID_N - 1; ix++) {
          if (iz == 0 || iz == GRID_N - 1) continue;
          auto S = [&](int i, int j) { return src[(size_t)j * GRID_N + i]; };
          float c = S(ix, iz);
          float avg = (c * 4 + (S(ix - 1, iz) + S(ix + 1, iz) + S(ix, iz - 1) + S(ix, iz + 1)) * 2 + S(ix - 1, iz - 1) + S(ix + 1, iz - 1) + S(ix - 1, iz + 1) + S(ix + 1, iz + 1)) / 16.0f;
          float slope = (std::fabs(S(ix + 1, iz) - S(ix - 1, iz)) + std::fabs(S(ix, iz + 1) - S(ix, iz - 1))) / (2 * GRID_STEP);
          float k = extra ? band[(size_t)iz * GRID_N + ix] * keep[(size_t)iz * GRID_N + ix] : (0.45f + 0.55f * sstep(slope, 0.15f, 0.8f)) * keep[(size_t)iz * GRID_N + ix];
          g_grid[(size_t)iz * GRID_N + ix] = c + (avg - c) * k;
        }
    });
  }
  // natural irregularity on trail and road banks: small bumps so the edges are never ruler-straight
  parallelFor(GRID_N, [&](int a, int b) {
    for (int iz = a; iz < b; iz++)
      for (int ix = 0; ix < GRID_N; ix++) {
        float x = GRID_MIN + ix * GRID_STEP, z = GRID_MIN + iz * GRID_STEP;
        PathHit q = pathQuery(x, z);
        float d = std::min(q.td, q.rd);
        float band = sstep(d, 2.6f, 3.4f) * (1 - sstep(d, 5.0f, 9.0f));
        if (band > 0) g_grid[(size_t)iz * GRID_N + ix] += band * (fbm(x * 0.35f, z * 0.35f, 3, 91) * 0.5f + fbm(x * 1.1f, z * 1.1f, 2, 92) * 0.18f);
      }
  });
  // trim stair steps against the finished ground (not the raw shape): end each step where the ground falls away
  for (Step& S : STEPS) {
    if (S.hw < 2.61f || S.hl > 2.2f) continue;  // gap lips keep their size
    float cx = S.x, cz = S.z, px = S.dz, pz = -S.dx;
    float ext[2];
    for (int sd = 0; sd < 2; sd++) {
      float sg = sd ? 1.0f : -1.0f, e = 2.6f;
      while (e < 7.2f) {
        float qx = cx + px * sg * (e + 0.3f), qz = cz + pz * sg * (e + 0.3f);
        bool ok = true;
        for (float a = -S.hl; a <= S.hl + 0.01f; a += S.hl) ok = ok && getTerrainHeight(qx + S.dx * a, qz + S.dz * a) > S.top - 1.2f;
        if (!ok) break;
        e += 0.3f;
      }
      ext[sd] = e;
    }
    // re-centre from the step's current centre, which may already be offset
    float half = S.hw;
    float l = std::min(ext[0], half * 2), r = std::min(ext[1], half * 2);
    float off = (r - l) * 0.5f;
    S.x += px * off; S.z += pz * off;
    S.hw = (l + r) * 0.5f;
  }
  // nothing of the ground may poke through a step: each grid node near a staircase is capped by the NEAREST step only
  {
    std::unordered_map<size_t, std::array<float, 3>> cap;  // node -> (distance outside footprint, cap height, is stair)  // node -> (distance outside footprint, cap height)
    for (const Step& S : STEPS) {
      float R = std::max(S.hl, S.hw) + 3.0f;
      int x0 = std::max(0, (int)((S.x - R - GRID_MIN) / GRID_STEP)), x1 = std::min(GRID_N - 1, (int)((S.x + R - GRID_MIN) / GRID_STEP) + 1);
      int z0 = std::max(0, (int)((S.z - R - GRID_MIN) / GRID_STEP)), z1 = std::min(GRID_N - 1, (int)((S.z + R - GRID_MIN) / GRID_STEP) + 1);
      for (int iz = z0; iz <= z1; iz++)
        for (int ix = x0; ix <= x1; ix++) {
          float ox = GRID_MIN + ix * GRID_STEP - S.x, oz = GRID_MIN + iz * GRID_STEP - S.z;
          float along = std::fabs(ox * S.dx + oz * S.dz), side = std::fabs(ox * S.dz - oz * S.dx);
          if (along > S.hl + 1.6f || side > S.hw + 2.6f) continue;
          float d = std::max(0.0f, along - S.hl);
          size_t k = (size_t)iz * GRID_N + ix;
          auto it = cap.find(k);
          if (it == cap.end() || d < (*it).second[0]) cap[k] = {d, S.top - 0.9f + d * 1.1f, (S.hw > 2.61f && S.hl < 2.2f) ? 1.0f : 0.0f};
        }
    }
    for (auto& kv : cap) {
      int ix = (int)(kv.first % GRID_N), iz = (int)(kv.first / GRID_N);
      PathHit q = pathQuery(GRID_MIN + ix * GRID_STEP, GRID_MIN + iz * GRID_STEP);  // never dig below the trail's own surface
      float floorH = (kv.second[2] > 0.5f && kv.second[0] > 0.01f && q.td < 4.0f) ? std::min(q.th - 0.6f, kv.second[1] + 2.0f) : -1e9f;
      g_grid[kv.first] = std::min(g_grid[kv.first], std::max(kv.second[1], floorH));
    }
  }
  g_colors.assign((size_t)GRID_N * GRID_N, 0);
  parallelFor(GRID_N, [](int a, int b) {
    for (int iz = a; iz < b; iz++)
      for (int ix = 0; ix < GRID_N; ix++) {
        auto H = [](int i, int j) { i = std::max(0, std::min(GRID_N - 1, i)); j = std::max(0, std::min(GRID_N - 1, j)); return g_grid[(size_t)j * GRID_N + i]; };
        float x = GRID_MIN + ix * GRID_STEP, z = GRID_MIN + iz * GRID_STEP;
        float y = H(ix, iz);
        float hx = H(ix + 1, iz) - H(ix - 1, iz), hz = H(ix, iz + 1) - H(ix, iz - 1);
        Vec3 n(-hx / (2 * GRID_STEP), 1, -hz / (2 * GRID_STEP));
        n.normalize();
        Color c = terrainColor(x, z, y, n.y);
        PathHit q = pathQuery(x, z);
        float wob = fbm(x * 0.45f, z * 0.45f, 3, 93) * 1.1f + fbm(x * 1.7f, z * 1.7f, 2, 94) * 0.35f;
        float road = std::max(1 - sstep(q.rd + wob, 2.0f, 3.2f), (1 - sstep(q.td + wob, 2.4f, 3.6f)) * 0.7f);
        g_colors[(size_t)iz * GRID_N + ix] = c.packSrgb((uint8_t)std::lround(road * 255));
      }
  });
}

const std::vector<float>& heightGrid() { return g_grid; }
const std::vector<uint32_t>& colorGrid() { return g_colors; }

float getTerrainHeight(float x, float z) {
  float gx = (x - GRID_MIN) / GRID_STEP, gz = (z - GRID_MIN) / GRID_STEP;
  if (g_grid.empty()) return baseHeight(x, z);
  gx = clampf(gx, 0, GRID_N - 1.001f);
  gz = clampf(gz, 0, GRID_N - 1.001f);
  int ix = (int)gx, iz = (int)gz;
  float fx = gx - ix, fz = gz - iz;
  // Catmull-Rom bicubic, identical to the near-terrain render so feet and collision match what is drawn
  auto w = [](float t, float* o) { float t2 = t * t, t3 = t2 * t; o[0] = -0.5f*t3 + t2 - 0.5f*t; o[1] = 1.5f*t3 - 2.5f*t2 + 1; o[2] = -1.5f*t3 + 2*t2 + 0.5f*t; o[3] = 0.5f*t3 - 0.5f*t2; };
  float wx[4], wz[4];
  w(fx, wx); w(fz, wz);
  float r = 0;
  for (int j = 0; j < 4; j++) {
    int zz = std::min(std::max(iz - 1 + j, 0), GRID_N - 1);
    const float* row = &g_grid[(size_t)zz * GRID_N];
    float acc = 0;
    for (int i = 0; i < 4; i++) acc += row[std::min(std::max(ix - 1 + i, 0), GRID_N - 1)] * wx[i];
    r += acc * wz[j];
  }
  return r;
}

float slopeAt(float x, float z) {
  const float e = 1.2f;
  float sx = getTerrainHeight(x + e, z) - getTerrainHeight(x - e, z);
  float sz = getTerrainHeight(x, z + e) - getTerrainHeight(x, z - e);
  return hypot2(sx, sz) / (2 * e);
}

float localSlope(float x, float z) {
  const float e = 0.5f;
  float sx = getTerrainHeight(x + e, z) - getTerrainHeight(x - e, z);
  float sz = getTerrainHeight(x, z + e) - getTerrainHeight(x, z - e);
  return hypot2(sx, sz) / (2 * e);
}

float stepHeightAt(float x, float z, float h) {
  int cx = (int)std::floor((x - GRID_MIN) / CELL), cz = (int)std::floor((z - GRID_MIN) / CELL);
  if (cx < 0 || cz < 0 || cx >= NC || cz >= NC || g_stepCells.empty()) return h;
  for (int i : g_stepCells[(size_t)cz * NC + cx]) {
    const Step& S = STEPS[i];
    float ox = x - S.x, oz = z - S.z;
    if (ox * ox + oz * oz > 16) continue;
    float along = ox * S.dx + oz * S.dz, side = ox * S.dz - oz * S.dx;
    if (std::fabs(along) <= S.hl && std::fabs(side) <= S.hw && S.top > h) h = S.top;
  }
  return h;
}

const Gap* gapAt(float x, float z, float feet) {
  for (const Gap& G : GAPS) {
    float ox = x - G.x, oz = z - G.z;
    if (ox * ox + oz * oz > (G.hl + 14) * (G.hl + 14)) continue;
    float along = std::fabs(ox * G.dx + oz * G.dz), side = std::fabs(ox * G.dz - oz * G.dx);
    if (along < G.hl + 1 && side < 9.5f && feet < G.base - 3 && feet > G.base - CHASM - 4) return &G;
  }
  return nullptr;
}

Color terrainColor(float x, float z, float h, float normalY) {
  static const Color grass = Color::hex("6c9238"), grassLight = Color::hex("95b54c"), grassDark = Color::hex("47702a"), dry = Color::hex("b19a58");
  static const Color dirt = Color::hex("9c7b4f"), dirtDark = Color::hex("735636"), gravel = Color::hex("8e8576"), gravelDark = Color::hex("6d6558");
  static const Color mud = Color::hex("5a4630"), wet = Color::hex("3f3326"), rock = Color::hex("7f7b74"), rockDark = Color::hex("57534d");
  static const Color rockWarm = Color::hex("8c7c6a"), scree = Color::hex("9a958b"), alpine = Color::hex("9a8a52"), forest = Color::hex("2f5a24");
  static const Color snow = Color::hex("eef3f7"), sand = Color::hex("b9a98a"), pebble = Color::hex("8a8478"), paddy = Color::hex("a7b54a"), paddy2 = Color::hex("c9b660");
  float broad = fbm(x * 0.02f, z * 0.02f, 2, 11);
  float fine = valueNoise(x * 0.35f, z * 0.35f, 13);
  float r = hypot2(x, z);
  float rv = valleyRadius(x, z);
  float u = sstep(r, rv - 14, rv + 95);
  Color out = grass;
  out = out.lerp(grassLight, sstep(broad, 0.0f, 0.6f) * 0.7f);
  out = out.lerp(grassDark, sstep(-broad, 0.1f, 0.7f) * 0.7f);
  out = out.lerp(dry, sstep(fbm(x * 0.03f + 5, z * 0.03f, 2, 17), 0.2f, 0.6f) * 0.65f);
  out = out.lerp(grassDark, forestDensity(x, z) * 0.35f);
  out = out.lerp(forest, u * (1 - sstep(h, 60, 85)) * sstep(forestDensity(x * 1.3f, z * 1.3f), 0.2f, 0.6f) * 0.75f);
  float step;
  float tz = terraceZone(x, z, step) * (1 - u);
  if (tz > 0.05f) {
    float band = 0.5f + 0.5f * std::sin(h / step * TAU + 0.6f);
    Color field = paddy.lerp(paddy2, band * 0.7f + fine * 0.15f);
    out = out.lerp(field, tz * 0.55f * sstep(normalY, 0.8f, 0.95f));
  }
  float mudN = fbm(x * 0.06f + 9, z * 0.06f - 4, 2, 29);
  float low = 1 - sstep(h, 0.4f, 4.5f);
  float mudAmt = sstep(mudN, 0.25f, 0.55f) * (0.35f + low * 0.65f) * (1 - u);
  out = out.lerp(mud, mudAmt * 0.85f);
  out = out.lerp(wet, mudAmt * sstep(mudN, 0.45f, 0.62f) * 0.6f);
  out = out.lerp(dirtDark, sstep(fine, 0.55f, 0.9f) * 0.18f * (1 - u));
  float village = 1 - sstep(r, 7, 14);
  out = out.lerp(dirt, village * 0.85f);
  PathHit q = pathQuery(x, z);
  float pm = 1 - sstep(q.rd, 2.0f, 3.8f);
  if (pm > 0) {
    out = out.lerp(gravel, pm);
    out = out.lerp(gravelDark, pm * sstep(fine, 0.0f, 0.7f) * 0.5f);
    out = out.lerp(mud, pm * sstep(mudN, 0.3f, 0.6f) * 0.35f);
  }
  float shoulder = (1 - sstep(q.rd, 3.6f, 6.5f)) * (1 - pm);
  out = out.lerp(dry, shoulder * 0.6f);
  RiverHit rq = riverQuery(x, z);
  if (rq.d < rq.hw + 14) {
    float rm = 1 - sstep(rq.d, rq.hw - 2, rq.hw + 2.5f);
    out = out.lerp(wet, rm);
    out = out.lerp(scree, rm * sstep(fine, 0.3f, 0.8f) * 0.35f);
    float bank = std::max(0.0f, 1 - std::fabs(rq.d - rq.hw - 3.5f) / 4.5f);
    float bar = sstep(valueNoise(x * 0.08f, z * 0.08f, 47), 0.1f, 0.5f);
    out = out.lerp(sand.lerp(pebble, sstep(fine, 0.2f, 0.8f)), bank * (0.45f + 0.4f * bar));
    out = out.lerp(mud, bank * (1 - bar) * 0.25f);
  }
  for (const Lake& L : LAKES) {
    float d = hypot2(x - L.x, z - L.z);
    if (d > L.r + 12) continue;
    float lm = 1 - sstep(d, L.r - 1.5f, L.r + 1.5f);
    out = out.lerp(wet, lm * 0.9f);
    float shore = std::max(0.0f, 1 - std::fabs(d - L.r - 2.5f) / 4.0f);
    out = out.lerp(sand.lerp(pebble, sstep(fine, 0.0f, 0.8f)), shore * 0.65f);
  }
  out = out.lerp(alpine, sstep(h, 70, 100) * (1 - sstep(h, 130, 150)) * 0.45f);
  float slope = 1 - sstep(normalY, 0.55f, 0.84f);
  float band = fbm(x * 0.05f + h * 0.02f, z * 0.05f - h * 0.03f, 3, 77) * 0.5f + 0.5f;
  Color rk = rock.lerp(rockDark, band * 0.6f).lerp(rockWarm, sstep(fbm(x * 0.04f, z * 0.04f, 2, 51), 0.1f, 0.6f) * 0.5f);
  float cliff = 1 - sstep(normalY, 0.42f, 0.66f);
  out = out.lerp(rk, std::min(1.0f, (slope * 0.6f + sstep(h, 130, 155) * 0.6f) * sstep(h, 85, 135) + cliff * (0.55f + 0.35f * sstep(fine, -0.3f, 0.5f))));
  out = out.lerp(scree, (1 - slope) * sstep(h, 85, 115) * sstep(fine, -0.2f, 0.6f) * 0.5f);
  float tdw = q.td + fbm(x * 0.45f, z * 0.45f, 3, 93) * 1.1f + fbm(x * 1.7f, z * 1.7f, 2, 94) * 0.35f;  // wavy, broken edge
  float tq = 1 - sstep(tdw, 2.4f, 3.6f);
  if (tq > 0) {
    Color t = dirt.lerp(gravel, 0.45f).lerp(dirtDark, sstep(fine, 0.2f, 0.8f) * 0.4f);
    out = out.lerp(t, tq * 0.9f);
  }
  float snowLine = 152 + fbm(x * 0.02f, z * 0.02f, 2, 61) * 18;
  out = out.lerp(snow, sstep(h, snowLine, snowLine + 22) * (0.08f + 0.92f * sstep(normalY, 0.58f, 0.8f)) * (1 - tq * 0.5f));
  out = out.offsetHSL(0, 0, fine * 0.03f);
  return out;
}

std::vector<std::vector<Vec3>> fenceLines() {
  std::vector<std::vector<Vec3>> out;
  for (const Road& r : ROADS) {
    for (int side : {-1, 1}) {
      std::vector<Vec3> run;
      for (size_t i = 1; i + 1 < r.pts.size(); i++) {
        float x = r.pts[i][0], z = r.pts[i][1];
        float dx = r.pts[i + 1][0] - r.pts[i - 1][0], dz = r.pts[i + 1][1] - r.pts[i - 1][1];
        float L = std::max(1e-4f, hypot2(dx, dz));
        float nx = (-dz / L) * side, nz = (dx / L) * side;
        float y = getTerrainHeight(x, z);
        float drop = y - getTerrainHeight(x + nx * 8, z + nz * 8);
        float fx = x + nx * 3.4f, fz = z + nz * 3.4f;
        if (drop > 3.2f && hypot2(fx, fz) > SETTLEMENT_RADIUS + 20 && riverMask(fx, fz) < 0.01f && lakeAt(fx, fz, 4) < 0) run.push_back({fx, getTerrainHeight(fx, fz), fz});
        else { if (run.size() > 2) out.push_back(run); run.clear(); }
      }
      if (run.size() > 2) out.push_back(run);
    }
  }
  return out;
}

MeshData buildBackdrop() {
  const int A = 720, Rn = 96;
  const float r0 = MAP_HALF - 2, r1 = 3600;
  static const Color forest = Color::hex("2f5a24"), rockDark = Color::hex("57534d"), rock = Color::hex("7f7b74"), rockWarm = Color::hex("8c7c6a"), snow = Color::hex("eef3f7");
  MeshData d;
  d.fmt = VFmt::P3C4N4;
  std::vector<Vec3> pos((size_t)(A + 1) * (Rn + 1));
  std::vector<uint32_t> col(pos.size());
  for (int j = 0; j <= Rn; j++) {
    float t = (float)j / Rn;
    float rr = r0 + (r1 - r0) * std::pow(t, 1.5f);
    for (int i = 0; i <= A; i++) {
      float a = (float)i / A * TAU;
      float x = std::cos(a) * rr, z = std::sin(a) * rr;
      float ridge = ridged(x * 0.0026f + 5, z * 0.0026f - 3, 6, 71);
      float crag = ridged(x * 0.011f - 2, z * 0.011f + 7, 4, 13);
      float env = std::sin(std::min(1.0f, t * 1.25f) * PI);
      float y = 140 + env * (80 + std::pow(ridge, 1.7f) * 420 + crag * 55 * sstep(ridge, 0.25f, 0.7f)) + (1 - t) * 70;
      if (t > 0.92f) y *= 1 - (t - 0.92f) / 0.08f * 0.6f;
      float edge = getTerrainHeight(std::cos(a) * r0, std::sin(a) * r0) - 0.6f;
      y = lerpf(edge, std::max(y, edge), sstep(t, 0, 0.1f));
      size_t k = (size_t)j * (A + 1) + i;
      pos[k] = {x, y, z};
      float sn = sstep(y, 250 + ridge * 50, 340) * (0.55f + 0.45f * sstep(ridge, 0.2f, 0.6f));
      Color c = forest.lerp(rockDark, sstep(y, 100, 150)).lerp(rock, sstep(y, 140, 200) * 0.7f).lerp(rockWarm, sstep(ridge, 0.3f, 0.7f) * 0.3f).lerp(snow, sn);
      col[k] = c.packSrgb();
    }
  }
  std::vector<Vec3> nrm(pos.size(), Vec3(0, 0, 0));
  for (int j = 0; j < Rn; j++)
    for (int i = 0; i < A; i++) {
      uint32_t a = j * (A + 1) + i, b = a + 1, c = a + A + 1, e = c + 1;
      uint32_t tri[6] = {a, b, c, b, e, c};
      for (int t = 0; t < 6; t += 3) {
        Vec3 n = (pos[tri[t + 1]] - pos[tri[t]]).cross(pos[tri[t + 2]] - pos[tri[t]]);
        for (int q = 0; q < 3; q++) nrm[tri[t + q]] += n;
        d.idx.push_back(tri[t]); d.idx.push_back(tri[t + 1]); d.idx.push_back(tri[t + 2]);
      }
    }
  d.verts.resize(pos.size() * sizeof(VtxP3C4N4));
  VtxP3C4N4* v = (VtxP3C4N4*)d.verts.data();
  for (size_t k = 0; k < pos.size(); k++) {
    Vec3 n = nrm[k].normalized();
    if (n.y < 0) n = -n;
    v[k].p[0] = pos[k].x; v[k].p[1] = pos[k].y; v[k].p[2] = pos[k].z;
    v[k].c = col[k];
    v[k].n[0] = (int8_t)std::lround(n.x * 127); v[k].n[1] = (int8_t)std::lround(n.y * 127); v[k].n[2] = (int8_t)std::lround(n.z * 127); v[k].n[3] = 0;
    d.bmin = Vec3(std::min(d.bmin.x, pos[k].x), std::min(d.bmin.y, pos[k].y), std::min(d.bmin.z, pos[k].z));
    d.bmax = Vec3(std::max(d.bmax.x, pos[k].x), std::max(d.bmax.y, pos[k].y), std::max(d.bmax.z, pos[k].z));
  }
  d.vertexCount = (int)pos.size();
  return d;
}

}  // namespace world
