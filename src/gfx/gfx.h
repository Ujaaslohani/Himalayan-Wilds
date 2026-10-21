#pragma once
#include "gl.h"
#include "../core/math.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

enum class VFmt { P3C4, P3C4N4, GLB, Terrain, Pos3, Pos2 };

struct VtxP3C4 { float p[3]; uint32_t c; };
struct VtxP3C4N4 { float p[3]; uint32_t c; int8_t n[4]; };
struct VtxGlb { float p[3]; int8_t n[4]; uint16_t uv[2]; uint32_t c; };

struct InstLite { float x, y, z, s, yaw; };
struct InstFull { float r0[4], r1[4], r2[4]; uint32_t tint; };

struct Mesh {
  GLuint vao = 0, vbo = 0, ibo = 0;
  GLsizei count = 0;
  GLenum indexType = GL_UNSIGNED_SHORT;
  VFmt fmt = VFmt::P3C4;
  int vertexCount = 0;
  Vec3 center;
  float radius = 0;
  Vec3 bmin, bmax;
  bool valid() const { return vao != 0; }
  void destroy();
  void draw() const;
  void drawInstanced(GLsizei n) const;
};

struct MeshData {
  VFmt fmt = VFmt::P3C4;
  std::vector<uint8_t> verts;
  std::vector<uint32_t> idx;
  int vertexCount = 0;
  Vec3 bmin{1e9f, 1e9f, 1e9f}, bmax{-1e9f, -1e9f, -1e9f};
};

Mesh uploadMesh(const MeshData& d);
GLsizei vertexStride(VFmt f);

enum class InstKind { Lite, Full };

struct InstBatch {
  GLuint vao = 0, ivbo = 0;
  const Mesh* mesh = nullptr;
  InstKind kind = InstKind::Lite;
  GLsizei count = 0;
  size_t capacity = 0;
  bool owns = true;
  void create(const Mesh* m, InstKind k, GLuint shared = 0);
  void upload(const void* data, size_t n);
  void draw() const;
  void destroy();
};

struct Shader {
  GLuint prog = 0;
  std::unordered_map<std::string, GLint> locs;
  GLint loc(const char* name);
  void use() const { glUseProgram(prog); }
  void set1f(const char* n, float v) { glUniform1f(loc(n), v); }
  void set1i(const char* n, int v) { glUniform1i(loc(n), v); }
  void set2f(const char* n, float a, float b) { glUniform2f(loc(n), a, b); }
  void set3f(const char* n, float a, float b, float c) { glUniform3f(loc(n), a, b, c); }
  void set3(const char* n, const Vec3& v) { glUniform3f(loc(n), v.x, v.y, v.z); }
  void set3c(const char* n, const Color& c) { glUniform3f(loc(n), c.r, c.g, c.b); }
  void set4f(const char* n, float a, float b, float c, float d) { glUniform4f(loc(n), a, b, c, d); }
  void setMat4(const char* n, const Mat4& m) { glUniformMatrix4fv(loc(n), 1, GL_FALSE, m.m); }
};

bool compileShader(Shader& s, const std::string& vs, const std::string& fs, const std::string& defines = "");

struct Texture {
  GLuint id = 0;
  int w = 0, h = 0;
  size_t bytes = 0;
  void destroy();
};

struct TexLevels {
  bool bc = false, alpha = false, mipmaps = true;
  std::vector<std::vector<uint8_t>> data;
  std::vector<std::pair<int, int>> dims;
};
TexLevels prepareTexture(const uint8_t* rgba, int w, int h, bool mipmaps, bool compress, bool alphaCut);
Texture uploadPrepared(const TexLevels& L, bool repeat = true);
Texture uploadTextureRGBA(const uint8_t* rgba, int w, int h, bool mipmaps, bool compress, bool alphaCut, bool repeat = true);
Texture uploadTextureR8(const uint8_t* data, int w, int h, bool linear);
Texture createFloatTexture(const float* data, int w, int h);
Texture createRGBA8Texture(const uint8_t* data, int w, int h, bool linear);

namespace gfx {
void init();
bool hasS3TC();
float maxAniso();
size_t textureBytes();
size_t bufferBytes();
void addBufferBytes(long long n);
extern int drawCalls;
extern long long triangles;
}

uint16_t floatToHalf(float f);
