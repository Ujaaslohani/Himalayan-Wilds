#pragma once
#include "../gfx/gfx.h"

namespace sunfx {
bool wanted();
void occlusion(GLuint depthTex, int w, int h);
void composite(int W, int H);
void shutdown();
}
