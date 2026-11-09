#include "render/envmap.h"
#include "game/extras.h"
#include "game/events.h"
#include "game/relics.h"
#include "core/crashlog.h"
#include "core/version.h"
#include <atomic>
#include <algorithm>
#include <map>
#include "platform/platform.h"
#include "gfx/gfx.h"
#include "render/render.h"
#include "render/terrain_render.h"
#include "render/water.h"
#include "render/sky.h"
#include "render/sunfx.h"
#include "render/grass.h"
#include "world/terrain.h"
#include "world/forest.h"
#include "assets/assets.h"
#include "audio/audio.h"
#include "ui/ui.h"
#include "ui/font.h"
#include "ui/screens.h"
#include "game/engine.h"
#include "game/app.h"
#include "game/season.h"
#include "game/player.h"
#include "game/fx.h"
#include "game/props.h"
#include "game/resources.h"
#include "game/npcs.h"
#include "game/wildlife.h"
#include "game/archery.h"
#include "game/village.h"
#include "game/buildings.h"
#include "stb_image_write.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <ctime>
#include <functional>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <dxgi1_4.h>
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#else
#include <fstream>
#endif

namespace platform { void (*loadPump)() = nullptr; }
namespace {

struct SceneTarget {
  GLuint fbo = 0, color = 0, depth = 0, vel = 0, msFbo = 0, msColor = 0, msDepth = 0;
  int w = 0, h = 0, samples = 0;
  void ensure(int W, int H) {
    if (W == w && H == h && fbo) return;
    release();
    w = W; h = H;
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenTextures(1, &depth);
    glBindTexture(GL_TEXTURE_2D, depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    // motion vectors for temporal upscaling (fragment location 3 -> attachment 1, enabled per frame)
    glGenTextures(1, &vel);
    glBindTexture(GL_TEXTURE_2D, vel);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, vel, 0);
    { const GLenum one = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &one); }
    samples = 0;
    GLint maxS = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxS);
    int want = std::min((int)maxS, std::max(0, g_settings.msaa));
    if (want > 1) {
      glGenRenderbuffers(1, &msColor);
      glBindRenderbuffer(GL_RENDERBUFFER, msColor);
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, want, GL_RGBA8, w, h);
      glGenRenderbuffers(1, &msDepth);
      glBindRenderbuffer(GL_RENDERBUFFER, msDepth);
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, want, GL_DEPTH_COMPONENT24, w, h);
      glGenFramebuffers(1, &msFbo);
      glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth);
      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) samples = want;
      else { glDeleteFramebuffers(1, &msFbo); glDeleteRenderbuffers(1, &msColor); glDeleteRenderbuffers(1, &msDepth); msFbo = msColor = msDepth = 0; }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  GLuint drawFbo() const { return samples ? msFbo : fbo; }
  void resolve(GLbitfield bits) {
    if (!samples) return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, bits, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
  }
  void release() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (color) glDeleteTextures(1, &color);
    if (depth) glDeleteTextures(1, &depth);
    if (vel) glDeleteTextures(1, &vel);
    vel = 0;
    if (msFbo) glDeleteFramebuffers(1, &msFbo);
    if (msColor) glDeleteRenderbuffers(1, &msColor);
    if (msDepth) glDeleteRenderbuffers(1, &msDepth);
    fbo = color = depth = msFbo = msColor = msDepth = 0;
    w = h = 0;
  }
};


float rssMB() {
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) return pmc.WorkingSetSize / (1024.0f * 1024.0f);
  return 0;
#else
  std::ifstream f("/proc/self/status");
  std::string line;
  while (std::getline(f, line))
    if (line.rfind("VmRSS:", 0) == 0) return std::atof(line.c_str() + 6) / 1024.0f;
  return 0;
#endif
}

#ifdef _WIN32
IDXGIAdapter3* g_adapter = nullptr;
bool g_dxgiTried = false;
#endif

float vramMB(bool& exact) {
  exact = false;
#ifdef _WIN32
  if (!g_dxgiTried) {
    g_dxgiTried = true;
    HMODULE dll = LoadLibraryA("dxgi.dll");
    typedef HRESULT(WINAPI * CF)(REFIID, void**);
    CF create = dll ? (CF)(void*)GetProcAddress(dll, "CreateDXGIFactory1") : nullptr;
    IDXGIFactory1* f = nullptr;
    if (create && SUCCEEDED(create(__uuidof(IDXGIFactory1), (void**)&f))) {
      IDXGIAdapter1* a = nullptr;
      SIZE_T best = 0;
      for (UINT i = 0; f->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 d;
        a->GetDesc1(&d);
        IDXGIAdapter3* a3 = nullptr;
        if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && d.DedicatedVideoMemory > best && SUCCEEDED(a->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&a3))) {
          if (g_adapter) g_adapter->Release();
          g_adapter = a3;
          best = d.DedicatedVideoMemory;
        }
        a->Release();
      }
      f->Release();
    }
  }
  if (g_adapter) {
    DXGI_QUERY_VIDEO_MEMORY_INFO info;
    if (SUCCEEDED(g_adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info))) {
      exact = true;
      return info.CurrentUsage / 1048576.0f;
    }
  }
#endif
  static int nvx = -1;
  if (nvx < 0) {
    nvx = 0;
    GLint n = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    for (GLint i = 0; i < n; i++) {
      const char* e = (const char*)glGetStringi(GL_EXTENSIONS, i);
      if (e && !std::strcmp(e, "GL_NVX_gpu_memory_info")) nvx = 1;
    }
  }
  if (nvx == 1) {
    GLint total = 0, avail = 0;
    glGetIntegerv(0x9048, &total);
    glGetIntegerv(0x9049, &avail);
    if (total > 0) return (total - avail) / 1024.0f;
  }
  return -1;
}

void screenshot(const std::string& path) {
  int w = platform::width(), h = platform::height();
  std::vector<uint8_t> px((size_t)w * h * 3), flip((size_t)w * h * 3);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadBuffer(GL_BACK);
  glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
  for (int y = 0; y < h; y++) std::memcpy(&flip[(size_t)y * w * 3], &px[(size_t)(h - 1 - y) * w * 3], (size_t)w * 3);
  stbi_write_png(path.c_str(), w, h, 3, flip.data(), w * 3);
  std::printf("screenshot %s\n", path.c_str());
}

std::string timestampName() {
  std::time_t t = std::time(nullptr);
  char b[64];
  std::strftime(b, sizeof b, "screenshot_%Y%m%d_%H%M%S.png", std::localtime(&t));
  return platform::saveDir() + "/" + b;
}

const char* g_loadMsg = "";
float g_loadFrac = 0;
void loadingFrame(const char* msg, float frac = -1) {
  if (msg) g_loadMsg = msg;
  if (frac >= 0) g_loadFrac = frac;
  int W = platform::width(), H = platform::height();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, W, H);
  glDisable(GL_DEPTH_TEST);
  glClearColor(0.05f, 0.04f, 0.035f, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  float s = std::max(1.0f, H / 820.0f) * g_settings.uiScale;
  ui::begin(W, H, s);
  float w = ui::width(), h = ui::height();
  ui::text("HIMALAYAN WILDS", w / 2, h / 2 - 20, 40, ui::rgba("#fbbf24"), ui::SERIF, 1);
  ui::text(g_loadMsg, w / 2, h / 2 + 30, 14, ui::rgba("#a8a29e"), ui::SANS, 1);
  float bw = std::min(420.0f, w * 0.5f);
  ui::rect({w / 2 - bw / 2, h / 2 + 52, bw, 6}, ui::rgba("#292524"), 3);
  ui::rect({w / 2 - bw / 2, h / 2 + 52, bw * clampf(g_loadFrac, 0, 1), 6}, ui::rgba("#fbbf24"), 3);
  // moving dot: shows the game is alive even inside a long step
  float ph = (float)std::fmod(platform::time() * 0.8, 1.0);
  ui::rect({w / 2 - bw / 2 + (bw - 24) * ph, h / 2 + 62, 24, 3}, ui::rgba("#78716c"), 2);
  ui::end();
  platform::swap();
  platform::pollEvents();
  glEnable(GL_DEPTH_TEST);
}
// called from inside long load steps: redraw the loading screen at most ~20 times a second
void pumpLoading() {
  static double last = 0;
  double t = platform::time();
  if (t - last < 0.05) return;
  last = t;
  // called from inside a UI frame (a menu action that loads): drawing the loading screen here would flush and
  // re-scale that frame's half-built text, so only keep the window responsive
  if (ui::inFrame()) { platform::pollEvents(); return; }
  // may be called in the middle of off-screen GL work (bake, impostor capture): save and restore the GL state it touches
  GLint fbo = 0, rfbo = 0, vp[4], prog = 0, vao = 0, active = 0, tex[8], arr = 0;
  GLboolean depth = glIsEnabled(GL_DEPTH_TEST), blend = glIsEnabled(GL_BLEND), scis = glIsEnabled(GL_SCISSOR_TEST), cull = glIsEnabled(GL_CULL_FACE), dmask = GL_TRUE;
  GLint bsrc = 0, bdst = 0;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &rfbo);
  glGetIntegerv(GL_VIEWPORT, vp);
  glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arr);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &dmask);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &bsrc);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &bdst);
  for (int i = 0; i < 8; i++) { glActiveTexture(GL_TEXTURE0 + i); glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex[i]); }
  glDisable(GL_SCISSOR_TEST);
  loadingFrame(nullptr);
  for (int i = 0; i < 8; i++) { glActiveTexture(GL_TEXTURE0 + i); glBindTexture(GL_TEXTURE_2D, (GLuint)tex[i]); }
  glActiveTexture((GLenum)active);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)fbo);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)rfbo);
  glViewport(vp[0], vp[1], vp[2], vp[3]);
  glUseProgram((GLuint)prog);
  glBindVertexArray((GLuint)vao);
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)arr);
  depth ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
  blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
  scis ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
  cull ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
  glDepthMask(dmask);
  glBlendFunc((GLenum)bsrc, (GLenum)bdst);
}

bool g_prof = false;
std::map<std::string, double> g_profMs;
std::map<std::string, long long> g_profTri, g_profDraw;
struct PassProf {
  static const int RING = 4, MAXP = 512;
  GLuint q[RING][MAXP * 2] = {};
  const char* names[RING][MAXP] = {};
  double cpu[RING][MAXP] = {};
  int n[RING] = {};
  int cur = 0;
  bool on = false;
  std::map<std::string, double> gAcc, cAcc;
  int frames = 0;
  void begin() {
    if (!q[0][0]) for (int r = 0; r < RING; r++) glGenQueries(MAXP * 2, q[r]);
    cur = (cur + 1) % RING;
    n[cur] = 0;
  }
  int open(const char* name) {
    if (n[cur] >= MAXP) return -1;
    int i = n[cur]++;
    names[cur][i] = name;
    glQueryCounter(q[cur][i * 2], GL_TIMESTAMP);
    cpu[cur][i] = platform::time();
    return i;
  }
  void close(int i) {
    if (i < 0) return;
    cpu[cur][i] = (platform::time() - cpu[cur][i]) * 1000;
    glQueryCounter(q[cur][i * 2 + 1], GL_TIMESTAMP);
  }
  void collect() {
    int r = (cur + 2) % RING;
    if (!n[r]) return;
    GLint ok = 0;
    glGetQueryObjectiv(q[r][n[r] * 2 - 1], GL_QUERY_RESULT_AVAILABLE, &ok);
    if (!ok) return;
    for (int i = 0; i < n[r]; i++) {
      GLuint64 a = 0, b = 0;
      glGetQueryObjectui64v(q[r][i * 2], GL_QUERY_RESULT, &a);
      glGetQueryObjectui64v(q[r][i * 2 + 1], GL_QUERY_RESULT, &b);
      gAcc[names[r][i]] += (b - a) / 1e6;
      cAcc[names[r][i]] += cpu[r][i];
    }
    n[r] = 0;
    frames++;
  }
  static std::string top(const std::map<std::string, double>& m, int frames, const char* tag) {
    std::vector<std::pair<double, std::string>> v;
    for (auto& kv : m) v.push_back({kv.second / std::max(1, frames), kv.first});
    std::sort(v.rbegin(), v.rend());
    std::string out = tag;
    char b[96];
    for (size_t i = 0; i < v.size() && i < 10; i++) {
      std::string nm = v[i].second;
      size_t p = nm.find("::");
      if (p != std::string::npos) nm = nm.substr(0, p) + "." + nm.substr(p + 2);
      p = nm.find('(');
      if (p != std::string::npos) nm = nm.substr(0, p);
      std::snprintf(b, sizeof b, "  %s %.1f", nm.c_str(), v[i].first);
      out += b;
    }
    return out;
  }
  void publish() {
    if (frames) screens::setPassText(top(gAcc, frames, "GPU ms:"), top(cAcc, frames, "CPU ms:"));
    gAcc.clear(); cAcc.clear(); frames = 0;
  }
} g_pp;
#define PROF(x) do { if (g_pp.on && !g_prof) { int i_ = g_pp.open(#x); x; g_pp.close(i_); } else if (g_prof) { glFinish(); double t0_ = platform::time(); long long tr_ = gfx::triangles, dr_ = gfx::drawCalls; x; glFinish(); g_profDraw[#x] += gfx::drawCalls - dr_; g_profMs[#x] += (platform::time() - t0_) * 1000; g_profTri[#x] += gfx::triangles - tr_; } else { x; } } while (0)
void drawShadows() {
  bool staticRebuilt = false;
  if (sky::shadowStaticDirty()) {
    sky::onStaticRebuild();
    render::uploadFrame();
    render::beginShadowPass(sky::shadowMatrix(), g_settings.shadowSize, 1);
    PROF(forest::drawShadow());
    PROF(props::drawShadow());
    PROF(buildings::drawShadow());
    PROF(village::drawShadowStatic());
    PROF(resources::drawShadow());
    render::endShadowPass();
    sky::clearStaticShadow();
    staticRebuilt = true;
  }
  int sdx = 0, sdy = 0; float sdz = 0;
  if (!staticRebuilt && sky::shadowScroll(sdx, sdy, sdz)) {
    // walked a little: shift the cached static shadow and draw only the strips that scrolled in
    const int S = g_settings.shadowSize;
    render::scrollStaticShadow(sdx, sdy, sdz);
    render::uploadFrame();
    render::beginShadowPass(sky::shadowMatrix(), S, 3);
    glEnable(GL_SCISSOR_TEST);
    int rects[2][4]; int nr = 0;
    if (sdx) { rects[nr][0] = sdx > 0 ? S - sdx : 0; rects[nr][1] = 0; rects[nr][2] = std::abs(sdx); rects[nr][3] = S; nr++; }
    if (sdy) { rects[nr][0] = 0; rects[nr][1] = sdy > 0 ? S - sdy : 0; rects[nr][2] = S; rects[nr][3] = std::abs(sdy); nr++; }
    for (int i = 0; i < nr; i++) {
      glScissor(rects[i][0], rects[i][1], rects[i][2], rects[i][3]);
      PROF(forest::drawShadow());
      PROF(props::drawShadow());
      PROF(buildings::drawShadow());
      PROF(village::drawShadowStatic());
      PROF(resources::drawShadow());
    }
    glDisable(GL_SCISSOR_TEST);
    render::endShadowPass();
    sky::clearShadowScroll();
    staticRebuilt = true;  // the dynamic layer must be redone on top of the shifted cache
  }
  // moving things' shadows: redrawn every 2nd frame while the shadow map has not moved (a 1-frame lag at
  // walking speed is ~3 cm, invisible), every frame when it has
  static Mat4 lastDyn; static bool haveDyn = false; static bool staticWas = false;
  Mat4 sm = sky::shadowMatrix();
  bool same = haveDyn && std::memcmp(lastDyn.m, sm.m, sizeof sm.m) == 0 && !staticRebuilt;
  (void)staticWas;
  if (same && (render::frameNo & 1)) return;
  lastDyn = sm; haveDyn = true;
  render::beginShadowPass(sm, g_settings.shadowSize, 2);
  PROF(village::drawShadow());
  PROF(npcs::drawShadow());
  PROF(wildlife::drawShadow());
  render::endShadowPass();
}

GLuint g_sunDepth = 0;
SceneTarget* g_scene = nullptr;
// ---- quarter-resolution SSAO: soft contact shadows in creases, under canopies and where objects meet the ground ----
struct AOTarget { GLuint fbo = 0, tex = 0; int w = 0, h = 0; } g_ao;
void ssaoPass() {
  SceneTarget* sc = g_scene;
  if (!sc || !sc->fbo || !g_settings.ssao || std::getenv("HW_NOAO")) return;
  GLint cur = 0;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &cur);
  if ((GLuint)cur != sc->drawFbo()) return;  // only when the world is rendered into the scene target
  int w = std::max(1, sc->w / 2), h = std::max(1, sc->h / 2);  // half res each way = quarter of the pixels
  if (g_ao.w != w || g_ao.h != h) {
    if (g_ao.fbo) { glDeleteFramebuffers(1, &g_ao.fbo); glDeleteTextures(1, &g_ao.tex); }
    glGenTextures(1, &g_ao.tex);
    glBindTexture(GL_TEXTURE_2D, g_ao.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &g_ao.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_ao.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_ao.tex, 0);
    g_ao.w = w; g_ao.h = h;
  }
  sc->resolve(GL_DEPTH_BUFFER_BIT);
  static const char* VS = "out vec2 v_uv;\nvoid main(){ vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); v_uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }\n";
  static const char* AO_FS = R"(in vec2 v_uv;
uniform sampler2D u_depth;
uniform vec2 u_nf;
uniform float u_tanY;
uniform float u_aspect;
out vec4 o;
float lin(float d){ return u_nf.x * u_nf.y / (u_nf.y - d * (u_nf.y - u_nf.x)); }
void main(){
  float d = texture(u_depth, v_uv).r;
  if (d >= 0.99999) { o = vec4(1.0); return; }
  float z = lin(d);
  if (z > 70.0) { o = vec4(1.0); return; }
  float r = 0.9 / (z * u_tanY * 2.0);           // ~0.9 m world radius in screen space
  float ang = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.55) * 6.2831;
  float occ = 0.0;
  for (int i = 0; i < 8; i++) {
    float a = ang + float(i) * 2.39996, rr = r * sqrt((float(i) + 0.5) / 8.0);
    vec2 uv = v_uv + vec2(cos(a) / u_aspect, sin(a)) * rr;
    float zs = lin(texture(u_depth, uv).r);
    float dz = z - zs;
    occ += smoothstep(0.04, 0.25, dz) * (1.0 - smoothstep(0.9, 2.2, dz));
  }
  float ao = 1.0 - occ / 8.0 * 0.85;
  o = vec4(mix(ao, 1.0, smoothstep(45.0, 70.0, z)));
}
)";
  static const char* MUL_FS = R"(in vec2 v_uv;
uniform sampler2D u_ao;
uniform vec2 u_px;
out vec4 o;
void main(){
  float a = texture(u_ao, v_uv).r * 0.4;
  a += texture(u_ao, v_uv + vec2(u_px.x, 0.0)).r * 0.15 + texture(u_ao, v_uv - vec2(u_px.x, 0.0)).r * 0.15;
  a += texture(u_ao, v_uv + vec2(0.0, u_px.y)).r * 0.15 + texture(u_ao, v_uv - vec2(0.0, u_px.y)).r * 0.15;
  o = vec4(vec3(a), 1.0);
}
)";
  GLint vp[4];
  glGetIntegerv(GL_VIEWPORT, vp);
  glBindFramebuffer(GL_FRAMEBUFFER, g_ao.fbo);
  glViewport(0, 0, w, h);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_BLEND);
  Shader& a = render::program("ssao", VS, AO_FS);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sc->depth);
  a.set1i("u_depth", 0);
  a.set2f("u_nf", render::cam.nearZ, render::cam.farZ);
  a.set1f("u_tanY", 1.0f / std::max(1e-3f, render::cam.proj.at(1, 1)));
  a.set1f("u_aspect", (float)sc->w / std::max(1, sc->h));
  render::drawFullscreen();
  glBindFramebuffer(GL_FRAMEBUFFER, sc->drawFbo());
  glViewport(vp[0], vp[1], vp[2], vp[3]);
  Shader& m = render::program("ssaomul", VS, MUL_FS);
  glBindTexture(GL_TEXTURE_2D, g_ao.tex);
  m.set1i("u_ao", 0);
  m.set2f("u_px", 1.0f / w, 1.0f / h);
  glEnable(GL_BLEND);
  glBlendFunc(GL_DST_COLOR, GL_ZERO);
  render::drawFullscreen();
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
}
// ---- temporal upscaling (TAAU): the world renders at a lower resolution with a different sub-pixel jitter each
// frame; a resolve pass at window resolution reprojects last frame's image with the depth buffer, clamps it to
// this frame's local colour range (no ghosts) and blends 1/10 of the new frame in. Detail builds up over ~8
// frames, so ~55-60% internal resolution looks close to native. Replaces the full-res FXAA pass.
struct Taa {
  GLuint tex[2] = {}, fbo[2] = {};
  int w = 0, h = 0, cur = 0;
  bool valid = false;
  Mat4 prevVP;
  Vec3 prevPos;
  float jx = 0, jy = 0;  // this frame's jitter, in internal pixels
  void ensure(int W, int H) {
    if (W == w && H == h && fbo[0]) return;
    release();
    w = W; h = H;
    for (int i = 0; i < 2; i++) {
      glGenTextures(1, &tex[i]);
      glBindTexture(GL_TEXTURE_2D, tex[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glGenFramebuffers(1, &fbo[i]);
      glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[i], 0);
      { const float z[4] = {0, 0, 0, 1}; glClearBufferfv(GL_COLOR, 0, z); }  // fresh memory can hold NaNs
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    valid = false;
  }
  void release() {
    for (int i = 0; i < 2; i++) { if (fbo[i]) glDeleteFramebuffers(1, &fbo[i]); if (tex[i]) glDeleteTextures(1, &tex[i]); fbo[i] = tex[i] = 0; }
    w = h = 0; valid = false;
  }
} g_taa;
static float halton(int i, int b) { float f = 1, r = 0; while (i > 0) { f /= b; r += f * (i % b); i /= b; } return r; }
const char* TAA_FS = R"(in vec2 v_uv;
uniform sampler2D u_cur, u_depth, u_hist, u_vel;
uniform mat4 u_invVP, u_taaPrevVP;  // not u_prevVP: that name is in the frame block (NVIDIA rejects the redefinition)
uniform vec2 u_jit, u_ipx, u_nf, u_opx;
uniform float u_valid;
out vec4 o;
vec3 toY(vec3 c){ return vec3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b, 0.5 * c.r - 0.5 * c.b, -0.25 * c.r + 0.5 * c.g - 0.25 * c.b); }
vec3 fromY(vec3 y){ return vec3(y.x + y.y - y.z, y.x + y.z, y.x - y.y - y.z); }
// Catmull-Rom history fetch (5 bilinear taps): keeps the accumulated image sharp instead of blurring a little
// every frame the way a plain bilinear re-read does
vec3 histCR(vec2 uv){
  vec2 sz = 1.0 / u_opx, p = uv * sz, tc = floor(p - 0.5) + 0.5, f = p - tc;
  vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f)), w1 = 1.0 + f * f * (-2.5 + 1.5 * f), w2 = f * (0.5 + f * (2.0 - 1.5 * f)), w3 = f * f * (-0.5 + 0.5 * f);
  vec2 w12 = w1 + w2, t0 = (tc - 1.0) * u_opx, t3 = (tc + 2.0) * u_opx, t12 = (tc + w2 / w12) * u_opx;
  vec3 r = texture(u_hist, vec2(t12.x, t0.y)).rgb * (w12.x * w0.y)
         + texture(u_hist, vec2(t0.x, t12.y)).rgb * (w0.x * w12.y)
         + texture(u_hist, t12).rgb * (w12.x * w12.y)
         + texture(u_hist, vec2(t3.x, t12.y)).rgb * (w3.x * w12.y)
         + texture(u_hist, vec2(t12.x, t3.y)).rgb * (w12.x * w3.y);
  float wt = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
  return max(r / wt, 0.0);
}
void main(){
  vec2 cuv = v_uv - u_jit;  // undo this frame's jitter (a +j column-2 offset moves the image by -j pixels)
  vec3 c = toY(texture(u_cur, cuv).rgb);
  vec3 a = toY(texture(u_cur, cuv + vec2(u_ipx.x, 0.0)).rgb), b = toY(texture(u_cur, cuv - vec2(u_ipx.x, 0.0)).rgb);
  vec3 d = toY(texture(u_cur, cuv + vec2(0.0, u_ipx.y)).rgb), e = toY(texture(u_cur, cuv - vec2(0.0, u_ipx.y)).rgb);
  vec3 mn = min(c, min(min(a, b), min(d, e))), mx = max(c, max(max(a, b), max(d, e)));
  // variance clip: a tighter box than min/max for flat areas, so stale colour cannot linger
  vec3 m1 = (c + a + b + d + e) * 0.2, m2 = (c * c + a * a + b * b + d * d + e * e) * 0.2;
  vec3 sd = sqrt(max(m2 - m1 * m1, 0.0));
  mn = max(mn, m1 - 1.25 * sd); mx = min(mx, m1 + 1.25 * sd);
  vec4 mv = texture(u_vel, cuv);
  if (any(isnan(mv)) || any(isinf(mv))) mv = vec4(0.0);
  vec2 puv;
  float moving = 0.0;
  if (mv.z > 0.5 && mv.z < 1.5) {
    puv = v_uv - mv.xy;  // leaves in the wind: exact motion from the shader
  } else {
    // static world: rebuild motion from depth (nearest depth of the cross keeps edges with their object)
    float dep = texture(u_depth, cuv).r;
    dep = min(dep, min(min(texture(u_depth, cuv + vec2(u_ipx.x, 0.0)).r, texture(u_depth, cuv - vec2(u_ipx.x, 0.0)).r),
                       min(texture(u_depth, cuv + vec2(0.0, u_ipx.y)).r, texture(u_depth, cuv - vec2(0.0, u_ipx.y)).r)));
    float zn = min(dep, 0.99995) * 2.0 - 1.0;
    vec4 wp = u_invVP * vec4(v_uv * 2.0 - 1.0, zn, 1.0);
    wp /= wp.w;
    vec4 pc = u_taaPrevVP * wp;
    puv = pc.w > 1e-4 ? pc.xy / pc.w * 0.5 + 0.5 : vec2(-1.0);
    float lin = 2.0 * u_nf.x * u_nf.y / (u_nf.y + u_nf.x - (dep * 2.0 - 1.0) * (u_nf.y - u_nf.x));
    if (lin < 1.3) puv = v_uv;  // the bow and hands move with the camera
    moving = mv.z > 1.5 ? 1.0 : 0.0;  // animals, people, grass: no vector, lean on this frame
  }
  // a bad motion vector or a degenerate reprojection (NaN/Inf) must never reach the history: one NaN pixel
  // would spread and stay forever (the whole world turns black)
  bool off = !(puv.x >= 0.0 && puv.y >= 0.0 && puv.x <= 1.0 && puv.y <= 1.0);
  if (off) puv = v_uv;
  vec3 h = toY(histCR(puv));
  if (any(isnan(h)) || any(isinf(h))) { h = c; off = true; }
  vec3 hc = min(max(h, mn), mx);  // max/min (not clamp) also scrub a NaN on GPUs that optimise isnan away
  float rej = clamp(length(h - hc) * 5.0, 0.0, 1.0);
  float alpha = (u_valid < 0.5 || off) ? 1.0 : mix(mix(0.08, 0.3, moving), 0.5, rej);
  // alpha 1 must not touch the history at all: NaN * 0 is still NaN (uninitialised history texture)
  vec3 r = alpha >= 1.0 ? fromY(c) : fromY(mix(hc, c, alpha));
  if (any(isnan(r)) || any(isinf(r))) r = fromY(c);
  o = vec4(clamp(r, 0.0, 4.0), 1.0);
}
)";

int g_sunW = 0, g_sunH = 0;
void drawWorld(float dt, bool viewModel) {
  fx::beginFrame();
  const Color hz = sky::horizon();
  glClearColor(Color::linearToSrgb(hz.r), Color::linearToSrgb(hz.g), Color::linearToSrgb(hz.b), 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  envmap::setCameraHeight(render::cam.pos.y);
  envmap::hazeCullDistance(render::cam.pos.x, render::cam.pos.z, dt);
  PROF(render::bindShadow());
  PROF(grass::draw());
  PROF(forest::drawOpaque());
  PROF(props::draw());
  PROF(relics::draw());
  PROF(extras::draw());
  PROF(events::drawCaravan());
  PROF(buildings::draw());
  PROF(village::drawStructures());
  PROF(village::drawResidents());
  PROF(npcs::draw());
  PROF(wildlife::draw());
  PROF(resources::draw());
  PROF(archery::draw());
  PROF(terrain_render::draw());
  PROF(sky::drawSky());
  PROF(water::draw());
  PROF(forest::drawBlobs());
  PROF(props::drawCasters());
  PROF(fx::drawBlobs());
  PROF(village::addLights());
  PROF(buildings::drawPreview());
  PROF(fx::drawTransparent());
  PROF(props::drawGlow());
  PROF(fx::drawGlows());
  PROF(village::drawFestival());
  PROF(sky::drawStarsAndMist());
  PROF(sky::drawWeather(dt));
  PROF(water::drawParticles());
  if (g_sunDepth) { if (g_scene) g_scene->resolve(GL_DEPTH_BUFFER_BIT); PROF(sunfx::occlusion(g_sunDepth, g_sunW, g_sunH)); }
  // hierarchical-Z: last opaque depth (before the bow) feeds next frame's occlusion culling
  if (g_scene && g_scene->fbo && !g_scene->samples) {
    GLint cur = 0; glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &cur);
    // every 2nd frame: at 30 fps that is still 15 occlusion snapshots a second, at half the CPU / driver cost
    if ((GLuint)cur == g_scene->fbo && (render::frameNo & 1) == 0) PROF(render::hizCapture(g_scene->depth, g_scene->w, g_scene->h));
  }
  if (viewModel) archery::drawViewModel(dt, player::cameraWorld());
}

struct Shot { int frame; std::string name; std::function<void()> setup; };

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  timeBeginPeriod(1);
#endif
  if (!platform::init(argc, argv)) return 1;
  crashlog::init(platform::saveDir());
  crashlog::note(std::string(HW_VERSION_FULL) + " started on " + (const char*)glGetString(GL_RENDERER));
  gfx::init();
  render::init();
  fonts::init();
  ui::init();
  screens::init();
  audio::init();
  if (A.sfxMuted) audio::toggleSfxMute();

  auto mem = [](const char* tag) { if (platform::hasArg("--memlog")) std::printf("[mem] %-12s %.0f MB  t=%.2fs\n", tag, rssMB(), platform::time()); };
  mem("gl+ui");
  platform::loadPump = pumpLoading;
  loadingFrame("Shaping the valley...", 0.02f);
  {
    // terrain generation is pure CPU: run it on a worker while the window keeps redrawing
    std::atomic<bool> done{false};
    std::thread gen([&] { world::generate(); done = true; });
    while (!done) { pumpLoading(); std::this_thread::sleep_for(std::chrono::milliseconds(15)); }
    gen.join();
  }
  mem("terrain");
  loadingFrame("Growing the forests...", 0.25f);
  std::vector<AssetRequest> reqs = forest::requiredFiles();
  for (auto& f : resources::requiredFiles()) {
    bool have = false;
    for (auto& r : reqs) if (r.file == f) have = true;
    if (!have) reqs.push_back({f, f.find("Rock") != std::string::npos ? "rock" : "under", -1});
  }
  {
    auto ov = assets::procOverrides();
    reqs.erase(std::remove_if(reqs.begin(), reqs.end(), [&](const AssetRequest& r) { return std::find(ov.begin(), ov.end(), r.file) != ov.end(); }), reqs.end());
  }
  assets::loadAll(reqs);
  assets::buildProcTrees();
  for (auto& m : assets::missing()) std::fprintf(stderr, "missing asset: %s (place it in assets/models)\n", m.c_str());

  mem("assets");
  loadingFrame("Painting the ground...", 0.45f);
  envmap::build();
  terrain_render::init();
  loadingFrame("Filling the rivers...", 0.5f);
  water::init();
  sky::init();
  loadingFrame("Planting the forest...", 0.55f);
  forest::precomputeItems();
  forest::init();
  loadingFrame("Growing the grass...", 0.65f);
  grass::init();
  fx::init();
  props::init();
  loadingFrame("Drawing the map...", 0.68f);
  screens::prebuildMap();
  loadingFrame("Raising the village...", 0.7f);
  resources::initRender();
  wildlife::initRender();
  npcs::initRender();
  village::initRender();
  archery::initRender();
  app::init();
  wildlife::spawn(7);
  resources::spawn(11);
  npcs::reset();
  ambience::start();
  mem("init");

  std::string selftest = platform::argValue("--selftest");
  bool test = !selftest.empty();
  std::vector<Shot> shots;
  if (test) {
    auto look = [](float x, float z, float yaw, float pitch) {
      return [=] { E.player.pos = {x, world::getTerrainHeight(x, z) + 1.7f, z}; E.player.yaw = yaw; E.player.pitch = pitch; };
    };
    shots = {
        {10, "title", [=] {}},
        {20, "season", [=] { screens::setTitlePage(1); }},
        {30, "", [=] { screens::setTitlePage(0); app::newGame(); }},
        {200, "village", look(0, 14, 0, -0.08f)},
        {206, "summertrees", [=] { restoreSeason(1, 2, 12.0f); forest::seasonJumped(); E.time = 12.0f; E.cutscene.dur = 0; look(68.6f, -39.1f, 3.6f, -0.02f)(); }},
        {210, "", [=] { E.time = 6.3f; E.cutscene.dur = 0; look(68.6f, -39.1f, 3.6f, -0.02f)(); }},
        {216, "t_dawn", [=] {}},
        {219, "", [=] { E.time = 9.0f; E.cutscene.dur = 0; look(68.6f, -39.1f, 3.6f, -0.02f)(); }},
        {225, "t_morning", [=] {}},
        {228, "", [=] { E.time = 15.5f; E.cutscene.dur = 0; look(68.6f, -39.1f, 3.6f, -0.02f)(); }},
        {234, "t_afternoon", [=] {}},
        {237, "", [=] { E.time = 18.3f; E.cutscene.dur = 0; look(68.6f, -39.1f, 3.6f, -0.02f)(); }},
        {243, "t_dusk", [=] {}},
        {246, "", [=] { E.time = 22.5f; E.cutscene.dur = 0; look(68.6f, -39.1f, 3.6f, -0.02f)(); }},
        {252, "t_night", [=] {}},
        {255, "", [=] { restoreSeason(1, 2, 7.0f); E.time = 7.0f; E.cutscene.dur = 0; const auto& R = world::RIVER; size_t i = R.pts.size() * 35 / 100; float tx = R.pts[i + 1][0] - R.pts[i][0], tz = R.pts[i + 1][1] - R.pts[i][1]; float l = std::sqrt(tx * tx + tz * tz) + 1e-4f; tx /= l; tz /= l; float x = R.pts[i][0] - tz * (R.hw[i] + 9) - tx * 12, z = R.pts[i][1] + tx * (R.hw[i] + 9) - tz * 12; look(x, z, std::atan2(-(R.pts[i][0] + tx * 25 - x), -(R.pts[i][1] + tz * 25 - z)), -0.12f)(); }},
        {258, "river", [=] {}},
        {263, "", [=] { restoreSeason(1, 2, 17.5f); E.time = 17.5f; E.cutscene.dur = 0; if (!world::LAKES.empty()) { const Lake& L = world::LAKES[0]; float x = L.x + L.r + 8, z = L.z + 4; look(x, z, std::atan2(-(L.x - x), -(L.z - z)), -0.1f)(); } }},
        {266, "lake", [=] {}},
        {268, "", [=] { restoreSeason(1, 2, 21.0f); E.time = 21.0f; E.cutscene.dur = 0; look(0, 14, 0, -0.12f)(); }},
        {274, "vnight", [=] {}},
        {276, "", [=] { restoreSeason(1, 2, 15.4f); E.time = 15.4f; E.cutscene.dur = 0; float x = -184.3f, z = -67.4f, bx = x, bz = z - 30, bd = 1e9f; for (const Lake& L : world::LAKES) { float d = hypot2(L.x - x, L.z - z); if (d < bd) { bd = d; bx = L.x; bz = L.z; } } look(x, z, std::atan2(-(bx - x), -(bz - z)), -0.05f)(); }},
        {278, "lake2", [=] {}},
        {284, "", [=] { restoreSeason(1, 2, 22.5f); E.time = 22.5f; E.cutscene.dur = 0; E.lanternOn = true; look(68.6f, -39.1f, 3.6f, -0.15f)(); }},
        {288, "lantern", [=] {}},
        {800, "", [=] { restoreSeason(1, 2, 12.0f); forest::seasonJumped(); E.time = 12.0f; E.cutscene.dur = 0; look(68.6f, -39.1f, 3.6f, -0.02f)(); }},
        {900, "m_meadow", [=] {}},
        {905, "", [=] { E.time = 12.0f; look(40, -60, 2.2f, -0.05f)(); }},
        {1005, "m_forest", [=] {}},
        {1010, "", [=] { E.time = 12.0f; look(0, 14, 0, -0.08f)(); }},
        {1110, "m_village", [=] {}},
        {296, "", [=] { restoreSeason(2, 1, 17.5f); E.time = 17.0f; E.cutscene.dur = 0; float x = -71.4f, z = -233.7f; RiverHit q = world::riverQuery(x, z); float tx = x + q.tx * 14, tz = z + q.tz * 14; look(x, z, std::atan2(-(tx - x), -(tz - z)), -0.3f)(); }},
        {300, "rv2", [=] {}},
        {205, "res", [=] { A.tab = Tab::None; A.population = 12; village::syncResidents(); E.cutscene.dur = 0; restoreSeason(1, 2, 11.0f); E.time = 11.0f; look(0, 44, 0.0f, -0.1f)(); }},
        {260, "forest", look(40, -60, 2.2f, -0.05f)},
        {262, "sun", [=] { E.time = 9.5f; float ang = (9.5f - 6) / 24 * TAU; Vec3 sd = Vec3(std::cos(ang), std::sin(ang), -0.32f).normalized(); look(4, 18, std::atan2(-sd.x, -sd.z), std::asin(sd.y) * 0.85f)(); }},
        {270, "sun2", [=] {}},
        {272, "sunshade", [=] { E.time = 8.2f; float ang = (8.2f - 6) / 24 * TAU; Vec3 sd = Vec3(std::cos(ang), std::sin(ang), -0.32f).normalized(); std::vector<float> t; forest::nearTrees(40, -60, 60, t, 8); float tx = 40, tz = -60; if (t.size() >= 4) { tx = t[0]; tz = t[2]; } Vec3 h = Vec3(sd.x, 0, sd.z).normalized(); float x = tx - h.x * 5.0f, z = tz - h.z * 5.0f; look(x, z, std::atan2(-sd.x, -sd.z), std::asin(sd.y))(); }},
        {280, "sunshade2", [=] {}},
        {282, "suntemple", [=] { E.time = 7.2f; E.cutscene.dur = 0; float ang = (7.2f - 6) / 24 * TAU; Vec3 sd = Vec3(std::cos(ang), std::sin(ang), -0.32f).normalized(); const Shrine& sh = world::SHRINES[0]; Vec3 h = Vec3(sd.x, 0, sd.z).normalized(); float x = sh.x - h.x * 14.0f, z = sh.z - h.z * 14.0f; E.player.pos = {x, std::max(world::getTerrainHeight(x, z), sh.y) + 1.7f, z}; E.player.velY = 0; E.player.yaw = std::atan2(-sd.x, -sd.z); E.player.pitch = std::asin(sd.y); }},
        {292, "suntemple2", [=] {}},
        {294, "fdusk", [=] { restoreSeason(0, 2, 18.2f); E.time = 18.2f; look(40, -60, 2.2f, -0.02f)(); }},
        {330, "fspring", [=] { restoreSeason(0, 2, 8.0f); E.time = 8.0f; look(40, -60, 2.2f, -0.02f)(); }},
        {345, "fspring2", [=] {}},
        {350, "ftree", [=] { restoreSeason(0, 2, 13.0f); E.time = 13.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 34, z = tz + 6; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.08f)(); }},
        {365, "ftree2", [=] {}},
        {590, "rock", [=] { g_settings.undergrowthRadius = 0; forest::markDirty(); restoreSeason(0, 1, 10.5f); E.time = 10.5f; float rx = 40, rz = -60, rs = 1; forest::nearestRock(4, 30, rx, rz, rs); float x = rx - 1.2f * rs - 1.0f, z = rz - 1.0f * rs - 0.6f; look(x, z, std::atan2(-(rx - x), -(rz - z)), -0.5f)(); }},
        {700, "", [=] { restoreSeason(1, 1, 6.1f); E.time = 6.1f; E.cutscene.dur = 0; const Shrine& sh = world::SHRINES[world::SHRINES.size() > 1 ? 1 : 0]; float L = std::sqrt(sh.x * sh.x + sh.z * sh.z) + 1e-3f; float x = sh.x - sh.x / L * 16, z = sh.z - sh.z / L * 16; E.player.pos = {x, std::max(world::getTerrainHeight(x, z), sh.y) + 1.7f, z}; E.player.velY = 0; E.player.yaw = std::atan2(sh.x, sh.z); E.player.pitch = -0.3f; }},
        {712, "peak", [=] { E.player.velY = 0; }},
        {713, "", [=] { E.time = 7.4f; }},
        {725, "peak2", [=] { E.player.velY = 0; }},
        {726, "", [=] { E.player.pitch = 0.35f; }},
        {738, "sky", [=] {}},
        {606, "tday", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; E.cutscene.dur = 0; const Shrine& sh = world::SHRINES[0]; float x = sh.x + std::sin(sh.yaw) * 11.0f + 3.0f, z = sh.z + std::cos(sh.yaw) * 11.0f + 2.0f; E.player.pos = {x, std::max(world::getTerrainHeight(x, z), sh.y) + 1.7f, z}; E.player.velY = 0; E.player.yaw = std::atan2(-(sh.x - x), -(sh.z - z)); E.player.pitch = 0.1f; }},
        {620, "tday2", [=] {}},
        {621, "", [=] { E.player.pitch = -0.6f; }},
        {634, "tfloor", [=] {}},
        {636, "", [=] { restoreSeason(1, 1, 18.6f); E.time = 18.6f; float ang = (18.6f - 6) / 24 * TAU; Vec3 sd = Vec3(std::cos(ang), std::sin(ang), -0.32f).normalized(); look(4, 18, std::atan2(-sd.x, -sd.z), 0.05f)(); }},
        {648, "sset", [=] {}},
        {605, "rock2", [=] { g_settings.undergrowthRadius = 0; forest::markDirty(); restoreSeason(0, 1, 10.5f); E.time = 10.5f; float rx = 40, rz = -60, rs = 1; forest::nearestRock(4, 30, rx, rz, rs); float x = rx - 1.2f * rs - 1.0f, z = rz - 1.0f * rs - 0.6f; look(x, z, std::atan2(-(rx - x), -(rz - z)), -0.5f)(); }},
        {370, "tclose", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 7, z = tz + 2; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.3f)(); }},
        {385, "tclose2", [=] {}},
        {390, "tmid", [=] { std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 22, z = tz + 5; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.1f)(); }},
        {405, "tmid2", [=] {}},
        {410, "", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 14.0f, z = tz; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.12f)(); }},
        {424, "sw14", [=] {}},
        {425, "", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 18.0f, z = tz; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.12f)(); }},
        {439, "sw18", [=] {}},
        {440, "", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 22.0f, z = tz; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.12f)(); }},
        {454, "sw22", [=] {}},
        {455, "", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 26.0f, z = tz; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.12f)(); }},
        {469, "sw26", [=] {}},
        {470, "", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 30.0f, z = tz; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.12f)(); }},
        {484, "sw30", [=] {}},
        {485, "", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 34.0f, z = tz; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.12f)(); }},
        {499, "sw34", [=] {}},
        {500, "", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 38.0f, z = tz; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.12f)(); }},
        {514, "sw38", [=] {}},
        {515, "", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 42.0f, z = tz; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.12f)(); }},
        {529, "sw42", [=] {}},
        {530, "", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 46.0f, z = tz; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.12f)(); }},
        {544, "sw46", [=] {}},
        {545, "", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 50.0f, z = tz; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.12f)(); }},
        {559, "sw50", [=] {}},
        {560, "", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 54.0f, z = tz; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.12f)(); }},
        {574, "sw54", [=] {}},
        {575, "", [=] { restoreSeason(0, 1, 11.0f); E.time = 11.0f; std::vector<float> t; forest::nearTrees(40, -60, 80, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 58.0f, z = tz; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.12f)(); }},
        {589, "sw58", [=] {}},
        {306, "fdusk2", [=] {}},
        {940, "board", [=] { A.tab = Tab::None; E.time = 11; look(3.0f, 1.0f, std::atan2(-(6.0f - 3.0f), -(-5.0f - 1.0f)), -0.05f)(); }},
        {945, "boardui", [=] { A.boardOpen = true; }},
        {935, "nightsky", [=] { A.tab = Tab::None; E.time = 23.0f; E.cutscene.dur = 0; restoreSeason(3, 3, 23.0f); E.time = 23.0f; E.player.yaw = 0; E.player.pitch = 0.45f; }},
        {920, "caravan", [=] { Structure st; st.uid = 9001; st.kind = SK_MARKET_STALL; st.x = 4; st.z = 3; st.y = world::getTerrainHeight(4, 3); st.rot = 0; st.look = app::defaultLookFor(SK_MARKET_STALL); A.structures.push_back(st); village::structuresChanged(); events::restore(0, 0); look(0, 14, 0, -0.08f)(); }},
        {925, "photo", [=] { E.photo = true; E.photoStart = true; }},
        {930, "blizzard", [=] { E.photo = false; putenv((char*)"HW_BLIZZARD=1"); restoreSeason(3, 2, 13.0f); E.time = 13.0f; look(-30, 50, 0.6f, -0.04f)(); }},
        {910, "relic", [=] { restoreSeason(1, 2, 16.5f); E.time = 16.5f; E.cutscene.dur = 0; for (int i = 0; i < 20; i++) { float a, b, c; relics::position(i, a, b, c); } float x, y, z; if (relics::position(0, x, y, z)) { float px = x + 3.0f, pz = z + 1.5f; look(px, pz, std::atan2(-(x - px), -(z - pz)), -0.3f)(); } }},
        {915, "relic2", [=] { float x, y, z; if (relics::position(4, x, y, z)) { float px = x + 3.0f, pz = z + 1.5f; look(px, pz, std::atan2(-(x - px), -(z - pz)), -0.3f)(); } }},
        {900, "fprof", [=] { restoreSeason(1, 2, 11.0f); E.time = 11.0f; E.cutscene.dur = 0; look(40, -60, 2.2f, -0.05f)(); g_profMs.clear(); g_profTri.clear(); g_profDraw.clear(); g_prof = platform::hasArg("--prof"); }},
        {930, "fprof2", [=] { if (g_prof) { for (auto& kv : g_profMs) std::printf("prof %8.1f ms %9lld tri %5lld draws  %s\n", kv.second, g_profTri[kv.first], g_profDraw[kv.first], kv.first.c_str()); } g_prof = false; }},
        {308, "fnight", [=] { restoreSeason(0, 2, 20.6f); E.time = 20.6f; std::vector<float> t; forest::nearTrees(40, -60, 60, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 9, z = tz + 2; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.25f)(); }},
        {320, "fnight2", [=] {}},

        {330, "river", [=] { auto& rv = world::RIVER; size_t i = rv.pts.size() / 2; float x = rv.pts[i][0] + 18, z = rv.pts[i][1]; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.7f, z}; E.player.yaw = PI / 2; E.player.pitch = -0.12f; }},
        {400, "falls", [=] { auto& hd = world::RIVER.head; float x = hd[0] + 10, z = hd[1] + 40; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.7f, z}; E.player.yaw = 0.25f; E.player.pitch = 0.1f; }},
        {460, "inventory", [=] { look(0, 14, 0, -0.08f)(); A.tab = Tab::Inventory; }},
        {490, "build", [=] { A.tab = Tab::Buildings; A.buildSection = 1; }},
        {520, "map", [=] { A.tab = Tab::Map; }},
        {550, "journal", [=] { A.tab = Tab::Quests; }},
        {600, "night", [=] { A.tab = Tab::None; E.time = 22.5f; look(10, 30, PI, -0.02f)(); }},
        // shadow-cache scroll check: walk in, shoot, then force a full rebuild and shoot again (should match)
        {600, "", [=] { A.tab = Tab::None; restoreSeason(1, 2, 12.0f); E.time = 13.0f; look(58.0f, -39.1f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(58.37f, -39.1f + 0.11f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(58.74f, -39.1f + 0.22f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(59.11f, -39.1f + 0.33f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(59.48f, -39.1f + 0.44f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(59.85f, -39.1f + 0.55f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(60.22f, -39.1f + 0.66f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(60.59f, -39.1f + 0.77f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(60.96f, -39.1f + 0.88f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(61.33f, -39.1f + 0.99f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(61.70f, -39.1f + 1.10f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(62.07f, -39.1f + 1.21f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(62.44f, -39.1f + 1.32f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(62.81f, -39.1f + 1.43f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(63.18f, -39.1f + 1.54f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(63.55f, -39.1f + 1.65f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(63.92f, -39.1f + 1.76f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(64.29f, -39.1f + 1.87f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(64.66f, -39.1f + 1.98f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(65.03f, -39.1f + 2.09f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(65.40f, -39.1f + 2.20f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(65.77f, -39.1f + 2.31f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(66.14f, -39.1f + 2.42f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(66.51f, -39.1f + 2.53f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(66.88f, -39.1f + 2.64f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(67.25f, -39.1f + 2.75f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(67.62f, -39.1f + 2.86f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(67.99f, -39.1f + 2.97f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(68.36f, -39.1f + 3.08f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(68.73f, -39.1f + 3.19f, 3.6f, -0.25f)(); }},
        {600, "", [=] { look(69.10f, -39.1f + 3.30f, 3.6f, -0.25f)(); }},
        {600, "shA", [=] {}},
        {600, "", [=] { sky::invalidateStaticShadow(); }},
        {600, "shB", [=] {}},
        {640, "dusk", [=] { A.tab = Tab::None; E.time = 18.5f; look(40, -60, 1.6f, 0.05f)(); }},
        {660, "dawn", [=] { E.time = 5.8f; look(40, -60, 1.6f, 0.05f)(); }},
        {640, "vnight", [=] { A.tab = Tab::None; A.population = 5; restoreSeason(3, 1, 4.7f); E.time = 4.7f; look(4, 18, 0.2f, -0.06f)(); g_profMs.clear(); g_profTri.clear(); g_profDraw.clear(); g_prof = platform::hasArg("--prof"); }},
        {660, "vnight2", [=] { if (g_prof) { for (auto& kv : g_profMs) std::printf("prof %8.1f ms %9lld tri %5lld draws  %s\n", kv.second, g_profTri[kv.first], g_profDraw[kv.first], kv.first.c_str()); } g_prof = false; }},
        {680, "winter", [=] { E.time = 11; restoreSeason(3, 2, 11); look(-30, 50, 0.6f, -0.04f)(); }},
        {760, "mountain", [=] { restoreSeason(2, 0, 9); E.time = 9; look(160, -40, -0.8f, 0.05f)(); }},
        {820, "menu", [=] { A.menuOpen = true; }},
        {840, "deer", [=] { screens::openOptions(false); A.menuOpen = false; A.status = Status::Playing; E.time = 10.0f; float hx, hz; if (wildlife::nearestHerd(SP_DEER, 0, 0, hx, hz)) { float x = hx + 5, z = hz + 4; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.4f, z}; E.player.yaw = std::atan2(-(hx - x), -(hz - z)); E.player.pitch = -0.15f; } }},
        {846, "deer2", [=] {}},
        {1450, "", [=] { screens::openOptions(false); A.menuOpen = false; A.tab = Tab::None; A.status = Status::Playing; E.time = 11.0f; E.player.hp = 1e6f; float hx, hz; if (wildlife::nearestHerd(SP_BEAR, 0, 0, hx, hz)) { float x = hx + 7, z = hz + 5; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.6f, z}; E.player.velY = 0; E.player.yaw = std::atan2(-(hx - x), -(hz - z)); E.player.pitch = -0.12f; } }},
        {1456, "bear", [=] {}},
        {1460, "", [=] { E.player.hp = 1e6f; float hx, hz; if (wildlife::nearestHerd(SP_BOAR, 0, 0, hx, hz)) { float x = hx + 6, z = hz + 4; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.6f, z}; E.player.velY = 0; E.player.yaw = std::atan2(-(hx - x), -(hz - z)); E.player.pitch = -0.12f; } }},
        {1466, "boar", [=] {}},
        {1470, "", [=] { E.player.hp = 1e6f; float hx, hz; if (wildlife::nearestHerd(SP_GORAL, 0, 0, hx, hz)) { float x = hx + 5, z = hz + 3; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.6f, z}; E.player.velY = 0; E.player.yaw = std::atan2(-(hx - x), -(hz - z)); E.player.pitch = -0.15f; } }},
        {1476, "goat", [=] {}},
        {1480, "", [=] { E.player.hp = 1e6f; float hx, hz; if (wildlife::nearestHerd(SP_LEOPARD, 0, 0, hx, hz)) { float x = hx + 6, z = hz + 4; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.6f, z}; E.player.velY = 0; E.player.yaw = std::atan2(-(hx - x), -(hz - z)); E.player.pitch = -0.12f; } }},
        {1486, "leopard", [=] {}},
        {1000, "", [=] { screens::openOptions(false); A.menuOpen = false; A.tab = Tab::None; A.status = Status::Playing; restoreSeason(2, 2, 11); E.time = 11.0f; look(4, 16, 0.3f, -0.05f)(); E.player.hp = 1e6f; E.player.maxHp = 1e6f; extras::debugBandits(4 - std::sin(0.3f) * 30, 16 - std::cos(0.3f) * 30, 6); }},
        {1090, "bandits", [=] { E.player.hp = 1e6f; }},
        {1094, "", [=] { E.player.hp = 1e6f; float bx, bz; extras::banditCamp(bx, bz); look(bx + 3, bz + 3, std::atan2(3.0f, 3.0f), -0.05f)(); }},
        {1100, "bandits2", [=] {}},
        {1110, "", [=] { screens::openOptions(false); A.menuOpen = false; A.tab = Tab::None; restoreSeason(0, 3, 10); E.time = 10.0f; const float* F = grass::flowerFields(); float cx = F[0], cz = F[1], r = F[2]; float L = std::sqrt(cx * cx + cz * cz); float x = cx - cx / L * (r + 14), z = cz - cz / L * (r + 14); look(x, z, std::atan2(-(cx - x), -(cz - z)), -0.12f)(); }},
        {1170, "flowers", [=] {}},
        {1174, "", [=] { const float* F = grass::flowerFields(); float cx = F[0], cz = F[1]; look(cx + 3, cz + 2, std::atan2(-3.0f, -2.0f) + 0.6f, -0.3f)(); }},
        {1200, "flowers2", [=] {}},
        {1210, "", [=] { screens::openOptions(false); A.menuOpen = false; A.tab = Tab::None; restoreSeason(2, 3, 8); E.time = 8.0f; look(-39.2f, 1.7f, 2.9f, -0.35f)(); }},
        {1312, "", [=] { restoreSeason(1, 2, 9); E.time = 10.0f; village::debugGroup(-12.0f, 8.0f, 6); look(-12.0f, 12.5f, 0.0f, -0.12f)(); }},
        {1330, "mh", [=] { village::debugGroup(-12.0f, 8.0f, 6); }},
        {1332, "", [=] { look(-12.0f, 10.3f, 0.0f, -0.06f)(); village::debugGroup(-12.0f, 8.0f, 3); }},
        {1345, "mh2", [=] {}},
        {1347, "", [=] { look(-12.0f, 9.55f, 0.0f, -0.2f)(); }},
        {1360, "mh3", [=] {}},
        {1380, "", [=] { restoreSeason(1, 2, 10); E.time = 10.5f; A.tab = Tab::None; A.menuOpen = false;
            const StructureKind ks[3] = {SK_COTTAGE, SK_FAMILY_HOUSE, SK_RAISED_HOUSE};
            for (int i = 0; i < 3; i++) { Structure st; st.uid = 9000 + i; st.kind = ks[i]; st.x = -9.0f + i * 9.0f; st.z = -9.0f; st.y = world::getTerrainHeight(st.x, st.z); st.rot = 0.0f; st.look.roof = i; st.look.wall = i; st.look.wood = i % 3; st.look.door = i; st.look.accent = i + 1; A.structures.push_back(st); }
            village::structuresChanged(); look(0.0f, 6.5f, 0.0f, 0.02f)(); }},
        {1405, "houses", [=] {}},
        {1410, "", [=] { restoreSeason(2, 4, 19.4f); E.time = 19.4f; A.tab = Tab::None; const Shrine& sh = world::SHRINES[0]; E.player.pos = {sh.x, sh.y + 1.8f, sh.z}; E.player.velY = 0; E.player.yaw = std::atan2(sh.x, sh.z); E.player.pitch = -0.2f; }},
        {1440, "templeview", [=] {}},
        {1490, "", [=] { restoreSeason(3, 1, 2.0f); E.time = 2.0f; A.tab = Tab::None; const Shrine& sh = world::SHRINES[0]; E.player.pos = {sh.x, sh.y + 1.8f, sh.z}; E.player.velY = 0; E.player.yaw = std::atan2(sh.x, sh.z); E.player.pitch = -0.2f; }},
        {1516, "nightview", [=] {}},
        {1362, "", [=] { look(-12.0f, 9.2f, 0.0f, -0.1f)(); }},
        {1375, "mh4", [=] {}},
        {1265, "", [=] { restoreSeason(1, 2, 9); E.time = 10.5f; extras::debugTrophies(); float x = -6.5f + std::sin(1.2f) * 4.6f, z = 3.0f + std::cos(1.2f) * 4.6f; look(x, z, 1.2f, -0.02f)(); }},
        {1290, "trophy", [=] {}},
        {1292, "", [=] { float bx = 6.0f, bz = -5.0f, a = std::atan2(-bx, -bz); float x = bx + std::sin(a) * 3.4f, z = bz + std::cos(a) * 3.4f; look(x, z, a, -0.03f)(); }},
        {1310, "board2", [=] {}},
        {1260, "riverleaf", [=] { float wl = world::waterHeightAt(-39.2f, 1.7f), g = world::getTerrainHeight(-39.2f, 1.7f); RiverHit q = world::riverQuery(-39.2f, 1.7f); std::printf("riverleaf: water %.2f ground %.2f river d %.1f hw %.1f wy %.2f\n", wl, g, q.d, q.hw, q.wy); }},
        {852, "", [=] { E.time = 12.0f; float hx, hz; if (wildlife::nearestHerd(SP_LEOPARD, 0, 0, hx, hz)) { float x = hx + 2.2f, z = hz + 1.6f; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.0f, z}; E.player.velY = 0; E.player.yaw = std::atan2(-(hx - x), -(hz - z)); E.player.pitch = -0.25f; } }},
        {853, "leo", [=] {}},
        {850, "wolf", [=] { float hx, hz; if (wildlife::nearestHerd(SP_WOLF, 0, 0, hx, hz)) { float x = hx + 6, z = hz + 5; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.5f, z}; E.player.yaw = std::atan2(-(hx - x), -(hz - z)); E.player.pitch = -0.15f; E.player.hp = 1e6f; } }},
        {600, "kalyan", [=] { A.tab = Tab::None; float hx = 5, hz = -4; npcs::position("hunter", hx, hz); std::printf("hunter at %.1f %.1f\n", hx, hz); float x = hx, z = hz + 2.4f; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.55f, z}; E.player.yaw = 0; E.player.pitch = -0.1f; }},
        {606, "kalyan2", [=] {}},
        {610, "temple", [=] { E.time = 19.1f; ([=] { A.tab = Tab::None; const Shrine& sh = world::SHRINES[0]; float x = sh.x + std::sin(sh.yaw) * 26, z = sh.z + std::cos(sh.yaw) * 26; E.player.pos = {x, std::max(world::getTerrainHeight(x, z), sh.y) + 3.5f, z}; E.player.velY = 0; E.player.yaw = sh.yaw; E.player.pitch = -0.08f; })(); }},
        {616, "temple2", [=] { restoreSeason(3, 1, 19.8f); E.time = 19.8f; A.tab = Tab::None; E.cutscene.dur = 0; const Shrine& sh = world::SHRINES[0]; float L = std::sqrt(sh.x * sh.x + sh.z * sh.z); float x = sh.x - sh.x / L * 30, z = sh.z - sh.z / L * 30; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.7f, z}; E.player.velY = 0; E.player.yaw = std::atan2(sh.x, sh.z); E.player.pitch = -0.45f; g_profMs.clear(); g_profTri.clear(); g_profDraw.clear(); g_prof = platform::hasArg("--prof"); }},
        {624, "temple3", [=] { if (g_prof) { for (auto& kv : g_profMs) std::printf("prof %8.1f ms %9lld tri %5lld draws  %s\n", kv.second, g_profTri[kv.first], g_profDraw[kv.first], kv.first.c_str()); } g_prof = false; }},
        {862, "", [=] { A.menuOpen = false; A.tab = Tab::None; screens::openOptions(false); restoreSeason(1, 1, 10.5f); E.time = 10.5f; std::vector<float> t; forest::nearTrees(10, 40, 160, t, 12); float tx = 10, tz = 40, ts = 1; for (size_t i = 0; i + 3 < t.size(); i += 4) if (std::fabs(world::getTerrainHeight(t[i], t[i + 2]) - world::getTerrainHeight(t[i] + 12, t[i + 2] + 4)) < 2.5f) { tx = t[i]; tz = t[i + 2]; ts = t[i + 3]; break; } float x = tx + 12, z = tz + 4; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.22f)(); }},
        {876, "ptree", [=] {}},
        {826, "options", [=] { screens::openOptions(true); }},
        {830, "place", [=] { screens::openOptions(false); A.menuOpen = false; look(4, 16, 0.3f, -0.15f)(); for (auto& k : {"wood", "stone", "fiber"}) A.inv[k] = 99; app::startStructure(SK_COTTAGE); A.showStyle = true; }},
        {832, "dialogue", [=] { actions::cancelPlacement(); actions::openDialogue("carpenter"); }},
        {834, "people", [=] { A.dialogue = -1; A.population = 3; A.tab = Tab::Buildings; A.buildSection = 4; }},
        {836, "victory", [=] { A.tab = Tab::None; A.status = Status::Victory; }},
        {960, "end", [=] {}},
    };
    {
      // colour-grade comparison set: each season x morning/evening/night from the same viewpoint
      const char* sn[4] = {"spring", "summer", "autumn", "winter"};
      const char* tn[3] = {"morning", "evening", "night"};
      const float tv[3] = {8.0f, 18.0f, 22.0f};
      std::vector<Shot> g;
      for (int si = 0; si < 4; si++)
        for (int ti = 0; ti < 3; ti++) {
          float tt = tv[ti];
          int ss = si;
          g.push_back({0, std::string("g_") + sn[si] + "_" + tn[ti], [=] { restoreSeason(ss, 3, tt); E.time = tt; look(-6, 30, 0.35f, -0.03f)(); }});
        }
      shots.insert(shots.end() - 1, g.begin(), g.end());
    }
    std::string only = platform::argValue("--shots");
    if (!only.empty()) {
      // keep a setup step only when the shot it prepares was asked for (unrequested setups used to run anyway and
      // added hundreds of software-rendered frames to every test)
      std::vector<Shot> keep, pending;
      int f = 0;
      for (auto& s : shots) {
        if (s.name.empty() && s.frame <= 60) { f = std::max(s.frame, f + 2); keep.push_back({f, s.name, s.setup}); continue; }  // boot: new game etc.
        if (s.name.empty()) { pending.push_back(s); continue; }
        bool want = s.name == "end" || ("," + only + ",").find("," + s.name + ",") != std::string::npos;
        if (!want) { pending.clear(); continue; }
        if (s.name == "end") pending.clear();
        for (auto& p : pending) { f += 2; keep.push_back({f, p.name, p.setup}); }
        pending.clear();
        f = std::max(f + 70, 200);  // the first shot waits for the world to finish loading, as before
        keep.push_back({f, s.name, s.setup});
      }
      shots = keep;
      if (std::getenv("HW_SPIN")) shots.erase(std::remove_if(shots.begin(), shots.end(), [](const Shot& s) { return s.name == "end"; }), shots.end());
    }
  }

  double last = platform::time();
  int frames = 0, statFrames = 0;
  double statT = 0, statMs = 0;
  bool needClick = test ? false : true;
  float slowT = 0, fastT = 0;
  float baseGrass = g_settings.grassRadius, baseUnder = g_settings.undergrowthRadius;
  size_t shotIdx = 0;
  double testFpsSum = 0;
  int testFpsN = 0;
  float peakRam = 0;
  Status prevStatus = A.status;
  FrameStats lastFs;
  GLuint gpuQ[4] = {0, 0, 0, 0};
  double gpuAcc = 0, cpuAcc = 0;
  int gpuN = 0;
  SceneTarget scene;
  g_scene = &scene;
  float renderScale = g_settings.renderScale;

  // shader warm-up: draw the world once from a few spots (and seasons) during loading so every shader variant
  // is compiled now instead of hitching the first time it appears in play
  if (!test && !std::getenv("HW_NOWARM")) {
    loadingFrame("Preparing shaders...", 0.8f);
    Vec3 savePos = E.player.pos;
    float saveYaw = E.player.yaw, savePitch = E.player.pitch;
    int W = std::max(1, platform::width()), H = std::max(1, platform::height());
    scene.ensure(W, H);
    struct Spot { float x, z, yaw; };
    sky::warmPrograms();
    grass::setPreload(true);
    Spot spots[5] = {{savePos.x, savePos.z, saveYaw}, {0, 14, 0}, {40, -60, 2.2f}, {world::SHRINES.empty() ? 0 : world::SHRINES[0].x - 20, world::SHRINES.empty() ? 0 : world::SHRINES[0].z, 1.57f}, {-6, 30, 0.35f}};
    for (auto& sp : spots) {
      E.player.pos = {sp.x, world::getTerrainHeight(sp.x, sp.z) + 1.7f, sp.z};
      E.player.yaw = sp.yaw;
      E.player.pitch = -0.05f;
      player::updateCamera((float)W / H);
      sky::update(0.016f);
      render::uploadFrame();
      forest::update(render::cam.pos, render::cam.forward());
      glBindFramebuffer(GL_FRAMEBUFFER, scene.drawFbo());
      glViewport(0, 0, scene.w, scene.h);
      drawWorld(0.016f, false);
      glFinish();
      pumpLoading();
      // second pass at night: compiles lamp/glow/firefly paths too
      float t0 = E.time;
      E.time = 22.0f; sky::update(0.016f); render::uploadFrame();
      drawWorld(0.016f, false);
      glFinish();
      E.time = t0;
    }
    grass::setPreload(false);
    sky::update(0.016f);
    // let the forest worker finish building the cells around the start position before play begins
    for (int i = 0; i < 40; i++) { player::updateCamera((float)W / H); forest::update(savePos, render::cam.forward()); pumpLoading(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
    loadingFrame("Ready", 1.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    E.player.pos = savePos; E.player.yaw = saveYaw; E.player.pitch = savePitch;
  }

  platform::loadPump = nullptr;  // loading over: no more mid-step redraws
  while (!platform::shouldClose()) {
    platform::pollEvents();
    double t = platform::time();
    float dt = (float)std::min(0.1, t - last);
    last = t;
    if (test) dt = 1.0f / 60;
    Input& in = platform::input();

    if (in.focusLost) needClick = true;
    if (in.pressed[K_F11]) platform::toggleFullscreen();
    if (in.pressed[K_F3]) screens::toggleStats();
    if (in.pressed[K_L] && E.playing && !E.uiOpen && !E.photo) { E.lanternOn = !E.lanternOn; audio::crispClick(); }
    bool takeShot = in.pressed[K_F2];
    if (in.pressed[K_ESC]) { if (E.photo) E.photo = false; else screens::escape(); }
    else {
      for (int k = 0; k < K_COUNT; k++) {
        if (!in.pressed[k] || k == K_ESC || k == K_F2 || k == K_F3 || k == K_F4 || k == K_F11) continue;
        if (k == K_R && E.playing && !E.uiOpen && E.world.placing.empty()) { app::craftArrows(false); continue; }
        player::onKeyPressed(k);
      }
    }
    E.uiOpen = app::uiOpen();
    E.playing = A.status == Status::Playing;
    if (A.status != prevStatus) { if (E.playing) needClick = false; prevStatus = A.status; }
    bool wantCapture = E.playing && !E.uiOpen && !needClick && platform::focused() && !test;
    if (wantCapture != in.captured) platform::setCapture(wantCapture);
    if (in.captured) {
      if (in.mousePressed[0]) player::onMouseDown(0);
      if (in.mousePressed[1]) player::onMouseDown(1);
    }
    if (in.mouseReleased[0]) player::onMouseUp(0);
    if (in.mouseReleased[1]) player::onMouseUp(1);

    g_pp.on = screens::statsVisible() || (g_settings.adaptiveDetail && E.playing);  // timings also drive per-system auto detail
    if (g_pp.on) g_pp.begin();
    forest::profOpen = g_pp.on ? +[](const char* n) { return g_pp.open(n); } : nullptr;
    forest::profClose = g_pp.on ? +[](int i) { g_pp.close(i); } : nullptr;
    // pause menu freezes the world: game time, animals, villagers, wind
    float simDt = ((A.menuOpen || E.photo) && E.playing && !test) ? 0.0f : dt;
    PROF(player::update(simDt));
    PROF(app::update(simDt));
    updateWind(simDt);
    updateSeason(E.time);
    audio::setMusicPlaying(A.status == Status::Playing && !A.menuOpen);
    audio::setMusicMuted(A.musicMuted);
    ambience::update(dt);

    if (screens::takeSettingsChanged()) {
      renderScale = g_settings.renderScale;
      baseGrass = g_settings.grassRadius;
      forest::markDirty();
    }
    int W = platform::width(), H = platform::height();
    if (W <= 0 || H <= 0) { platform::clearFrameInput(); continue; }
    glViewport(0, 0, W, H);
    player::updateCamera((float)W / H);
    sky::update(simDt);
    render::uploadFrame();
    PROF(forest::update(render::cam.pos, render::cam.forward()));

    if (gpuQ[0] == 0) glGenQueries(4, gpuQ);
    glBeginQuery(GL_TIME_ELAPSED, gpuQ[frames & 3]);
    {
      static double bsig = 0;
      double sg = 0;
      for (auto& b : A.buildings) if (b.built) sg += 1 + b.pos[0] * 0.37 + b.pos[2] * 0.91 + b.rotY;
      if (sg != bsig) { bsig = sg; sky::invalidateStaticShadow(); }
    }
    bool cut = E.cutscene.dur > 0;
    // Full-screen menus: render the world once, then reuse that frame until the menu closes.
    static bool frozenValid = false;
    static int frozenW = 0, frozenH = 0;
    static float frozenRs = 1;
    bool freeze = E.playing && !test && !cut && (A.menuOpen || A.howToOpen || A.tab == Tab::Map || A.tab == Tab::Inventory || A.tab == Tab::Quests);
    // other menus / dialogue: world at half rate; window in the background: world not redrawn at all
    bool unfocused = !test && !platform::focused();
    bool halfMenu = E.playing && !test && !cut && !freeze && E.world.placing.empty() && (A.tab != Tab::None || A.dialogue >= 0);
    bool hold = freeze || unfocused || halfMenu;
    if (!hold) frozenValid = false;
    bool skipWorld = hold && frozenValid && frozenW == W && frozenH == H && (freeze || unfocused || (render::frameNo & 1));
    if (!skipWorld && (sky::shadowNeedsUpdate() || sky::shadowStaticDirty())) PROF(drawShadows());
    float rs = clampf(g_settings.adaptiveQuality ? renderScale : g_settings.renderScale, 0.4f, 1.0f);
    bool taa = g_settings.temporalUpscale && (!test || std::getenv("HW_TAA"));
    if (taa) rs = std::max(0.5f, rs * 0.9f);  // temporal upscaling rebuilds the rest of the detail over a few frames
    if (const char* e = std::getenv("HW_RS")) rs = (float)std::atof(e);
    if (skipWorld) rs = frozenRs;
    bool sunOn = !cut && sunfx::wanted();
    bool scaled = rs < 0.99f || g_settings.fxaa || sunOn || hold || taa;  // full res and no effects: draw straight to the screen, no copy
    if (scaled) {
      scene.ensure(std::max(1, (int)(W * rs)), std::max(1, (int)(H * rs)));
      glBindFramebuffer(GL_FRAMEBUFFER, scene.drawFbo());
      glViewport(0, 0, scene.w, scene.h);
    }
    g_sunDepth = scaled && sunOn ? scene.depth : 0;
    g_sunW = scene.w;
    g_sunH = scene.h;
    Mat4 vpPlain = render::cam.viewProj, projPlain = render::cam.proj;
    {
      // motion-vector inputs for leaf shaders: last frame's un-jittered camera and wind clock
      static Mat4 lastVP; static float lastWind = 0; static bool haveLast = false;
      std::memcpy(render::frame.vpNoJit, vpPlain.m, 64);
      std::memcpy(render::frame.prevVP, (haveLast ? lastVP : vpPlain).m, 64);
      render::frame.taa[0] = haveLast ? lastWind : render::frame.season2[2];
      if (!skipWorld) { lastVP = vpPlain; lastWind = render::frame.season2[2]; haveLast = true; }
    }
    bool velOn = taa && scaled && !scene.samples && scene.vel;
    if (scaled && scene.fbo && !scene.samples) {
      glBindFramebuffer(GL_FRAMEBUFFER, scene.fbo);
      const GLenum bufs[4] = {GL_COLOR_ATTACHMENT0, GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT1};
      glDrawBuffers(velOn ? 4 : 1, bufs);
      if (velOn && !skipWorld) { const float z[4] = {0, 0, 0, 0}; glClearBufferfv(GL_COLOR, 3, z); }
    }
    render::velocityPass = velOn;
    if (!taa || skipWorld) render::uploadFrame();
    if (taa && !skipWorld) {
      // sub-pixel jitter (Halton 2,3 over 8 frames) baked into the projection for this frame's world draw only
      int k = (int)(render::frameNo % 8) + 1;
      g_taa.jx = halton(k, 2) - 0.5f; g_taa.jy = halton(k, 3) - 0.5f;
      render::cam.proj.m[8] += g_taa.jx * 2.0f / scene.w;
      render::cam.proj.m[9] += g_taa.jy * 2.0f / scene.h;
      render::cam.update();
      std::memcpy(render::frame.proj, render::cam.proj.m, 64);  // every shader reads the projection from the frame block
      std::memcpy(render::frame.viewProj, render::cam.viewProj.m, 64);
      render::uploadFrame();
    }
    if (!skipWorld) drawWorld(dt, E.playing && !E.uiOpen && !cut && !E.photo);
    render::velocityPass = false;
    if (taa && !skipWorld) {
      render::cam.proj = projPlain; render::cam.update();
      std::memcpy(render::frame.proj, render::cam.proj.m, 64);
      std::memcpy(render::frame.viewProj, render::cam.viewProj.m, 64);
      render::uploadFrame();
    }
    if (hold && !skipWorld) { frozenValid = true; frozenW = W; frozenH = H; frozenRs = rs; }
    if (scaled) {
      if (!skipWorld) scene.resolve(GL_COLOR_BUFFER_BIT);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glViewport(0, 0, W, H);
      static const char* UPVS = "out vec2 v_uv;\nvoid main(){ vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); v_uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }\n";
      static const char* UPFS = R"(in vec2 v_uv;
uniform sampler2D u_src;
uniform vec2 u_px;
uniform int u_fxaa;
uniform float u_sharp;
uniform float u_vig;
out vec4 o;
float vig(){ vec2 d = (v_uv - 0.5) * vec2(1.6, 1.0); return 1.0 - smoothstep(0.35, 1.05, length(d)) * 0.38 * u_vig; }
float lum(vec3 c){ return dot(c, vec3(0.299, 0.587, 0.114)); }
vec3 sharpen(vec3 c){
  // contrast-adaptive sharpening: restores crispness lost to the lower render resolution
  if (u_sharp <= 0.0) return c;
  vec3 a = texture(u_src, v_uv + vec2(0.0, -u_px.y)).rgb, b = texture(u_src, v_uv + vec2(-u_px.x, 0.0)).rgb;
  vec3 d = texture(u_src, v_uv + vec2(u_px.x, 0.0)).rgb, e = texture(u_src, v_uv + vec2(0.0, u_px.y)).rgb;
  vec3 mn = min(c, min(min(a, b), min(d, e))), mx = max(c, max(max(a, b), max(d, e)));
  vec3 amp = sqrt(clamp(min(mn, 1.0 - mx) / max(mx, 1e-4), 0.0, 1.0));
  vec3 w = -amp * mix(0.125, 0.2, u_sharp);
  return clamp((c + (a + b + d + e) * w) / (1.0 + 4.0 * w), 0.0, 1.0);
}
void main(){
  vec3 m = texture(u_src, v_uv).rgb;
  if (u_fxaa == 0) { o = vec4(sharpen(m) * vig(), 1.0); return; }
  vec3 nw = texture(u_src, v_uv + vec2(-1.0, -1.0) * u_px).rgb, ne = texture(u_src, v_uv + vec2(1.0, -1.0) * u_px).rgb;
  vec3 sw = texture(u_src, v_uv + vec2(-1.0, 1.0) * u_px).rgb, se = texture(u_src, v_uv + vec2(1.0, 1.0) * u_px).rgb;
  float lNW = lum(nw), lNE = lum(ne), lSW = lum(sw), lSE = lum(se), lM = lum(m);
  float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE))), lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
  vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), (lNW + lSW) - (lNE + lSE));
  float red = max((lNW + lNE + lSW + lSE) * 0.03125, 1.0 / 128.0);
  float rcp = 1.0 / (min(abs(dir.x), abs(dir.y)) + red);
  dir = clamp(dir * rcp, vec2(-8.0), vec2(8.0)) * u_px;
  vec3 a = 0.5 * (texture(u_src, v_uv + dir * (1.0 / 3.0 - 0.5)).rgb + texture(u_src, v_uv + dir * (2.0 / 3.0 - 0.5)).rgb);
  vec3 b = a * 0.5 + 0.25 * (texture(u_src, v_uv - dir * 0.5).rgb + texture(u_src, v_uv + dir * 0.5).rgb);
  float lB = lum(b);
  vec3 f = (lB < lMin || lB > lMax) ? a : b;
  o = vec4((f + (sharpen(m) - m) * u_sharp * 0.6) * vig(), 1.0);
}
)";
      GLuint presentTex = scene.color;
      int presW = scene.w, presH = scene.h;
      if (taa && render::program("taa", UPVS, TAA_FS).prog) {  // resolve shader failed to build: show the plain frame
        g_taa.ensure(W, H);
        if (!skipWorld) {
          if ((render::cam.pos - g_taa.prevPos).length() > 4.0f) g_taa.valid = false;  // teleport / cutscene cut
          int dst = g_taa.cur ^ 1;
          glBindFramebuffer(GL_FRAMEBUFFER, g_taa.fbo[dst]);
          glViewport(0, 0, W, H);
          glDisable(GL_DEPTH_TEST);
          glDisable(GL_BLEND);
          Shader& t = render::program("taa", UPVS, TAA_FS);
          // own texture units: 1 and 2 hold the terrain height / colour maps that grass and terrain rely on
          glActiveTexture(GL_TEXTURE27); glBindTexture(GL_TEXTURE_2D, scene.color);
          glActiveTexture(GL_TEXTURE28); glBindTexture(GL_TEXTURE_2D, scene.depth);
          glActiveTexture(GL_TEXTURE29); glBindTexture(GL_TEXTURE_2D, g_taa.tex[g_taa.cur]);
          glActiveTexture(GL_TEXTURE30); glBindTexture(GL_TEXTURE_2D, scene.vel);
          glActiveTexture(GL_TEXTURE0);
          t.set1i("u_cur", 27); t.set1i("u_depth", 28); t.set1i("u_hist", 29); t.set1i("u_vel", 30);
          t.set2f("u_opx", 1.0f / W, 1.0f / H);
          Mat4 inv = vpPlain.inverse();
          glUniformMatrix4fv(t.loc("u_invVP"), 1, GL_FALSE, inv.m);
          glUniformMatrix4fv(t.loc("u_taaPrevVP"), 1, GL_FALSE, (g_taa.valid ? g_taa.prevVP : vpPlain).m);
          t.set2f("u_jit", g_taa.jx / scene.w, g_taa.jy / scene.h);
          t.set2f("u_ipx", 1.0f / scene.w, 1.0f / scene.h);
          t.set2f("u_nf", render::cam.nearZ, render::cam.farZ);
          t.set1f("u_valid", g_taa.valid && !std::getenv("HW_TAANOHIST") ? 1.0f : 0.0f);
          PROF(render::drawFullscreen());
          g_taa.cur = dst;
          g_taa.valid = true;
          g_taa.prevVP = vpPlain;
          g_taa.prevPos = render::cam.pos;
          glBindFramebuffer(GL_FRAMEBUFFER, 0);
          glViewport(0, 0, W, H);
        }
        presentTex = g_taa.tex[g_taa.cur];
        presW = W; presH = H;
      } else g_taa.valid = false;
      Shader& up = render::program("upscale", UPVS, UPFS);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_BLEND);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, presentTex);
      up.set1i("u_src", 0);
      up.set2f("u_px", 1.0f / presW, 1.0f / presH);
      up.set1i("u_fxaa", g_settings.fxaa && !taa ? 1 : 0);  // TAA already anti-aliases
      up.set1f("u_vig", E.playing ? 1.0f : 0.0f);  // vignette folded into this pass: no extra full-screen draw
      up.set1f("u_sharp", taa ? 0.55f : clampf((1.0f - rs) / 0.28f, 0.0f, 1.0f) * 0.4f);  // light sharpening offsets the history blur
      PROF(render::drawFullscreen());
      glEnable(GL_DEPTH_TEST);
      if (sunOn) PROF(sunfx::composite(W, H));
    }

    if (E.playing && !scaled) {  // only when there was no final pass to fold it into
      static const char* VG_VS = "out vec2 v_uv;\nvoid main(){ vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); v_uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }\n";
      static const char* VG_FS = "in vec2 v_uv;\nout vec4 o;\nvoid main(){ vec2 d = (v_uv - 0.5) * vec2(1.6, 1.0); float v = smoothstep(0.35, 1.05, length(d)); o = vec4(vec3(1.0 - v * 0.38), 1.0); }\n";
      render::program("vignette", VG_VS, VG_FS);
      glDisable(GL_DEPTH_TEST);
      glEnable(GL_BLEND);
      glBlendFunc(GL_DST_COLOR, GL_ZERO);
      render::drawFullscreen();
      glDisable(GL_BLEND);
      glEnable(GL_DEPTH_TEST);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    float uiScale = std::max(1.0f, H / 820.0f) * g_settings.uiScale;
    ui::begin(W, H, uiScale);
    float pr = platform::pixelRatio();
    ui::MouseState ms{};
    bool free = !in.captured;
    ms.x = free ? in.mx * pr : -1e5f;
    ms.y = free ? in.my * pr : -1e5f;
    ms.down = free && in.mouseDown[0];
    ms.pressed = free && in.mousePressed[0];
    ms.released = free && in.mouseReleased[0];
    ms.wheel = free ? in.wheel : 0;
    ui::setMouse(ms);
    PROF(screens::draw(dt, free));
    bool uiTookClick = ui::clickConsumed();
    ui::end();
    if (needClick && in.mousePressed[0] && !uiTookClick && E.playing && !app::uiOpen()) needClick = false;

    if (takeShot) screenshot(timestampName());
    // HW_SPIN="x,z,yaw0,rate,pitch": from frame 260 stand at x,z and turn (rate rad/frame) for 60 frames, saving each
    // frame, to catch anything that appears only while the camera turns
    if (test) {
      static const char* spin = std::getenv("HW_SPIN");
      if (spin) {
        float sx = 0, sz = 0, y0 = 0, rate = 0, pit = 0;
        std::sscanf(spin, "%f,%f,%f,%f,%f", &sx, &sz, &y0, &rate, &pit);
        if (frames >= 200 && frames < 380) {
          int k = std::max(0, frames - 300);
          E.player.pos = {sx, world::getTerrainHeight(sx, sz) + 1.7f, sz};
          E.player.yaw = y0 + rate * k;
          E.player.pitch = pit;
          if (frames >= 300) screenshot(selftest + "/spin_" + std::to_string(frames - 300) + ".png");
        }
        if (frames == 380) platform::requestClose();
      }
    }
    if (test && shotIdx < shots.size() && frames == shots[shotIdx].frame) {
      if (!shots[shotIdx].name.empty() && shots[shotIdx].name != "end") {
        int sf = statFrames + 1;
        std::printf("STAT %s tris=%lld draws=%d inst=%d cull=%.0f haze=%.2f\n", shots[shotIdx].name.c_str(), gfx::triangles / sf, gfx::drawCalls / sf, forest::instanceCount(), envmap::g_cullQ, envmap::hazeAt(render::cam.pos.x, render::cam.pos.z));
        std::fflush(stdout);
      }
      if (!shots[shotIdx].name.empty() && shots[shotIdx].name != "end") screenshot(selftest + "/" + shots[shotIdx].name + ".png");
      if (shots[shotIdx].name == "end") {
        std::printf("selftest: %d draws/frame, %dk tris/frame\n", lastFs.drawCalls, lastFs.tris / 1000);
        std::printf("selftest: avg %.1f fps (render-bound frames), peak RAM %.0f MB, GPU %.0f MB, instances %d\n", testFpsN ? testFpsSum / testFpsN : 0, peakRam, (gfx::textureBytes() + gfx::bufferBytes()) / 1048576.0, forest::instanceCount());
        platform::requestClose();
      }
      shotIdx++;
    }
    if (test) {
      for (auto& s : shots)
        if (s.frame == frames + 1) s.setup();
    }

    glEndQuery(GL_TIME_ELAPSED);
    if (frames >= 3) {
      GLuint64 ns = 0;
      GLint ready = 0;
      GLuint q = gpuQ[(frames + 1) & 3];
      glGetQueryObjectiv(q, GL_QUERY_RESULT_AVAILABLE, &ready);
      if (ready) { glGetQueryObjectui64v(q, GL_QUERY_RESULT, &ns); gpuAcc += ns / 1e6; gpuN++; }
    }
    cpuAcc += (platform::time() - t) * 1000;
    if (g_pp.on) g_pp.collect();
    platform::swap();
    {
      // frame pacing: never let the CPU queue more than 2 frames ahead of the GPU. Keeps input latency low and frame
      // times even; the wait only happens when the GPU is the bottleneck, and the CPU sleeps instead of spinning
      static GLsync fences[2] = {nullptr, nullptr};
      static int fi = 0;
      if (fences[fi]) {
        GLenum r = glClientWaitSync(fences[fi], GL_SYNC_FLUSH_COMMANDS_BIT, 0);
        double waitStart = platform::time();
        while (r == GL_TIMEOUT_EXPIRED && platform::time() - waitStart < 0.05) {
          std::this_thread::sleep_for(std::chrono::microseconds(300));
          r = glClientWaitSync(fences[fi], 0, 0);
        }
        glDeleteSync(fences[fi]);
      }
      fences[fi] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
      fi ^= 1;
    }
    if (!test) {
      bool menu = A.status != Status::Playing || A.menuOpen || A.tab != Tab::None || A.howToOpen;
      if (platform::minimized()) std::this_thread::sleep_for(std::chrono::milliseconds(100));  // minimised: nothing to show, idle
      int cap = !platform::focused() ? 10 : menu ? g_settings.menuFps : g_settings.fpsLimit;
      static int hz = platform::refreshRate();
      if (g_settings.vsync && cap >= hz - 1) cap = 0;
      if (cap > 0) {
        static double nextT = 0;
        double frame = 1.0 / cap, nowT = platform::time();
        if (nextT < nowT - frame) nextT = nowT;
        nextT += frame;
        double wait = nextT - platform::time();
        // hybrid wait: sleep in 1 ms slices while far from the deadline, then a short yield-spin for precision
        while (nextT - platform::time() > 0.0018) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        while (platform::time() < nextT) std::this_thread::yield();
      }
    }
    frames++;
    render::frameNo++;
    double ft = platform::time() - t;
    statMs += ft;
    statFrames++;
    statT += dt;
    if (test && frames > 40) { testFpsSum += 1.0 / std::max(1e-4, ft); testFpsN++; }
    if (statT >= 0.5) {
      FrameStats fs;
      fs.fps = statFrames / (float)statT;
      fs.ms = (float)(statMs / statFrames * 1000);
      fs.gpuMs = gpuN ? (float)(gpuAcc / gpuN) : 0;
      fs.cpuMs = (float)(cpuAcc / statFrames);
      fs.renderScale = g_settings.adaptiveQuality ? renderScale : g_settings.renderScale;
      gpuAcc = cpuAcc = 0;
      gpuN = 0;
      fs.drawCalls = gfx::drawCalls / std::max(1, statFrames);
      fs.tris = (int)(gfx::triangles / std::max(1, statFrames));
      fs.ramMB = rssMB();
      peakRam = std::max(peakRam, fs.ramMB);
      { bool exact; float v = vramMB(exact); fs.gpuMB = v >= 0 ? v : (gfx::textureBytes() + gfx::bufferBytes()) / 1048576.0f; fs.vramExact = exact ? 1 : v >= 0 ? 2 : 0; }
      fs.instances = forest::instanceCount();
      screens::setStats(fs);
      if (g_pp.on && g_pp.frames && g_settings.adaptiveDetail && E.playing && !test) {
        // per-system auto detail: when over budget, trim whichever system is heaviest right now (trees or grass);
        // when well under budget, give detail back to whichever was trimmed most
        auto ms = [&](const char* k) { auto it = g_pp.gAcc.find(k); return it == g_pp.gAcc.end() ? 0.0 : it->second / g_pp.frames; };
        double tr = ms("forest::drawOpaque()"), gr = ms("grass::draw()");
        float budget = 1000.0f / std::max(30.0f, g_settings.fpsLimit > 0 ? std::min((float)g_settings.fpsLimit, (float)platform::refreshRate()) : (float)platform::refreshRate());  // a 30/40 fps cap buys detail back
        if (g_settings.eco) budget *= 0.5f;  // Eco: keep the GPU idle at least half the time
        // target fps: the player's cap (or the display); missing it for any reason (GPU, driver, laptop
        // copy-to-display) now also trims detail, not only a busy GPU timer
        float target = g_settings.fpsLimit > 0 ? std::min((float)g_settings.fpsLimit, (float)platform::refreshRate()) : (float)platform::refreshRate();
        static float over = 0, under = 0;
        // trim detail only when the GPU is what holds the frame back: a hot laptop throttles its CPU (every CPU timer
        // goes up together) and cutting trees then only made the picture worse without getting the fps back
        bool gpuLimits = fs.gpuMs > budget * 0.85f || (fs.fps < target * 0.9f && fs.gpuMs >= fs.cpuMs);
        if (gpuLimits) { over += (float)statT; under = 0; }
        else if (fs.gpuMs < budget * 0.6f && fs.fps > target * 0.97f) { under += (float)statT; over = 0; }
        else over = under = 0;
        if (over >= 1.0f) {
          over = 0;
          if (gr > tr * 0.6 && g_settings.grassAuto > 0.6f) g_settings.grassAuto = std::max(0.6f, g_settings.grassAuto - 0.1f);
          else if (g_settings.detailScale > 0.45f) { g_settings.detailScale = std::max(0.45f, g_settings.detailScale - 0.1f); forest::markDirty(); }
        } else if (under >= 4.0f) {
          under = 0;
          if (g_settings.grassAuto < 1.0f && g_settings.grassAuto <= g_settings.detailScale) g_settings.grassAuto = std::min(1.0f, g_settings.grassAuto + 0.05f);
          else if (g_settings.detailScale < 1.0f) { g_settings.detailScale = std::min(1.0f, g_settings.detailScale + 0.05f); forest::markDirty(); }
        }
      }
      if (g_pp.on) g_pp.publish();
      lastFs = fs;
      if (g_settings.adaptiveDetail && !test && E.playing && fs.gpuMs > 0 && !g_pp.on) {
        static float dSlow = 0, dFast = 0;
        float budget = 1000.0f / (g_settings.fpsLimit > 0 ? std::min(g_settings.fpsLimit, platform::refreshRate()) : platform::refreshRate());
        if (g_settings.eco) budget *= 0.5f;
        float target = g_settings.fpsLimit > 0 ? std::min((float)g_settings.fpsLimit, (float)platform::refreshRate()) : (float)platform::refreshRate();
        if (fs.gpuMs > budget * 0.85f || (fs.fps < target * 0.9f && fs.gpuMs >= fs.cpuMs)) dSlow += (float)statT, dFast = 0;  // GPU busy (not a throttled CPU)
        else if (fs.gpuMs < budget * 0.6f && fs.fps > target * 0.97f) dFast += (float)statT, dSlow = 0;
        else dSlow = dFast = 0;
        float& ds = g_settings.detailScale;
        if (dSlow >= 3.0f && ds > 0.6f) { ds = std::max(0.6f, ds - 0.1f); dSlow = 0; forest::markDirty(); }  // slower, shallower: no detail churn
        else if (dFast >= 4.0f && ds < 1.0f) { ds = std::min(1.0f, ds + 0.05f); dFast = 0; forest::markDirty(); }
      }
      if (g_settings.adaptiveQuality && !test && E.playing) {
        float budget = 1000.0f / (g_settings.fpsLimit > 0 ? g_settings.fpsLimit : 60);
        if (g_settings.eco) budget *= 0.5f;
        // only the GPU's own time decides: a low fps from vsync misses, driver stalls or the CPU no longer
        // makes the picture blurrier (it used to drop render scale to ~68% on frames the GPU finished in 8 ms)
        if (fs.gpuMs > budget * 0.85f) slowT += (float)statT, fastT = 0;
        else if (fs.gpuMs < budget * 0.6f) fastT += (float)statT, slowT = 0;
        else slowT = fastT = 0;
        bool gpuBound = fs.gpuMs > fs.cpuMs * 1.2f;
        float floorS = std::min(0.8f, g_settings.renderScale);  // auto never goes below 80% (or the player's own lower setting)
        if (renderScale < floorS) renderScale = floorS;
        if (slowT > 3 && gpuBound && renderScale > floorS) {
          renderScale = std::max(floorS, renderScale - 0.05f);
          slowT = 0;
        } else if (slowT > 2 && g_settings.undergrowthRadius > baseUnder * 0.6f) {
          // the grass radius itself is never touched any more: shrinking it pulled the dense near grass in to ~8 m
          // (and was saved to config.ini, so it stayed small next session). Only undergrowth and the far grass trim.
          g_settings.undergrowthRadius *= 0.88f;
          g_settings.grassAuto = std::max(0.6f, g_settings.grassAuto - 0.1f);
          forest::markDirty();
          slowT = 0;
        } else if (fastT > 3 && renderScale < g_settings.renderScale) {
          renderScale = std::min(g_settings.renderScale, renderScale + 0.1f);
          fastT = 0;
        } else if (fastT > 8 && g_settings.undergrowthRadius < baseUnder) {
          g_settings.undergrowthRadius = std::min(baseUnder, g_settings.undergrowthRadius * 1.08f);
          forest::markDirty();
          fastT = 0;
        }
      }
      statT = 0;
      statMs = 0;
      statFrames = 0;
      gfx::drawCalls = 0;
      gfx::triangles = 0;
    }
    platform::clearFrameInput();
  }

  app::finishSaves();
  app::saveNow();
  audio::shutdown();
  scene.release();
  screens::shutdown();
  forest::shutdown();
  grass::shutdown();
  water::shutdown();
  sky::shutdown();
  terrain_render::shutdown();
  fx::shutdown();
  props::shutdown();
  resources::shutdown();
  assets::shutdown();
  ui::shutdown();
  fonts::shutdown();
  render::shutdown();
  platform::shutdown();
  return 0;
}

void hwWritePng(const char* path, int w, int h, const uint8_t* rgb) { stbi_write_png(path, w, h, 3, rgb, w * 3); }
