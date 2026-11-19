#include "platform.h"
#include "../gfx/gl.h"
#include "../gfx/gfx.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

Settings g_settings;

namespace {
using platform::BINDS; using platform::BIND_COUNT; using platform::boundKey; using platform::keyName;
GLFWwindow* g_win = nullptr;
Input g_in;
std::vector<std::string> g_args;
std::string g_assetDir;
std::string g_exeDir;
bool g_fullscreen = false;
int g_winX = 100, g_winY = 100, g_winW = 1600, g_winH = 900;
double g_lastX = 0, g_lastY = 0;
bool g_haveLast = false;

int g_lastRaw = -1;
int mapKey(int k) {
  // rebindable actions first (Options > Controls), then the fixed keys (menus, arrows, function keys)
  for (int i = 0; i < BIND_COUNT; i++) if (boundKey(BINDS[i].key) == k) return BINDS[i].key;
  switch (k) {
    case GLFW_KEY_RIGHT_SHIFT: return boundKey(K_SHIFT) == GLFW_KEY_LEFT_SHIFT ? K_SHIFT : -1;
    case GLFW_KEY_ESCAPE: return K_ESC; case GLFW_KEY_ENTER: case GLFW_KEY_KP_ENTER: return K_ENTER;
    case GLFW_KEY_UP: return K_UP; case GLFW_KEY_DOWN: return K_DOWN; case GLFW_KEY_LEFT: return K_LEFT; case GLFW_KEY_RIGHT: return K_RIGHT;
    case GLFW_KEY_1: return K_1; case GLFW_KEY_2: return K_2; case GLFW_KEY_3: return K_3;
    case GLFW_KEY_4: return K_4; case GLFW_KEY_5: return K_5; case GLFW_KEY_6: return K_6;
    case GLFW_KEY_F2: return K_F2; case GLFW_KEY_F3: return K_F3; case GLFW_KEY_F4: return K_F4; case GLFW_KEY_F11: return K_F11;
    case GLFW_KEY_LEFT_CONTROL: return K_CTRL;
    default: return -1;
  }
}

void onKey(GLFWwindow*, int key, int, int action, int) {
  if (action == GLFW_PRESS) g_lastRaw = key;
  int k = mapKey(key);
  if (k < 0) return;
  if (action == GLFW_PRESS) { g_in.down[k] = true; g_in.pressed[k] = true; }
  else if (action == GLFW_RELEASE) g_in.down[k] = false;
}
void onButton(GLFWwindow*, int button, int action, int) {
  int b = button == GLFW_MOUSE_BUTTON_LEFT ? 0 : button == GLFW_MOUSE_BUTTON_RIGHT ? 1 : button == GLFW_MOUSE_BUTTON_MIDDLE ? 2 : -1;
  if (b < 0) return;
  if (action == GLFW_PRESS) { g_in.mouseDown[b] = true; g_in.mousePressed[b] = true; }
  else { g_in.mouseDown[b] = false; g_in.mouseReleased[b] = true; }
}
void onCursor(GLFWwindow*, double x, double y) {
  if (g_haveLast) { g_in.dx += (float)(x - g_lastX); g_in.dy += (float)(y - g_lastY); }
  g_lastX = x; g_lastY = y; g_haveLast = true;
  if (!g_in.captured) { g_in.mx = (float)x; g_in.my = (float)y; }
}
void onScroll(GLFWwindow*, double, double y) { g_in.wheel += (float)y; }
void onFocus(GLFWwindow*, int focused) {
  if (!focused) {
    for (bool& d : g_in.down) d = false;
    for (bool& d : g_in.mouseDown) d = false;
    g_in.focusLost = true;
  }
}

std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
  return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
}  // namespace


// ---- GL state shadow -------------------------------------------------------------------------------------------
// The renderer saves and restores state with glGetIntegerv / glIsEnabled dozens of times a frame. With a threaded
// driver each of those waits for every queued command to finish (1-2 ms a time on the P600: it showed up as
// "sunfx.occlusion" and "hizCapture" on the CPU list). The few values ever asked for are tracked here as they are
// set, and the queries answer from the copy without touching the driver.
namespace {
using platform::BINDS; using platform::BIND_COUNT; using platform::boundKey; using platform::keyName;
PFNGLVIEWPORTPROC r_viewport; PFNGLBINDFRAMEBUFFERPROC r_bindFb; PFNGLENABLEPROC r_enable; PFNGLDISABLEPROC r_disable;
PFNGLGETINTEGERVPROC r_getInt; PFNGLISENABLEDPROC r_isEnabled; PFNGLACTIVETEXTUREPROC r_activeTex; PFNGLDEPTHFUNCPROC r_depthFunc;
PFNGLDELETEFRAMEBUFFERSPROC r_delFb;
GLint s_vp[4], s_drawFb = 0, s_readFb = 0, s_active = GL_TEXTURE0, s_depthFunc = GL_LESS;
const GLenum S_CAPS[4] = {GL_DEPTH_TEST, GL_BLEND, GL_SCISSOR_TEST, GL_CULL_FACE};
GLboolean s_cap[4];
int capIx(GLenum c) { for (int i = 0; i < 4; i++) if (S_CAPS[i] == c) return i; return -1; }
void GLAD_API_PTR h_viewport(GLint x, GLint y, GLsizei w, GLsizei h) { s_vp[0] = x; s_vp[1] = y; s_vp[2] = w; s_vp[3] = h; r_viewport(x, y, w, h); }
void GLAD_API_PTR h_bindFb(GLenum t, GLuint f) {
  if (t == GL_FRAMEBUFFER || t == GL_DRAW_FRAMEBUFFER) s_drawFb = (GLint)f;
  if (t == GL_FRAMEBUFFER || t == GL_READ_FRAMEBUFFER) s_readFb = (GLint)f;
  r_bindFb(t, f);
}
void GLAD_API_PTR h_delFb(GLsizei n, const GLuint* f) {   // deleting a bound framebuffer rebinds 0
  for (GLsizei i = 0; i < n; i++) { if ((GLint)f[i] == s_drawFb) s_drawFb = 0; if ((GLint)f[i] == s_readFb) s_readFb = 0; }
  r_delFb(n, f);
}
void GLAD_API_PTR h_enable(GLenum c) { int i = capIx(c); if (i >= 0) s_cap[i] = GL_TRUE; r_enable(c); }
void GLAD_API_PTR h_disable(GLenum c) { int i = capIx(c); if (i >= 0) s_cap[i] = GL_FALSE; r_disable(c); }
GLboolean GLAD_API_PTR h_isEnabled(GLenum c) { int i = capIx(c); return i >= 0 ? s_cap[i] : r_isEnabled(c); }
void GLAD_API_PTR h_activeTex(GLenum t) { s_active = (GLint)t; r_activeTex(t); }
void GLAD_API_PTR h_depthFunc(GLenum f) { s_depthFunc = (GLint)f; r_depthFunc(f); }
void GLAD_API_PTR h_getInt(GLenum p, GLint* d) {
  switch (p) {
    case GL_VIEWPORT: d[0] = s_vp[0]; d[1] = s_vp[1]; d[2] = s_vp[2]; d[3] = s_vp[3]; return;
    case GL_DRAW_FRAMEBUFFER_BINDING: *d = s_drawFb; return;   // same value as GL_FRAMEBUFFER_BINDING
    case GL_READ_FRAMEBUFFER_BINDING: *d = s_readFb; return;
    case GL_ACTIVE_TEXTURE: *d = s_active; return;
    case GL_DEPTH_FUNC: *d = s_depthFunc; return;
    default: r_getInt(p, d);
  }
}
void installStateShadow() {
  if (std::getenv("HW_NOSHADOWSTATE")) return;
  glGetIntegerv(GL_VIEWPORT, s_vp);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &s_drawFb);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &s_readFb);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &s_active);
  glGetIntegerv(GL_DEPTH_FUNC, &s_depthFunc);
  for (int i = 0; i < 4; i++) s_cap[i] = glIsEnabled(S_CAPS[i]);
  r_viewport = glad_glViewport; glad_glViewport = h_viewport;
  r_bindFb = glad_glBindFramebuffer; glad_glBindFramebuffer = h_bindFb;
  r_delFb = glad_glDeleteFramebuffers; glad_glDeleteFramebuffers = h_delFb;
  r_enable = glad_glEnable; glad_glEnable = h_enable;
  r_disable = glad_glDisable; glad_glDisable = h_disable;
  r_isEnabled = glad_glIsEnabled; glad_glIsEnabled = h_isEnabled;
  r_activeTex = glad_glActiveTexture; glad_glActiveTexture = h_activeTex;
  r_depthFunc = glad_glDepthFunc; glad_glDepthFunc = h_depthFunc;
  r_getInt = glad_glGetIntegerv; glad_glGetIntegerv = h_getInt;
}
}  // namespace

namespace platform {

const std::vector<std::string>& args() { return g_args; }
bool hasArg(const std::string& a) {
  for (auto& s : g_args) if (s == a) return true;
  return false;
}
std::string argValue(const std::string& a, const std::string& def) {
  for (size_t i = 0; i + 1 < g_args.size(); i++) if (g_args[i] == a) return g_args[i + 1];
  return def;
}

bool fileExists(const std::string& p) {
  std::error_code ec;
  return fs::exists(fs::u8path(p), ec);
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
  std::ifstream f(fs::u8path(path), std::ios::binary);
  if (!f) return false;
  f.seekg(0, std::ios::end);
  std::streamoff n = f.tellg();
  if (n < 0) return false;
  f.seekg(0, std::ios::beg);
  out.resize((size_t)n);
  if (n > 0) f.read((char*)out.data(), n);
  return (bool)f;
}

bool writeFile(const std::string& path, const std::string& data) {
  std::error_code ec;
  fs::create_directories(fs::u8path(path).parent_path(), ec);
  std::string tmp = path + ".tmp";
  {
    std::ofstream f(fs::u8path(tmp), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(data.data(), (std::streamsize)data.size());
    if (!f) return false;
  }
  fs::rename(fs::u8path(tmp), fs::u8path(path), ec);
  if (ec) {
    fs::remove(fs::u8path(path), ec);
    fs::rename(fs::u8path(tmp), fs::u8path(path), ec);
  }
  return !ec;
}

std::vector<std::string> listFiles(const std::string& dir, const std::string& ext) {
  std::vector<std::string> out;
  std::error_code ec;
  if (!fs::exists(fs::u8path(dir), ec)) return out;
  for (auto& e : fs::directory_iterator(fs::u8path(dir), ec)) {
    if (!e.is_regular_file()) continue;
    std::string p = e.path().u8string();
    if (p.size() >= ext.size() && p.compare(p.size() - ext.size(), ext.size(), ext) == 0) out.push_back(p);
  }
  std::sort(out.begin(), out.end());
  return out;
}

std::string assetPath(const std::string& rel) { return g_assetDir + "/" + rel; }

std::string findAsset(const std::string& name) {
  const char* subs[] = {"", "models/", "low poly/", "glb/"};
  for (const char* s : subs) {
    std::string p = g_assetDir + "/" + s + name;
    if (fileExists(p)) return p;
  }
  return "";
}

std::string saveDir() {
#ifdef _WIN32
  const char* app = std::getenv("APPDATA");
  if (app) return std::string(app) + "/HimalayanWilds";
#else
  const char* xdg = std::getenv("XDG_DATA_HOME");
  if (xdg && *xdg) return std::string(xdg) + "/HimalayanWilds";
  const char* home = std::getenv("HOME");
  if (home) return std::string(home) + "/.local/share/HimalayanWilds";
#endif
  return g_exeDir + "/saves";
}

void loadSettings() {
  std::vector<std::string> candidates = {g_exeDir + "/config.ini", "config.ini"};
  for (auto& path : candidates) {
    std::ifstream f(fs::u8path(path));
    if (!f) continue;
    std::string line;
    while (std::getline(f, line)) {
      line = trim(line);
      if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;
      size_t eq = line.find('=');
      if (eq == std::string::npos) continue;
      std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
      float fv = (float)std::atof(v.c_str());
      int iv = std::atoi(v.c_str());
      bool bv = v == "1" || v == "true" || v == "yes" || v == "on";
      Settings& s = g_settings;
      if (k == "width") s.width = iv;
      else if (k == "height") s.height = iv;
      else if (k == "fullscreen") s.fullscreen = bv;
      else if (k == "vsync") s.vsync = bv;
      else if (k == "msaa") s.msaa = iv;
      else if (k == "fxaa") s.fxaa = bv;
      else if (k == "edge_upscale") s.edgeUpscale = bv;
      else if (k == "hud") s.hudOn = bv;
      else if (k == "hud_hide") s.hudHide = (uint32_t)std::strtoul(v.c_str(), nullptr, 10);
      else if (k.rfind("key_", 0) == 0) {
        for (int i = 0; i < BIND_COUNT; i++) {
          std::string nm = BINDS[i].name, id;
          for (char c : nm) id += std::isalnum((unsigned char)c) ? (char)std::tolower((unsigned char)c) : '_';
          if (k == "key_" + id) s.binds[BINDS[i].key] = iv;
        }
      }
      else if (k == "ssao") s.ssao = bv;
      else if (k == "volumetric_light") s.volumetric = bv;
      else if (k == "volumetric_fog") s.volFog = bv;
      else if (k == "water_reflections") s.waterRefl = bv;
      else if (k == "shadow_size") s.shadowSize = iv;
      else if (k == "mouse_sensitivity") s.mouseSensitivity = fv;
      else if (k == "player_speed") s.playerSpeed = fv;
      else if (k == "sprint_multiplier") s.sprintMultiplier = fv;
      else if (k == "fov") s.fov = fv;
      else if (k == "gravity") s.gravity = fv;
      else if (k == "music_volume") s.musicVolume = fv;
      else if (k == "sfx_volume") s.sfxVolume = fv;
      else if (k == "grass_radius") s.grassRadius = fv;
      else if (k == "undergrowth_radius") s.undergrowthRadius = fv;
      else if (k == "tree_radius") s.treeRadius = fv;
      else if (k == "rock_radius") s.rockRadius = fv;
      else if (k == "texture_size") s.textureSize = iv;
      else if (k == "adaptive_quality") s.adaptiveQuality = bv;
      else if (k == "adaptive_detail") s.adaptiveDetail = bv;
      else if (k == "tri_budget") s.triBudget = iv;
      else if (k == "ui_scale") s.uiScale = fv;
      else if (k == "render_scale") s.renderScale = fv;
      else if (k == "render_resolution") s.renderRes = iv;
      else if (k == "draw_distance") s.drawDistance = fv;
      else if (k == "full_map") s.fullMap = bv;
      else if (k == "fps_limit") s.fpsLimit = iv;
      else if (k == "menu_fps") s.menuFps = iv;
      else if (k == "leaf_prepass") s.leafPrepass = v == "auto" ? 2 : bv ? 1 : 0;
      else if (k == "eco_mode") s.eco = bv;
      else if (k == "temporal_upscale_v2") s.temporalUpscale = bv;  // new key: the old one defaulted on
      else if (k == "weather") s.weather = v == "rain" ? 1 : v == "clear" ? 2 : 0;
    }
    break;
  }
}

bool init(int argc, char** argv) {
  for (int i = 1; i < argc; i++) g_args.push_back(argv[i]);
  std::error_code ec;
  fs::path exe = argc > 0 ? fs::absolute(fs::u8path(argv[0]), ec).parent_path() : fs::current_path(ec);
  g_exeDir = exe.u8string();
  std::vector<fs::path> roots = {fs::current_path(ec) / "assets", exe / "assets", exe.parent_path() / "assets", exe.parent_path().parent_path() / "assets"};
  std::string custom = argValue("--assets");
  if (!custom.empty()) roots.insert(roots.begin(), fs::u8path(custom));
  for (auto& r : roots) {
    if (fs::exists(r, ec)) { g_assetDir = r.u8string(); break; }
  }
  if (g_assetDir.empty()) g_assetDir = (exe / "assets").u8string();
  loadSettings();
  if (hasArg("--windowed")) g_settings.fullscreen = false;
  std::string w = argValue("--width"), h = argValue("--height");
  if (!w.empty()) g_settings.width = std::atoi(w.c_str());
  if (!h.empty()) g_settings.height = std::atoi(h.c_str());
  if (hasArg("--novsync")) g_settings.vsync = false;
  std::string msaa = argValue("--msaa");
  if (!msaa.empty()) g_settings.msaa = std::atoi(msaa.c_str());

  if (!glfwInit()) { std::fprintf(stderr, "Failed to start GLFW\n"); return false; }
  // ask for OpenGL 4.3 (GPU-side culling with compute shaders); fall back to 3.3 when the driver has no 4.3
  bool want43 = !std::getenv("HW_GL33");
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, want43 ? 4 : 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, std::max(0, g_settings.msaa));
  glfwWindowHint(GLFW_DEPTH_BITS, 24);
  glfwWindowHint(GLFW_STENCIL_BITS, 0);
  GLFWmonitor* mon = g_settings.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
  int W = g_settings.width, H = g_settings.height;
  if (mon) {
    const GLFWvidmode* vm = glfwGetVideoMode(mon);
    W = vm->width; H = vm->height;
    g_fullscreen = true;
  }
  g_win = glfwCreateWindow(W, H, "Himalayan Wilds", mon, nullptr);
  if (!g_win && want43) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    g_win = glfwCreateWindow(W, H, "Himalayan Wilds", mon, nullptr);
  }
  if (!g_win && g_settings.msaa > 0) {
    glfwWindowHint(GLFW_SAMPLES, 0);
    g_win = glfwCreateWindow(W, H, "Himalayan Wilds", mon, nullptr);
  }
  if (!g_win) { std::fprintf(stderr, "Failed to create an OpenGL 3.3 window\n"); glfwTerminate(); return false; }
  glfwMakeContextCurrent(g_win);
  if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) { std::fprintf(stderr, "Failed to load OpenGL\n"); return false; }
  installStateShadow();
  gfx::loadExtras([](const char* n) { return (void*)glfwGetProcAddress(n); });
  glfwSwapInterval(g_settings.vsync ? 1 : 0);
  glfwSetKeyCallback(g_win, onKey);
  glfwSetMouseButtonCallback(g_win, onButton);
  glfwSetCursorPosCallback(g_win, onCursor);
  glfwSetScrollCallback(g_win, onScroll);
  glfwSetWindowFocusCallback(g_win, onFocus);
  if (glfwRawMouseMotionSupported()) glfwSetInputMode(g_win, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
  return true;
}

void shutdown() {
  if (g_win) glfwDestroyWindow(g_win);
  glfwTerminate();
}

bool shouldClose() { return glfwWindowShouldClose(g_win); }
void requestClose() { glfwSetWindowShouldClose(g_win, 1); }
void pollEvents() { glfwPollEvents(); }
void swap() { glfwSwapBuffers(g_win); }
int width() { int w, h; glfwGetFramebufferSize(g_win, &w, &h); return w; }
int height() { int w, h; glfwGetFramebufferSize(g_win, &w, &h); return h; }
double time() { return glfwGetTime(); }
float pixelRatio() {
  int w, h, fw, fh;
  glfwGetWindowSize(g_win, &w, &h);
  glfwGetFramebufferSize(g_win, &fw, &fh);
  return w > 0 ? (float)fw / w : 1.0f;
}
bool focused() { return glfwGetWindowAttrib(g_win, GLFW_FOCUSED) != 0; }
void* glProc(const char* name) { return (void*)glfwGetProcAddress(name); }
bool minimized() { return glfwGetWindowAttrib(g_win, GLFW_ICONIFIED) != 0; }
int refreshRate() {
  GLFWmonitor* m = glfwGetWindowMonitor(g_win);
  if (!m) m = glfwGetPrimaryMonitor();
  const GLFWvidmode* v = m ? glfwGetVideoMode(m) : nullptr;
  return v && v->refreshRate > 0 ? v->refreshRate : 60;
}
Input& input() { return g_in; }

void clearFrameInput() {
  for (bool& p : g_in.pressed) p = false;
  for (bool& p : g_in.mousePressed) p = false;
  for (bool& p : g_in.mouseReleased) p = false;
  g_in.dx = g_in.dy = 0;
  g_in.wheel = 0;
  g_in.focusLost = false;
}

void setCapture(bool on) {
  if (on == g_in.captured) return;
  g_in.captured = on;
  glfwSetInputMode(g_win, GLFW_CURSOR, on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
  g_haveLast = false;
  if (!on) {
    int w, h;
    glfwGetWindowSize(g_win, &w, &h);
    glfwSetCursorPos(g_win, w * 0.5, h * 0.5);
    g_in.mx = w * 0.5f; g_in.my = h * 0.5f;
  }
}

bool isFullscreen() { return g_fullscreen; }
static int g_swapCur = -1;
const BindDef BINDS[] = {
    {K_W, "Move forward", GLFW_KEY_W}, {K_S, "Move back", GLFW_KEY_S}, {K_A, "Move left", GLFW_KEY_A}, {K_D, "Move right", GLFW_KEY_D},
    {K_SPACE, "Jump", GLFW_KEY_SPACE}, {K_SHIFT, "Sprint", GLFW_KEY_LEFT_SHIFT}, {K_C, "Crouch", GLFW_KEY_C},
    {K_E, "Interact / rotate right", GLFW_KEY_E}, {K_Q, "Rotate left", GLFW_KEY_Q}, {K_F, "Quick shot", GLFW_KEY_F},
    {K_R, "Craft 10 arrows", GLFW_KEY_R}, {K_L, "Lantern", GLFW_KEY_L}, {K_TAB, "Pouch", GLFW_KEY_TAB},
    {K_J, "Journal", GLFW_KEY_J}, {K_B, "Build", GLFW_KEY_B}, {K_M, "Map", GLFW_KEY_M}, {K_K, "Skills", GLFW_KEY_K},
    {K_I, "Inventory", GLFW_KEY_I}, {K_P, "Photo mode", GLFW_KEY_P}, {K_H, "Hide / show HUD", GLFW_KEY_H},
};
const int BIND_COUNT = (int)(sizeof(BINDS) / sizeof(BINDS[0]));
int boundKey(int action) {
  int b = g_settings.binds[action];
  if (b) return b;
  for (int i = 0; i < BIND_COUNT; i++) if (BINDS[i].key == action) return BINDS[i].def;
  return 0;
}
std::string keyName(int k) {
  switch (k) {
    case GLFW_KEY_SPACE: return "Space"; case GLFW_KEY_LEFT_SHIFT: return "L Shift"; case GLFW_KEY_RIGHT_SHIFT: return "R Shift";
    case GLFW_KEY_LEFT_CONTROL: return "L Ctrl"; case GLFW_KEY_RIGHT_CONTROL: return "R Ctrl"; case GLFW_KEY_LEFT_ALT: return "L Alt";
    case GLFW_KEY_RIGHT_ALT: return "R Alt"; case GLFW_KEY_TAB: return "Tab"; case GLFW_KEY_CAPS_LOCK: return "Caps";
    case GLFW_KEY_BACKSPACE: return "Backspace"; case GLFW_KEY_ENTER: return "Enter"; case GLFW_KEY_ESCAPE: return "Esc";
    case GLFW_KEY_UP: return "Up"; case GLFW_KEY_DOWN: return "Down"; case GLFW_KEY_LEFT: return "Left"; case GLFW_KEY_RIGHT: return "Right";
    case GLFW_KEY_INSERT: return "Ins"; case GLFW_KEY_DELETE: return "Del"; case GLFW_KEY_HOME: return "Home"; case GLFW_KEY_END: return "End";
    case GLFW_KEY_PAGE_UP: return "PgUp"; case GLFW_KEY_PAGE_DOWN: return "PgDn";
    default: break;
  }
  if (k >= GLFW_KEY_F1 && k <= GLFW_KEY_F25) return "F" + std::to_string(k - GLFW_KEY_F1 + 1);
  if (k >= GLFW_KEY_KP_0 && k <= GLFW_KEY_KP_9) return "Num " + std::to_string(k - GLFW_KEY_KP_0);
  if ((k >= GLFW_KEY_A && k <= GLFW_KEY_Z) || (k >= GLFW_KEY_0 && k <= GLFW_KEY_9)) return std::string(1, (char)k);
  if (const char* n = glfwGetKeyName(k, 0)) { std::string s = n; for (auto& c : s) c = (char)std::toupper((unsigned char)c); return s; }
  return "#" + std::to_string(k);
}
std::string actionKeyName(int action) { return keyName(boundKey(action)); }
void setBind(int action, int physical) {
  int old = boundKey(action);
  for (int i = 0; i < BIND_COUNT; i++)
    if (BINDS[i].key != action && boundKey(BINDS[i].key) == physical) g_settings.binds[BINDS[i].key] = old;   // swap
  g_settings.binds[action] = physical;
}
void resetBinds() { for (int& b : g_settings.binds) b = 0; }
int takeLastRawKey() { int k = g_lastRaw; g_lastRaw = -1; return k; }

void setVsync(bool on) { g_settings.vsync = on; g_swapCur = on ? 1 : 0; glfwSwapInterval(g_swapCur); }
void setSwapInterval(int n) { if (n != g_swapCur) { g_swapCur = n; glfwSwapInterval(n); } }

bool saveSettings() {
  const Settings& s = g_settings;
  char b[2048];
  std::snprintf(b, sizeof b,
                "; Himalayan Wilds settings (also editable from Options in game)\n[video]\nwidth = %d\nheight = %d\nfullscreen = %s\nvsync = %s\nmsaa = %d\nfxaa = %s\n; SSAO: soft screen-space contact shadows (costs ~0.3-0.6 ms; baked AO already covers most of it)\nssao = %s\n; volumetric light: sun shafts through trees, fog and over ridges (~0.8-1.2 ms)\nvolumetric_light = %s\n; volumetric fog: drifting ground fog lit by the sun, lanterns and fires (~0.3-0.5 ms)\nvolumetric_fog = %s\n; water reflections: lakes and rivers mirror the trees, banks and sky (~1 ms with water on screen)\nwater_reflections = %s\nshadow_size = %d\ntexture_size = %d\nadaptive_quality = %s\nadaptive_detail = %s\ntri_budget = %d\nui_scale = %.2f\n; edge_upscale: sharper edge-aware upscaling when render_scale is below 100%%\nedge_upscale = %s\nrender_scale = %.2f\n; render_resolution: 0 = native, or 480 / 600 / 720 / 900 / 1080 / 1440 / 2160 (the 3D view is drawn at this height, then scaled to the window)\nrender_resolution = %d\ndraw_distance = %.2f\nfull_map = %s\nfps_limit = %d\nmenu_fps = %d\n; leaf depth pre-pass: auto (timed on your GPU), on or off. Turn off if leaves show speckles\nleaf_prepass = %s\n; temporal upscaling: renders fewer pixels and rebuilds detail from previous frames\ntemporal_upscale_v2 = %s\n; eco: automatic quality only raises detail while the GPU stays under ~50%% busy (cooler, quieter)\neco_mode = %s\n\n[world]\n; weather: auto (showers come and go), rain (always raining, except in snow), clear (never rains)\nweather = %s\ngrass_radius = %.0f\nundergrowth_radius = %.0f\ntree_radius = %.0f\nrock_radius = %.0f\n\n[player]\nfov = %.0f\nmouse_sensitivity = %.4f\nplayer_speed = %.2f\nsprint_multiplier = %.2f\ngravity = %.1f\n\n[audio]\nmusic_volume = %.2f\nsfx_volume = %.2f\n",
                s.width, s.height, g_fullscreen ? "true" : "false", s.vsync ? "true" : "false", s.msaa, s.fxaa ? "true" : "false", s.ssao ? "true" : "false", s.volumetric ? "true" : "false", s.volFog ? "true" : "false", s.waterRefl ? "true" : "false", s.shadowSize, s.textureSize, s.adaptiveQuality ? "true" : "false", s.adaptiveDetail ? "true" : "false", s.triBudget, s.uiScale, s.edgeUpscale ? "true" : "false", s.renderScale, s.renderRes, s.drawDistance, s.fullMap ? "true" : "false", s.fpsLimit, s.menuFps, s.leafPrepass == 2 ? "auto" : s.leafPrepass ? "on" : "off", s.temporalUpscale ? "true" : "false", s.eco ? "true" : "false",
                s.weather == 1 ? "rain" : s.weather == 2 ? "clear" : "auto", s.grassRadius, s.undergrowthRadius, s.treeRadius, s.rockRadius, s.fov, s.mouseSensitivity, s.playerSpeed, s.sprintMultiplier, s.gravity, s.musicVolume, s.sfxVolume);
  std::string out = b;
  char hb[128];
  std::snprintf(hb, sizeof hb, "\n[hud]\nhud = %s\n; hidden HUD pieces (bit mask, set from Options > HUD)\nhud_hide = %u\n", s.hudOn ? "true" : "false", s.hudHide);
  out += hb;
  out += "\n[controls]\n; physical key codes (GLFW); set from Options > Controls, removed lines fall back to the default\n";
  for (int i = 0; i < BIND_COUNT; i++) {
    if (!s.binds[BINDS[i].key]) continue;
    std::string nm = BINDS[i].name, id;
    for (char c : nm) id += std::isalnum((unsigned char)c) ? (char)std::tolower((unsigned char)c) : '_';
    out += "key_" + id + " = " + std::to_string(s.binds[BINDS[i].key]) + "   ; " + keyName(s.binds[BINDS[i].key]) + "\n";
  }
  return writeFile(g_exeDir + "/config.ini", out);
}

void toggleFullscreen() {
  if (!g_fullscreen) {
    glfwGetWindowPos(g_win, &g_winX, &g_winY);
    glfwGetWindowSize(g_win, &g_winW, &g_winH);
    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* vm = glfwGetVideoMode(mon);
    glfwSetWindowMonitor(g_win, mon, 0, 0, vm->width, vm->height, vm->refreshRate);
  } else {
    glfwSetWindowMonitor(g_win, nullptr, g_winX, g_winY, g_winW, g_winH, 0);
  }
  g_fullscreen = !g_fullscreen;
  glfwSwapInterval(g_settings.vsync ? 1 : 0);
}

}  // namespace platform
