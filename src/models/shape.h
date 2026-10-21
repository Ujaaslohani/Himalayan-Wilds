#pragma once
#include "../core/math.h"
#include "../gfx/gfx.h"
#include <vector>
#include <string>

struct Geo {
  std::vector<Vec3> pos;
  std::vector<Vec3> nrm;
  std::vector<uint32_t> idx;
};

extern int g_geoLod;
Geo boxGeo(float w, float h, float d, int ws = 1, int hs = 1, int ds = 1);
Geo cylinderGeo(float rTop, float rBot, float h, int radial, int hSeg = 1, bool openEnded = false, float thetaStart = 0, float thetaLen = TAU);
Geo coneGeo(float r, float h, int radial);
Geo icosaGeo(float radius, int detail);
Geo dodecaGeo(float radius);
Geo torusGeo(float radius, float tube, int radialSeg, int tubularSeg, float arc = TAU);
Geo planeGeo(float w, float h, int ws = 1, int hs = 1);
Geo circleGeo(float r, int seg);
Geo sphereGeo(float r, int ws, int hs);
Geo prismGeo(float base, float height, float depth);
void computeSmoothNormals(Geo& g);
Geo translated(Geo g, const Vec3& t);

enum class Pivot { None, Top, XNeg, ZNeg };

uint32_t hexToPacked(const char* hex);

class Shape {
 public:
  struct V { Vec3 p; Vec3 n; uint32_t c; };
  std::vector<V> verts;

  Shape& add(const Geo& g, const char* hex, Vec3 pos = {0, 0, 0}, Vec3 rot = {0, 0, 0}, Vec3 scale = {1, 1, 1});
  Shape& addC(const Geo& g, uint32_t packed, Vec3 pos = {0, 0, 0}, Vec3 rot = {0, 0, 0}, Vec3 scale = {1, 1, 1});
  Shape& addM(const Geo& g, uint32_t packed, const Mat4& m, Pivot pivot = Pivot::None);
  Shape& cloth(const Geo& g, const char* hex, Pivot pivot, Vec3 pos = {0, 0, 0}, Vec3 rot = {0, 0, 0});
  Shape& ball(const char* hex, Vec3 pos, Vec3 scale, Vec3 rot = {0, 0, 0}, int detail = 1);
  Shape& box(const char* hex, Vec3 pos, Vec3 size, Vec3 rot = {0, 0, 0});
  Shape& tube(const char* hex, Vec3 pos, float rTop, float rBot, float h, int seg = 7, Vec3 rot = {0, 0, 0}, Vec3 scale = {1, 1, 1});
  Shape& cone(const char* hex, Vec3 pos, float r, float h, int seg = 6, Vec3 rot = {0, 0, 0}, Vec3 scale = {1, 1, 1});
  Shape& rod(const char* hex, Vec3 a, Vec3 b, float rA, float rB, int seg = 6);
  void append(const Shape& o);
  void reshape(void (*fn)(Vec3&));
  template <class F> void reshapeF(F fn) { for (auto& v : verts) fn(v.p); }
  MeshData build(bool smooth) const;
};

Mat4 placeMatrix(Vec3 pos, Vec3 rot, Vec3 scale);
