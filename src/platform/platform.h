#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct Settings {
  int width = 1600;
  int height = 900;
  bool fullscreen = false;
  bool vsync = true;
  int msaa = 0;
  bool fxaa = true;
  int shadowSize = 1536;
  float mouseSensitivity = 0.0024f;
  float playerSpeed = 4.8f;
  float sprintMultiplier = 1.75f;
  float fov = 75.0f;
  float gravity = -18.0f;
  float musicVolume = 0.22f;
  float sfxVolume = 0.6f;
  float grassRadius = 36.0f;
  float undergrowthRadius = 60.0f;
  float treeRadius = 110.0f;
  float rockRadius = 110.0f;
  int textureSize = 1024;
  bool adaptiveQuality = true;
  bool adaptiveDetail = true;
  float detailScale = 1.0f;
  int triBudget = 300000;
  float uiScale = 1.0f;
  float renderScale = 1.0f;
  float drawDistance = 1.6f;
  bool fullMap = false;
  int fpsLimit = 60;
  int menuFps = 30;
  float drawDist() const { return fullMap ? 8.0f : drawDistance; }
};

extern Settings g_settings;

enum Key {
  K_W, K_A, K_S, K_D, K_Q, K_E, K_R, K_F, K_C, K_J, K_B, K_M, K_I, K_TAB, K_SPACE, K_SHIFT, K_CTRL,
  K_ESC, K_ENTER, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_1, K_2, K_3, K_4, K_5, K_6, K_F2, K_F3, K_F4, K_F11, K_COUNT
};

struct Input {
  bool down[K_COUNT] = {};
  bool pressed[K_COUNT] = {};
  bool mouseDown[3] = {};
  bool mousePressed[3] = {};
  bool mouseReleased[3] = {};
  float mx = 0, my = 0;
  float dx = 0, dy = 0;
  float wheel = 0;
  bool captured = false;
  bool focusLost = false;
};

namespace platform {
bool init(int argc, char** argv);
void shutdown();
bool shouldClose();
void requestClose();
void pollEvents();
void swap();
int width();
int height();
double time();
float pixelRatio();
bool focused();
int refreshRate();
void setCapture(bool on);
Input& input();
void clearFrameInput();
void toggleFullscreen();
bool isFullscreen();
void setVsync(bool on);
bool saveSettings();
std::string assetPath(const std::string& rel);
std::string findAsset(const std::string& name);
std::string saveDir();
bool readFile(const std::string& path, std::vector<uint8_t>& out);
bool writeFile(const std::string& path, const std::string& data);
bool fileExists(const std::string& path);
std::vector<std::string> listFiles(const std::string& dir, const std::string& ext);
void loadSettings();
const std::vector<std::string>& args();
bool hasArg(const std::string& a);
std::string argValue(const std::string& a, const std::string& def = "");
}  // namespace platform
