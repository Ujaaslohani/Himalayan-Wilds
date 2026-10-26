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

namespace {

struct SceneTarget {
  GLuint fbo = 0, color = 0, depth = 0, msFbo = 0, msColor = 0, msDepth = 0;
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

void loadingFrame(const char* msg) {
  int W = platform::width(), H = platform::height();
  glViewport(0, 0, W, H);
  glClearColor(0.05f, 0.04f, 0.035f, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  float s = std::max(1.0f, H / 820.0f) * g_settings.uiScale;
  ui::begin(W, H, s);
  float w = ui::width(), h = ui::height();
  ui::text("HIMALAYAN WILDS", w / 2, h / 2 - 20, 40, ui::rgba("#fbbf24"), ui::SERIF, 1);
  ui::text(msg, w / 2, h / 2 + 30, 14, ui::rgba("#a8a29e"), ui::SANS, 1);
  ui::end();
  platform::swap();
  platform::pollEvents();
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
  }
  render::beginShadowPass(sky::shadowMatrix(), g_settings.shadowSize, 2);
  PROF(village::drawShadow());
  PROF(npcs::drawShadow());
  PROF(wildlife::drawShadow());
  render::endShadowPass();
}

GLuint g_sunDepth = 0;
SceneTarget* g_scene = nullptr;
int g_sunW = 0, g_sunH = 0;
void drawWorld(float dt, bool viewModel) {
  fx::beginFrame();
  const Color hz = sky::horizon();
  glClearColor(Color::linearToSrgb(hz.r), Color::linearToSrgb(hz.g), Color::linearToSrgb(hz.b), 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  PROF(render::bindShadow());
  PROF(grass::draw());
  PROF(forest::drawOpaque());
  PROF(props::draw());
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
  if (viewModel) archery::drawViewModel(dt, player::cameraWorld());
}

struct Shot { int frame; std::string name; std::function<void()> setup; };

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  timeBeginPeriod(1);
#endif
  if (!platform::init(argc, argv)) return 1;
  gfx::init();
  render::init();
  fonts::init();
  ui::init();
  screens::init();
  audio::init();
  if (A.sfxMuted) audio::toggleSfxMute();

  auto mem = [](const char* tag) { if (platform::hasArg("--memlog")) std::printf("[mem] %-12s %.0f MB  t=%.2fs\n", tag, rssMB(), platform::time()); };
  mem("gl+ui");
  loadingFrame("Shaping the valley...");
  world::generate();
  mem("terrain");
  loadingFrame("Loading forests...");
  std::vector<AssetRequest> reqs = forest::requiredFiles();
  for (auto& f : resources::requiredFiles()) {
    bool have = false;
    for (auto& r : reqs) if (r.file == f) have = true;
    if (!have) reqs.push_back({f, f.find("Rock") != std::string::npos ? "rock" : "under", -1});
  }
  assets::loadAll(reqs);
  for (auto& m : assets::missing()) std::fprintf(stderr, "missing asset: %s (place it in assets/models)\n", m.c_str());

  mem("assets");
  loadingFrame("Raising the village...");
  terrain_render::init();
  water::init();
  sky::init();
  forest::init();
  grass::init();
  fx::init();
  props::init();
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
        {308, "fnight", [=] { restoreSeason(0, 2, 20.6f); E.time = 20.6f; std::vector<float> t; forest::nearTrees(40, -60, 60, t, 1); float tx = t.size() ? t[0] : 40, tz = t.size() ? t[2] : -60; float x = tx + 9, z = tz + 2; look(x, z, std::atan2(-(tx - x), -(tz - z)), 0.25f)(); }},
        {320, "fnight2", [=] {}},

        {330, "river", [=] { auto& rv = world::RIVER; size_t i = rv.pts.size() / 2; float x = rv.pts[i][0] + 18, z = rv.pts[i][1]; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.7f, z}; E.player.yaw = PI / 2; E.player.pitch = -0.12f; }},
        {400, "falls", [=] { auto& hd = world::RIVER.head; float x = hd[0] + 10, z = hd[1] + 40; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.7f, z}; E.player.yaw = 0.25f; E.player.pitch = 0.1f; }},
        {460, "inventory", [=] { look(0, 14, 0, -0.08f)(); A.tab = Tab::Inventory; }},
        {490, "build", [=] { A.tab = Tab::Buildings; A.buildSection = 1; }},
        {520, "map", [=] { A.tab = Tab::Map; }},
        {550, "journal", [=] { A.tab = Tab::Quests; }},
        {600, "night", [=] { A.tab = Tab::None; E.time = 22.5f; look(10, 30, PI, -0.02f)(); }},
        {640, "dusk", [=] { A.tab = Tab::None; E.time = 18.5f; look(40, -60, 1.6f, 0.05f)(); }},
        {660, "dawn", [=] { E.time = 5.8f; look(40, -60, 1.6f, 0.05f)(); }},
        {640, "vnight", [=] { A.tab = Tab::None; A.population = 5; restoreSeason(3, 1, 4.7f); E.time = 4.7f; look(4, 18, 0.2f, -0.06f)(); g_profMs.clear(); g_profTri.clear(); g_profDraw.clear(); g_prof = platform::hasArg("--prof"); }},
        {660, "vnight2", [=] { if (g_prof) { for (auto& kv : g_profMs) std::printf("prof %8.1f ms %9lld tri %5lld draws  %s\n", kv.second, g_profTri[kv.first], g_profDraw[kv.first], kv.first.c_str()); } g_prof = false; }},
        {680, "winter", [=] { E.time = 11; restoreSeason(3, 2, 11); look(-30, 50, 0.6f, -0.04f)(); }},
        {760, "mountain", [=] { restoreSeason(2, 0, 9); E.time = 9; look(160, -40, -0.8f, 0.05f)(); }},
        {820, "menu", [=] { A.menuOpen = true; }},
        {840, "deer", [=] { screens::openOptions(false); A.menuOpen = false; A.status = Status::Playing; float hx, hz; if (wildlife::nearestHerd(SP_DEER, 0, 0, hx, hz)) { float x = hx + 5, z = hz + 4; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.4f, z}; E.player.yaw = std::atan2(-(hx - x), -(hz - z)); E.player.pitch = -0.15f; } }},
        {846, "deer2", [=] {}},
        {852, "", [=] { E.time = 12.0f; float hx, hz; if (wildlife::nearestHerd(SP_LEOPARD, 0, 0, hx, hz)) { float x = hx + 2.2f, z = hz + 1.6f; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.0f, z}; E.player.velY = 0; E.player.yaw = std::atan2(-(hx - x), -(hz - z)); E.player.pitch = -0.25f; } }},
        {853, "leo", [=] {}},
        {850, "wolf", [=] { float hx, hz; if (wildlife::nearestHerd(SP_WOLF, 0, 0, hx, hz)) { float x = hx + 6, z = hz + 5; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.5f, z}; E.player.yaw = std::atan2(-(hx - x), -(hz - z)); E.player.pitch = -0.15f; E.player.hp = 1e6f; } }},
        {600, "kalyan", [=] { A.tab = Tab::None; float hx = 5, hz = -4; npcs::position("hunter", hx, hz); std::printf("hunter at %.1f %.1f\n", hx, hz); float x = hx, z = hz + 2.4f; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.55f, z}; E.player.yaw = 0; E.player.pitch = -0.1f; }},
        {606, "kalyan2", [=] {}},
        {610, "temple", [=] { E.time = 19.1f; ([=] { A.tab = Tab::None; const Shrine& sh = world::SHRINES[0]; float x = sh.x + std::sin(sh.yaw) * 26, z = sh.z + std::cos(sh.yaw) * 26; E.player.pos = {x, std::max(world::getTerrainHeight(x, z), sh.y) + 3.5f, z}; E.player.velY = 0; E.player.yaw = sh.yaw; E.player.pitch = -0.08f; })(); }},
        {616, "temple2", [=] { restoreSeason(3, 1, 19.8f); E.time = 19.8f; A.tab = Tab::None; E.cutscene.dur = 0; const Shrine& sh = world::SHRINES[0]; float L = std::sqrt(sh.x * sh.x + sh.z * sh.z); float x = sh.x - sh.x / L * 30, z = sh.z - sh.z / L * 30; E.player.pos = {x, world::getTerrainHeight(x, z) + 1.7f, z}; E.player.velY = 0; E.player.yaw = std::atan2(sh.x, sh.z); E.player.pitch = -0.45f; g_profMs.clear(); g_profTri.clear(); g_profDraw.clear(); g_prof = platform::hasArg("--prof"); }},
        {624, "temple3", [=] { if (g_prof) { for (auto& kv : g_profMs) std::printf("prof %8.1f ms %9lld tri %5lld draws  %s\n", kv.second, g_profTri[kv.first], g_profDraw[kv.first], kv.first.c_str()); } g_prof = false; }},
        {826, "options", [=] { screens::openOptions(true); }},
        {830, "place", [=] { screens::openOptions(false); A.menuOpen = false; look(4, 16, 0.3f, -0.15f)(); for (auto& k : {"wood", "stone", "fiber"}) A.inv[k] = 99; app::startStructure(SK_COTTAGE); A.showStyle = true; }},
        {832, "dialogue", [=] { actions::cancelPlacement(); actions::openDialogue("carpenter"); }},
        {834, "people", [=] { A.dialogue = -1; A.population = 3; A.tab = Tab::Buildings; A.buildSection = 4; }},
        {836, "victory", [=] { A.tab = Tab::None; A.status = Status::Victory; }},
        {960, "end", [=] {}},
    };
    std::string only = platform::argValue("--shots");
    if (!only.empty()) {
      std::vector<Shot> keep;
      int f = 0;
      for (auto& s : shots) {
        bool want = s.name.empty() || s.name == "end" || ("," + only + ",").find("," + s.name + ",") != std::string::npos;
        if (!want) continue;
        f = s.name.empty() ? std::max(s.frame, f + 2) : f + 70;
        keep.push_back({f, s.name, s.setup});
      }
      shots = keep;
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
    bool takeShot = in.pressed[K_F2];
    if (in.pressed[K_ESC]) screens::escape();
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

    g_pp.on = screens::statsVisible();
    if (g_pp.on) g_pp.begin();
    forest::profOpen = g_pp.on ? +[](const char* n) { return g_pp.open(n); } : nullptr;
    forest::profClose = g_pp.on ? +[](int i) { g_pp.close(i); } : nullptr;
    // pause menu freezes the world: game time, animals, villagers, wind
    float simDt = (A.menuOpen && E.playing && !test) ? 0.0f : dt;
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
    if (!freeze) frozenValid = false;
    bool skipWorld = freeze && frozenValid && frozenW == W && frozenH == H;
    if (!skipWorld && (sky::shadowNeedsUpdate() || sky::shadowStaticDirty())) PROF(drawShadows());
    float rs = clampf(g_settings.adaptiveQuality ? renderScale : g_settings.renderScale, 0.4f, 1.0f);
    if (skipWorld) rs = frozenRs;
    bool sunOn = !cut && sunfx::wanted();
    bool scaled = rs < 0.99f || g_settings.fxaa || sunOn || freeze;
    if (scaled) {
      scene.ensure(std::max(1, (int)(W * rs)), std::max(1, (int)(H * rs)));
      glBindFramebuffer(GL_FRAMEBUFFER, scene.drawFbo());
      glViewport(0, 0, scene.w, scene.h);
    }
    g_sunDepth = scaled && sunOn ? scene.depth : 0;
    g_sunW = scene.w;
    g_sunH = scene.h;
    if (!skipWorld) drawWorld(dt, E.playing && !E.uiOpen && !cut);
    if (freeze && !skipWorld) { frozenValid = true; frozenW = W; frozenH = H; frozenRs = rs; }
    if (scaled) {
      if (!skipWorld) scene.resolve(GL_COLOR_BUFFER_BIT);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glViewport(0, 0, W, H);
      static const char* UPVS = "out vec2 v_uv;\nvoid main(){ vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); v_uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }\n";
      static const char* UPFS = R"(in vec2 v_uv;
uniform sampler2D u_src;
uniform vec2 u_px;
uniform int u_fxaa;
out vec4 o;
float lum(vec3 c){ return dot(c, vec3(0.299, 0.587, 0.114)); }
void main(){
  vec3 m = texture(u_src, v_uv).rgb;
  if (u_fxaa == 0) { o = vec4(m, 1.0); return; }
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
  o = vec4((lB < lMin || lB > lMax) ? a : b, 1.0);
}
)";
      Shader& up = render::program("upscale", UPVS, UPFS);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_BLEND);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, scene.color);
      up.set1i("u_src", 0);
      up.set2f("u_px", 1.0f / scene.w, 1.0f / scene.h);
      up.set1i("u_fxaa", g_settings.fxaa ? 1 : 0);
      PROF(render::drawFullscreen());
      glEnable(GL_DEPTH_TEST);
      if (sunOn) PROF(sunfx::composite(W, H));
    }

    if (E.playing) {
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
    if (test && shotIdx < shots.size() && frames == shots[shotIdx].frame) {
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
    if (!test) {
      bool menu = A.status != Status::Playing || A.menuOpen || A.tab != Tab::None || A.howToOpen;
      int cap = !platform::focused() ? 15 : menu ? g_settings.menuFps : g_settings.fpsLimit;
      static int hz = platform::refreshRate();
      if (g_settings.vsync && cap >= hz - 1) cap = 0;
      if (cap > 0) {
        static double nextT = 0;
        double frame = 1.0 / cap, nowT = platform::time();
        if (nextT < nowT - frame) nextT = nowT;
        nextT += frame;
        double wait = nextT - platform::time();
        if (wait > 0.002) std::this_thread::sleep_for(std::chrono::duration<double>(wait - 0.0015));
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
      if (g_pp.on) g_pp.publish();
      lastFs = fs;
      if (g_settings.adaptiveDetail && !test && E.playing && fs.gpuMs > 0) {
        static float dSlow = 0, dFast = 0;
        float budget = 1000.0f / (g_settings.fpsLimit > 0 ? std::min(g_settings.fpsLimit, platform::refreshRate()) : platform::refreshRate());
        if (fs.gpuMs > budget * 0.85f) dSlow += (float)statT, dFast = 0;
        else if (fs.gpuMs < budget * 0.6f) dFast += (float)statT, dSlow = 0;
        else dSlow = dFast = 0;
        float& ds = g_settings.detailScale;
        if (dSlow >= 1.0f && ds > 0.45f) { ds = std::max(0.45f, ds - 0.1f); dSlow = 0; forest::markDirty(); }
        else if (dFast >= 4.0f && ds < 1.0f) { ds = std::min(1.0f, ds + 0.05f); dFast = 0; forest::markDirty(); }
      }
      if (g_settings.adaptiveQuality && !test && E.playing) {
        float budget = 1000.0f / (g_settings.fpsLimit > 0 ? g_settings.fpsLimit : 60);
        if (fs.gpuMs > budget * 0.78f || fs.fps < (g_settings.fpsLimit > 0 ? g_settings.fpsLimit : 60) * 0.92f) slowT += (float)statT, fastT = 0;
        else if (fs.gpuMs < budget * 0.55f) fastT += (float)statT, slowT = 0;
        bool gpuBound = fs.gpuMs > fs.cpuMs * 0.8f;
        if (slowT > 2 && gpuBound && renderScale > 0.85f) {
          renderScale = std::max(0.85f, renderScale - 0.05f);
          slowT = 0;
        } else if (slowT > 2 && g_settings.grassRadius > baseGrass * 0.55f) {
          g_settings.grassRadius *= 0.85f;
          g_settings.undergrowthRadius *= 0.88f;
          forest::markDirty();
          slowT = 0;
        } else if (fastT > 6 && renderScale < g_settings.renderScale) {
          renderScale = std::min(g_settings.renderScale, renderScale + 0.1f);
          fastT = 0;
        } else if (fastT > 8 && g_settings.grassRadius < baseGrass) {
          g_settings.grassRadius = std::min(baseGrass, g_settings.grassRadius * 1.1f);
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
