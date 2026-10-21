#include "platform.h"
#include "../gfx/gl.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

Settings g_settings;

namespace {
GLFWwindow* g_win = nullptr;
Input g_in;
std::vector<std::string> g_args;
std::string g_assetDir;
std::string g_exeDir;
bool g_fullscreen = false;
int g_winX = 100, g_winY = 100, g_winW = 1600, g_winH = 900;
double g_lastX = 0, g_lastY = 0;
bool g_haveLast = false;

int mapKey(int k) {
  switch (k) {
    case GLFW_KEY_W: return K_W; case GLFW_KEY_A: return K_A; case GLFW_KEY_S: return K_S; case GLFW_KEY_D: return K_D;
    case GLFW_KEY_Q: return K_Q; case GLFW_KEY_E: return K_E; case GLFW_KEY_R: return K_R; case GLFW_KEY_F: return K_F;
    case GLFW_KEY_C: return K_C; case GLFW_KEY_J: return K_J; case GLFW_KEY_B: return K_B; case GLFW_KEY_M: return K_M;
    case GLFW_KEY_I: return K_I; case GLFW_KEY_TAB: return K_TAB; case GLFW_KEY_SPACE: return K_SPACE;
    case GLFW_KEY_LEFT_SHIFT: case GLFW_KEY_RIGHT_SHIFT: return K_SHIFT;
    case GLFW_KEY_LEFT_CONTROL: return K_CTRL;
    case GLFW_KEY_ESCAPE: return K_ESC; case GLFW_KEY_ENTER: case GLFW_KEY_KP_ENTER: return K_ENTER;
    case GLFW_KEY_UP: return K_UP; case GLFW_KEY_DOWN: return K_DOWN; case GLFW_KEY_LEFT: return K_LEFT; case GLFW_KEY_RIGHT: return K_RIGHT;
    case GLFW_KEY_1: return K_1; case GLFW_KEY_2: return K_2; case GLFW_KEY_3: return K_3;
    case GLFW_KEY_4: return K_4; case GLFW_KEY_5: return K_5; case GLFW_KEY_6: return K_6;
    case GLFW_KEY_F2: return K_F2; case GLFW_KEY_F3: return K_F3; case GLFW_KEY_F11: return K_F11;
    default: return -1;
  }
}

void onKey(GLFWwindow*, int key, int, int action, int) {
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
      else if (k == "draw_distance") s.drawDistance = fv;
      else if (k == "full_map") s.fullMap = bv;
      else if (k == "fps_limit") s.fpsLimit = iv;
      else if (k == "menu_fps") s.menuFps = iv;
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
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
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
  if (!g_win && g_settings.msaa > 0) {
    glfwWindowHint(GLFW_SAMPLES, 0);
    g_win = glfwCreateWindow(W, H, "Himalayan Wilds", mon, nullptr);
  }
  if (!g_win) { std::fprintf(stderr, "Failed to create an OpenGL 3.3 window\n"); glfwTerminate(); return false; }
  glfwMakeContextCurrent(g_win);
  if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) { std::fprintf(stderr, "Failed to load OpenGL\n"); return false; }
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
void setVsync(bool on) { g_settings.vsync = on; glfwSwapInterval(on ? 1 : 0); }

bool saveSettings() {
  const Settings& s = g_settings;
  char b[2048];
  std::snprintf(b, sizeof b,
                "; Himalayan Wilds settings (also editable from Options in game)\n[video]\nwidth = %d\nheight = %d\nfullscreen = %s\nvsync = %s\nmsaa = %d\nfxaa = %s\nshadow_size = %d\ntexture_size = %d\nadaptive_quality = %s\nadaptive_detail = %s\ntri_budget = %d\nui_scale = %.2f\nrender_scale = %.2f\ndraw_distance = %.2f\nfull_map = %s\nfps_limit = %d\nmenu_fps = %d\n\n[world]\ngrass_radius = %.0f\nundergrowth_radius = %.0f\ntree_radius = %.0f\nrock_radius = %.0f\n\n[player]\nfov = %.0f\nmouse_sensitivity = %.4f\nplayer_speed = %.2f\nsprint_multiplier = %.2f\ngravity = %.1f\n\n[audio]\nmusic_volume = %.2f\nsfx_volume = %.2f\n",
                s.width, s.height, g_fullscreen ? "true" : "false", s.vsync ? "true" : "false", s.msaa, s.fxaa ? "true" : "false", s.shadowSize, s.textureSize, s.adaptiveQuality ? "true" : "false", s.adaptiveDetail ? "true" : "false", s.triBudget, s.uiScale, s.renderScale, s.drawDistance, s.fullMap ? "true" : "false", s.fpsLimit, s.menuFps,
                s.grassRadius, s.undergrowthRadius, s.treeRadius, s.rockRadius, s.fov, s.mouseSensitivity, s.playerSpeed, s.sprintMultiplier, s.gravity, s.musicVolume, s.sfxVolume);
  return writeFile(g_exeDir + "/config.ini", b);
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
