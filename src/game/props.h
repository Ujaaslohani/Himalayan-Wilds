#pragma once

namespace props {
void init();
void rebuildLandmarks();
void rebuildCasters();
void draw();
void drawShadow();
void drawGlow();
void drawCasters();
void checkSummits();
bool summitReached(const char* name);
void markSummit(const char* name);
void resetSummits();
void shutdown();
}
