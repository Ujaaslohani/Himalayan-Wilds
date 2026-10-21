#include "shadows.h"
#include "terrain.h"
#include "../render/render.h"

namespace {
const char* CAST_VS = R"(
in vec2 a_pos;
in vec4 a_i0;
in vec4 a_i1;
out vec2 v_q;
out float v_fade;
void main(){
  vec3 c = a_i0.xyz; float r = a_i0.w; vec3 nrm = a_i1.xyz; float h = a_i1.w;
  vec3 L = normalize(u_sunDir.xyz);
  vec3 sd = -L - nrm * dot(-L, nrm);
  float sl = length(sd);
  vec3 ax = normalize(cross(nrm, vec3(0.0, 0.0, 1.0)));
  sd = sl > 0.001 ? sd / sl : ax;
  vec3 sp = normalize(cross(sd, nrm));
  float el = max(dot(L, nrm), 0.12);
  float len = r + min(h * sqrt(1.0 - el * el) / el * 0.55, h * 1.6);
  vec2 q = a_pos;
  vec3 w = c + nrm * 0.07 + sp * q.x * r + sd * (q.y * len + len - r);
  v_q = q;
  float dc = distance(c.xz, u_camPos.xz);
  v_fade = smoothstep(26.0, 34.0, dc) * (1.0 - smoothstep(60.0, 90.0, dc));
  gl_Position = u_viewProj * vec4(w, 1.0);
}
)";
const char* CAST_FS = R"(
in vec2 v_q;
in float v_fade;
out vec4 fragColor;
void main(){
  float d = length(v_q);
  float a = (1.0 - smoothstep(0.22, 1.0, d)) * mix(1.0, 0.55, v_q.y * 0.5 + 0.5) * 0.66 * u_shadowStrength * v_fade;
  if (a < 0.004) discard;
  fragColor = vec4(0.02, 0.03, 0.05, a);
}
)";
GLuint g_vao = 0, g_ibo = 0;
size_t g_cap = 0;
}  // namespace

CastInstance castInstance(float x, float y, float z, float r, float h) {
  float e = std::max(0.6f, r * 0.5f);
  Vec3 n(world::getTerrainHeight(x - e, z) - world::getTerrainHeight(x + e, z), 2 * e, world::getTerrainHeight(x, z - e) - world::getTerrainHeight(x, z + e));
  n.normalize();
  return {{x, y, z}, r, {n.x, n.y, n.z}, h};
}

void drawCasts(const std::vector<CastInstance>& list) {
  if (list.empty()) return;
  if (!g_vao) {
    glGenVertexArrays(1, &g_vao);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, render::quadVaoBuffer());
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
    glGenBuffers(1, &g_ibo);
    glBindBuffer(GL_ARRAY_BUFFER, g_ibo);
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 32, nullptr);
    glVertexAttribDivisor(6, 1);
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, 32, (void*)16);
    glVertexAttribDivisor(7, 1);
  }
  glBindVertexArray(g_vao);
  glBindBuffer(GL_ARRAY_BUFFER, g_ibo);
  size_t bytes = list.size() * sizeof(CastInstance);
  if (bytes > g_cap) {
    g_cap = bytes * 2;
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)g_cap, nullptr, GL_DYNAMIC_DRAW);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, list.data());
  render::program("cast", CAST_VS, CAST_FS);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-2, -2);
  glBindVertexArray(g_vao);
  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)list.size());
  gfx::drawCalls++;
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void shutdownCasts() {
  if (g_ibo) glDeleteBuffers(1, &g_ibo);
  if (g_vao) glDeleteVertexArrays(1, &g_vao);
  g_ibo = g_vao = 0;
}
