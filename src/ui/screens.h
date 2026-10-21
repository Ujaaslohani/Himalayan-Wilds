#include <string>
#pragma once

struct FrameStats { float fps = 0, ms = 0; int drawCalls = 0, tris = 0; float ramMB = 0, gpuMB = 0; int instances = 0; float gpuMs = 0, cpuMs = 0, renderScale = 1; int vramExact = 0; };

namespace screens {
void init();
void draw(float dt, bool cursorFree);
void toggleStats();
bool optionsOpen();
void openOptions(bool on);
void setTitlePage(int p);
bool takeSettingsChanged();
void setStats(const FrameStats& s);
void setPassText(const std::string& gpu, const std::string& cpu);
bool statsVisible();
bool escape();
void shutdown();
}
