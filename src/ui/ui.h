#pragma once
#include "../core/math.h"
#include <string>
#include <vector>
#include <cstdint>

struct FontFace;

namespace ui {

enum FontId { SERIF = 0, SANS = 1 };

struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
  Rect inset(float d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
};

uint32_t rgba(const char* hex, float alpha = 1);
uint32_t withAlpha(uint32_t c, float a);

bool init();
void shutdown();
void begin(int width, int height, float scale);
void end();
float scale();
void setScale(float s);
void setTextShadow(float k);
float width();
float height();

void rect(const Rect& r, uint32_t c, float radius = 0);
void border(const Rect& r, uint32_t c, float width, float radius = 0);
void circle(float cx, float cy, float r, uint32_t c);
void ring(float cx, float cy, float r, float width, uint32_t c, float startAngle = 0, float sweep = TAU);
void line(float x0, float y0, float x1, float y1, float width, uint32_t c);
void poly(const std::vector<Vec2>& pts, uint32_t c);
void tri(Vec2 a, Vec2 b, Vec2 c, uint32_t col);
void image(unsigned tex, const Rect& r, uint32_t c = 0xffffffffu, float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1);
void pushClip(const Rect& r);
void popClip();
void gradientV(const Rect& r, uint32_t top, uint32_t bottom);
void radialVignette(uint32_t edge, float inner);

float textWidth(const std::string& s, float size, FontId f = SERIF);
float text(const std::string& s, float x, float y, float size, uint32_t c, FontId f = SERIF, int align = 0, float shadow = 0);
float textShadowed(const std::string& s, float x, float y, float size, uint32_t c, FontId f = SERIF, int align = 0);
std::vector<std::string> wrap(const std::string& s, float size, float maxW, FontId f = SERIF);
float paragraph(const std::string& s, float x, float y, float maxW, float size, uint32_t c, FontId f = SERIF, float lineH = 1.3f, int align = 0);
std::string ellipsize(const std::string& s, float size, float maxW, FontId f = SERIF);

struct MouseState { float x, y; bool down, pressed, released; float wheel; };
void setMouse(const MouseState& m);
const MouseState& mouse();
bool hovered(const Rect& r);
bool clicked(const Rect& r);
bool anyHot();
void consumeClick();
bool clickConsumed();

}  // namespace ui
