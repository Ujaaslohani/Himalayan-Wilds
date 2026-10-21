#pragma once
#include <string>
#include <vector>
#include <cstdint>

#include "stb_truetype.h"

struct FontFace {
  std::vector<uint8_t> data;
  stbtt_fontinfo info;
  int ascent = 0, descent = 0, gap = 0;
};

namespace fonts {
bool init();
FontFace* serif();
FontFace* sans();
float measure(FontFace* f, const std::string& s, float px);
void shutdown();
}

struct Canvas {
  int w = 0, h = 0;
  std::vector<uint8_t> px;
  void init(int W, int H);
  void fillRect(int x, int y, int rw, int rh, uint32_t rgba);
  void vgradient(int x, int y, int rw, int rh, uint32_t top, uint32_t bottom);
  void strokeRect(float x, float y, float rw, float rh, float width, uint32_t rgba);
  void line(float x0, float y0, float x1, float y1, float width, uint32_t rgba);
  void bezier(float x0, float y0, float c1x, float c1y, float c2x, float c2y, float x1, float y1, float width, uint32_t rgba);
  void text(FontFace* f, const std::string& s, float px, float cx, float cy, uint32_t rgba, bool center = true);
  void blend(int x, int y, uint32_t rgba, float cover);
};
