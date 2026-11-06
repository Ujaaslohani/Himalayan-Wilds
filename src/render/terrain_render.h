#pragma once
#include "render.h"

namespace terrain_render {
void init();
void draw();
void drawGroundMesh(const Mesh& m, bool trail = false);  // trail: rocky-trail photo texture (stairs, pillars)
void shutdown();
GLuint heightTexture();
GLuint bakeTexture();
GLuint colorTexture();
const char* groundCommon();
int patchCount();
}
