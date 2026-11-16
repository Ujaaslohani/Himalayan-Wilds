#include <cstdlib>
#include <cstdio>
#include "models.h"
#include "fp_hands_data.h"
#include "../platform/platform.h"
#include <cstring>
#include <memory>
#include <functional>

namespace {

using V = Vec3;
const V Z0{0, 0, 0};

void eyes(Shape& s, const char* color, float x, float y, float z, float r) {
  z += r * 0.3f;
  s.ball("#0c0a09", {x, y, z}, {r * 1.25f, r * 1.1f, r * 0.7f}, Z0, 0)
      .ball("#0c0a09", {-x, y, z}, {r * 1.25f, r * 1.1f, r * 0.7f}, Z0, 0)
      .ball(color, {x, y, z + r * 0.35f}, {r, r * 0.9f, r * 0.5f}, Z0, 0)
      .ball(color, {-x, y, z + r * 0.35f}, {r, r * 0.9f, r * 0.5f}, Z0, 0)
      .ball("#050404", {x, y, z + r * 0.6f}, {r * 0.35f, r * 0.75f, r * 0.3f}, Z0, 0)
      .ball("#050404", {-x, y, z + r * 0.6f}, {r * 0.35f, r * 0.75f, r * 0.3f}, Z0, 0);
}

void hoof(Shape& s, const char* color, float y, float z = 0.02f) {
  s.box(color, {0.022f, y, z}, {0.034f, 0.05f, 0.07f}).box(color, {-0.022f, y, z}, {0.034f, 0.05f, 0.07f});
}

struct SpeciesBuild {
  Shape body, head, leg;
  V headPivot;
  V legs[4];
  HitSphere hb;
  HeadHit hh;
  float shadow;
};

SpeciesBuild wolf() {
  SpeciesBuild m;
  const char *g1 = "#857f76", *g2 = "#57524b", *g3 = "#e2dccf", *g4 = "#2f2c28", *tan = "#a88c66";
  m.body.ball(g1, {0, 0.74f, -0.02f}, {0.23f, 0.25f, 0.52f})
      .ball(g1, {0, 0.76f, 0.3f}, {0.25f, 0.29f, 0.28f})
      .ball(g3, {0, 0.66f, 0.36f}, {0.17f, 0.22f, 0.2f})
      .ball(g1, {0, 0.88f, 0.42f}, {0.21f, 0.22f, 0.19f})
      .ball(g1, {0, 0.75f, -0.36f}, {0.21f, 0.24f, 0.24f})
      .ball(g3, {0, 0.6f, 0.0f}, {0.16f, 0.11f, 0.42f})
      .ball(g2, {0, 0.92f, 0.0f}, {0.15f, 0.07f, 0.45f})
      .ball(tan, {0.16f, 0.7f, 0.05f}, {0.07f, 0.12f, 0.3f})
      .ball(tan, {-0.16f, 0.7f, 0.05f}, {0.07f, 0.12f, 0.3f})
      .rod(g1, {0, 0.82f, -0.55f}, {0, 0.6f, -0.86f}, 0.07f, 0.085f, 7)
      .ball(g1, {0, 0.55f, -0.93f}, {0.09f, 0.09f, 0.16f})
      .ball(g4, {0, 0.47f, -1.05f}, {0.06f, 0.07f, 0.09f});
  m.head.rod(g1, {0, -0.05f, -0.05f}, {0, 0.1f, 0.12f}, 0.15f, 0.13f, 8)
      .ball(g1, {0, 0.12f, 0.17f}, {0.15f, 0.135f, 0.16f})
      .ball(g3, {0.1f, 0.06f, 0.2f}, {0.07f, 0.08f, 0.1f})
      .ball(g3, {-0.1f, 0.06f, 0.2f}, {0.07f, 0.08f, 0.1f})
      .rod(g1, {0, 0.1f, 0.26f}, {0, 0.06f, 0.47f}, 0.075f, 0.05f, 7)
      .rod(g3, {0, 0.03f, 0.26f}, {0, 0.0f, 0.45f}, 0.06f, 0.04f, 7)
      .ball("#1a1715", {0, 0.075f, 0.48f}, {0.035f, 0.03f, 0.03f}, Z0, 0)
      .box("#1a1715", {0, -0.005f, 0.4f}, {0.07f, 0.012f, 0.1f})
      .cone(g1, {0.085f, 0.29f, 0.1f}, 0.055f, 0.15f, 4, {-0.15f, 0, -0.2f})
      .cone(g1, {-0.085f, 0.29f, 0.1f}, 0.055f, 0.15f, 4, {-0.15f, 0, 0.2f})
      .cone(g4, {0.085f, 0.3f, 0.115f}, 0.03f, 0.09f, 4, {-0.15f, 0, -0.2f})
      .cone(g4, {-0.085f, 0.3f, 0.115f}, 0.03f, 0.09f, 4, {-0.15f, 0, 0.2f})
      .box(g4, {0, 0.18f, 0.27f}, {0.1f, 0.012f, 0.05f}, {0.4f, 0, 0});
  eyes(m.head, "#e0a526", 0.065f, 0.165f, 0.28f, 0.022f);
  m.leg.rod(g1, {0, 0.04f, 0}, {0, -0.28f, 0.02f}, 0.065f, 0.045f, 6)
      .rod(tan, {0, -0.28f, 0.02f}, {0, -0.56f, 0}, 0.04f, 0.032f, 6)
      .ball(g4, {0, -0.6f, 0.04f}, {0.045f, 0.03f, 0.065f});
  m.headPivot = {0, 0.88f, 0.55f};
  V l[4] = {{0.12f, 0.62f, 0.32f}, {-0.12f, 0.62f, 0.32f}, {0.12f, 0.64f, -0.36f}, {-0.12f, 0.64f, -0.36f}};
  for (int i = 0; i < 4; i++) m.legs[i] = l[i];
  m.hb = {0.74f, 0.36f, 0.3f};
  m.hh = {0.12f, 0.25f, 0.2f};
  m.shadow = 0.7f;
  return m;
}

SpeciesBuild bear() {
  SpeciesBuild m;
  const char *b1 = "#1d1a18", *b2 = "#2b2622", *b3 = "#35302b", *cream = "#e9dcc0", *muz = "#8a6b4c", *claw = "#d9cfbd";
  m.body.ball(b1, {0, 0.98f, -0.06f}, {0.62f, 0.56f, 0.82f})
      .ball(b2, {0, 1.1f, 0.42f}, {0.58f, 0.6f, 0.46f})
      .ball(b1, {0, 1.0f, -0.58f}, {0.56f, 0.54f, 0.46f})
      .ball(b3, {0, 1.36f, 0.22f}, {0.42f, 0.26f, 0.42f})
      .ball(b1, {0, 0.72f, -0.04f}, {0.5f, 0.32f, 0.66f})
      .ball(b2, {0.42f, 1.02f, 0.52f}, {0.22f, 0.32f, 0.24f})
      .ball(b2, {-0.42f, 1.02f, 0.52f}, {0.22f, 0.32f, 0.24f})
      .ball(b1, {0.4f, 1.0f, -0.58f}, {0.24f, 0.34f, 0.28f})
      .ball(b1, {-0.4f, 1.0f, -0.58f}, {0.24f, 0.34f, 0.28f})
      .ball(b3, {0, 1.12f, 0.78f}, {0.42f, 0.42f, 0.22f})
      .ball(b1, {0, 0.86f, -1.02f}, {0.12f, 0.12f, 0.1f})
      .box(cream, {0.17f, 1.12f, 0.98f}, {0.08f, 0.4f, 0.04f}, {0.15f, 0, 0.75f})
      .box(cream, {-0.17f, 1.12f, 0.98f}, {0.08f, 0.4f, 0.04f}, {0.15f, 0, -0.75f})
      .ball(cream, {0, 0.98f, 0.99f}, {0.07f, 0.06f, 0.03f}, Z0, 0);
  m.head.rod(b3, {0, -0.1f, -0.12f}, {0, 0.04f, 0.12f}, 0.34f, 0.3f, 10)
      .ball(b1, {0, 0.06f, 0.22f}, {0.31f, 0.27f, 0.27f})
      .ball(b2, {0.25f, 0.04f, 0.16f}, {0.13f, 0.17f, 0.13f})
      .ball(b2, {-0.25f, 0.04f, 0.16f}, {0.13f, 0.17f, 0.13f})
      .rod(muz, {0, 0.02f, 0.4f}, {0, -0.02f, 0.62f}, 0.12f, 0.09f, 8)
      .ball(muz, {0, -0.06f, 0.5f}, {0.1f, 0.06f, 0.1f})
      .ball("#100d0c", {0, 0.03f, 0.64f}, {0.065f, 0.048f, 0.04f}, Z0, 0)
      .box("#100d0c", {0, -0.09f, 0.57f}, {0.1f, 0.015f, 0.06f})
      .ball(b1, {0.2f, 0.32f, 0.12f}, {0.095f, 0.095f, 0.05f})
      .ball(b1, {-0.2f, 0.32f, 0.12f}, {0.095f, 0.095f, 0.05f})
      .ball("#4a3a30", {0.2f, 0.32f, 0.15f}, {0.05f, 0.05f, 0.02f}, Z0, 0)
      .ball("#4a3a30", {-0.2f, 0.32f, 0.15f}, {0.05f, 0.05f, 0.02f}, Z0, 0);
  eyes(m.head, "#3b2414", 0.11f, 0.15f, 0.44f, 0.022f);
  m.leg.rod(b1, {0, 0.1f, 0}, {0, -0.32f, 0.03f}, 0.17f, 0.13f, 8)
      .rod(b2, {0, -0.32f, 0.03f}, {0, -0.68f, 0.02f}, 0.13f, 0.12f, 8)
      .ball(b1, {0, -0.72f, 0.07f}, {0.14f, 0.07f, 0.17f})
      .cone(claw, {0.07f, -0.75f, 0.23f}, 0.014f, 0.06f, 4, {PI / 2, 0, 0})
      .cone(claw, {0.023f, -0.75f, 0.24f}, 0.014f, 0.06f, 4, {PI / 2, 0, 0})
      .cone(claw, {-0.023f, -0.75f, 0.24f}, 0.014f, 0.06f, 4, {PI / 2, 0, 0})
      .cone(claw, {-0.07f, -0.75f, 0.23f}, 0.014f, 0.06f, 4, {PI / 2, 0, 0});
  m.headPivot = {0, 1.2f, 0.92f};
  V l[4] = {{0.32f, 0.76f, 0.5f}, {-0.32f, 0.76f, 0.5f}, {0.32f, 0.76f, -0.6f}, {-0.32f, 0.76f, -0.6f}};
  for (int i = 0; i < 4; i++) m.legs[i] = l[i];
  m.hb = {1.0f, 0.72f, 0.45f};
  m.hh = {0.08f, 0.32f, 0.32f};
  m.shadow = 1.35f;
  return m;
}

SpeciesBuild deer() {
  SpeciesBuild m;
  const char *c1 = "#a8622f", *c2 = "#7c4420", *c3 = "#efe2c6", *c4 = "#faf6ee", *dk = "#2a1f1a";
  m.body.ball(c1, {0, 0.88f, 0.02f}, {0.27f, 0.28f, 0.56f})
      .ball(c1, {0, 0.92f, 0.36f}, {0.26f, 0.31f, 0.3f})
      .ball(c1, {0, 0.93f, -0.38f}, {0.27f, 0.29f, 0.3f})
      .ball(c2, {0.18f, 0.88f, 0.4f}, {0.1f, 0.2f, 0.17f})
      .ball(c2, {-0.18f, 0.88f, 0.4f}, {0.1f, 0.2f, 0.17f})
      .ball(c1, {0.17f, 0.9f, -0.44f}, {0.12f, 0.22f, 0.2f})
      .ball(c1, {-0.17f, 0.9f, -0.44f}, {0.12f, 0.22f, 0.2f})
      .ball(c3, {0, 0.72f, 0.02f}, {0.2f, 0.13f, 0.46f})
      .ball(c4, {0, 0.94f, -0.66f}, {0.15f, 0.17f, 0.06f})
      .ball(c2, {0, 1.15f, 0.0f}, {0.07f, 0.03f, 0.5f})
      .rod(c1, {0, 1.02f, -0.64f}, {0, 0.88f, -0.74f}, 0.05f, 0.03f, 5)
      .ball(c4, {0, 0.9f, -0.74f}, {0.04f, 0.07f, 0.03f}, Z0, 0);
  for (int i = 0; i < 8; i++) m.body.ball(c3, {i % 2 ? 0.15f : -0.15f, 1.07f - (i % 3) * 0.04f, -0.34f + i * 0.09f}, {0.035f, 0.018f, 0.035f}, Z0, 0);
  m.head.rod(c1, {0, -0.04f, -0.02f}, {0, 0.42f, 0.18f}, 0.14f, 0.085f, 8)
      .ball(c3, {0, 0.22f, 0.17f}, {0.07f, 0.11f, 0.065f})
      .ball(c1, {0, 0.5f, 0.29f}, {0.12f, 0.12f, 0.17f})
      .rod(c1, {0, 0.49f, 0.38f}, {0, 0.44f, 0.55f}, 0.085f, 0.06f, 7)
      .ball(dk, {0, 0.45f, 0.57f}, {0.045f, 0.035f, 0.03f}, Z0, 0)
      .ball(c4, {0, 0.41f, 0.52f}, {0.05f, 0.025f, 0.04f}, Z0, 0)
      .ball(c4, {0.085f, 0.545f, 0.39f}, {0.032f, 0.032f, 0.02f}, Z0, 0)
      .ball(c4, {-0.085f, 0.545f, 0.39f}, {0.032f, 0.032f, 0.02f}, Z0, 0)
      .cone(c1, {0.14f, 0.62f, 0.22f}, 0.06f, 0.22f, 5, {-0.3f, 0, -1.0f}, {1, 1, 0.45f})
      .cone(c1, {-0.14f, 0.62f, 0.22f}, 0.06f, 0.22f, 5, {-0.3f, 0, 1.0f}, {1, 1, 0.45f})
      .cone("#e8c9b4", {0.142f, 0.62f, 0.235f}, 0.035f, 0.16f, 5, {-0.3f, 0, -1.0f}, {1, 1, 0.3f})
      .cone("#e8c9b4", {-0.142f, 0.62f, 0.235f}, 0.035f, 0.16f, 5, {-0.3f, 0, 1.0f}, {1, 1, 0.3f});
  eyes(m.head, "#2a160b", 0.095f, 0.555f, 0.38f, 0.02f);
  const char* ant = "#e6d5b8";
  for (float sx : {1.0f, -1.0f}) {
    m.head.rod(ant, {sx * 0.05f, 0.6f, 0.27f}, {sx * 0.15f, 0.86f, 0.18f}, 0.022f, 0.016f, 5)
        .rod(ant, {sx * 0.15f, 0.86f, 0.18f}, {sx * 0.2f, 1.04f, 0.24f}, 0.016f, 0.01f, 5)
        .rod(ant, {sx * 0.1f, 0.74f, 0.22f}, {sx * 0.08f, 0.86f, 0.33f}, 0.014f, 0.008f, 4)
        .rod(ant, {sx * 0.16f, 0.9f, 0.19f}, {sx * 0.26f, 0.98f, 0.12f}, 0.012f, 0.007f, 4)
        .rod(ant, {sx * 0.19f, 0.98f, 0.22f}, {sx * 0.15f, 1.08f, 0.3f}, 0.01f, 0.006f, 4);
  }
  m.leg.rod(c1, {0, 0.06f, 0}, {0, -0.3f, 0.03f}, 0.075f, 0.05f, 6)
      .ball(c2, {0, -0.31f, 0.03f}, {0.05f, 0.05f, 0.05f}, Z0, 0)
      .rod(c2, {0, -0.31f, 0.03f}, {0, -0.62f, 0}, 0.04f, 0.03f, 6)
      .rod(c2, {0, -0.62f, 0}, {0, -0.68f, 0.02f}, 0.032f, 0.03f, 5);
  hoof(m.leg, dk, -0.7f);
  m.headPivot = {0, 1.0f, 0.5f};
  V l[4] = {{0.13f, 0.72f, 0.38f}, {-0.13f, 0.72f, 0.38f}, {0.14f, 0.74f, -0.42f}, {-0.14f, 0.74f, -0.42f}};
  for (int i = 0; i < 4; i++) m.legs[i] = l[i];
  m.hb = {0.86f, 0.5f, 0.32f};
  m.hh = {0.5f, 0.32f, 0.22f};
  m.shadow = 0.75f;
  return m;
}

SpeciesBuild boar() {
  SpeciesBuild m;
  const char *c1 = "#4b3a2c", *c2 = "#2c221b", *c3 = "#5d4a39", *pink = "#c4958a", *dk = "#151110";
  m.body.ball(c1, {0, 0.58f, -0.08f}, {0.36f, 0.36f, 0.62f})
      .ball(c2, {0, 0.7f, 0.26f}, {0.37f, 0.41f, 0.4f})
      .ball(c2, {0.22f, 0.6f, 0.32f}, {0.14f, 0.22f, 0.2f})
      .ball(c2, {-0.22f, 0.6f, 0.32f}, {0.14f, 0.22f, 0.2f})
      .ball(c1, {0.2f, 0.58f, -0.42f}, {0.14f, 0.22f, 0.2f})
      .ball(c1, {-0.2f, 0.58f, -0.42f}, {0.14f, 0.22f, 0.2f})
      .ball(c3, {0, 0.4f, 0}, {0.27f, 0.15f, 0.5f})
      .rod(c2, {0, 0.64f, -0.68f}, {0, 0.42f, -0.76f}, 0.028f, 0.012f, 4)
      .ball(dk, {0, 0.4f, -0.78f}, {0.04f, 0.07f, 0.04f}, Z0, 0);
  for (int i = 0; i < 11; i++) {
    float z = -0.45f + i * 0.09f;
    float y = 0.92f + std::sin((i / 10.0f) * PI) * 0.12f + (i > 6 ? 0.04f : 0);
    m.body.cone(dk, {0, y, z}, 0.05f, 0.16f + (i > 5 ? 0.06f : 0), 4, {-0.35f, 0, 0}, {0.6f, 1, 1});
  }
  m.head.ball(c2, {0, -0.02f, 0.14f}, {0.24f, 0.25f, 0.28f})
      .rod(c1, {0, -0.05f, 0.28f}, {0, -0.12f, 0.52f}, 0.14f, 0.1f, 8)
      .tube(pink, {0, -0.125f, 0.54f}, 0.105f, 0.105f, 0.05f, 10, {PI / 2, 0, 0})
      .ball("#2a1a16", {0.035f, -0.12f, 0.568f}, {0.018f, 0.022f, 0.01f}, Z0, 0)
      .ball("#2a1a16", {-0.035f, -0.12f, 0.568f}, {0.018f, 0.022f, 0.01f}, Z0, 0)
      .rod("#f4ecd8", {0.09f, -0.16f, 0.42f}, {0.13f, -0.05f, 0.48f}, 0.022f, 0.008f, 5)
      .rod("#f4ecd8", {-0.09f, -0.16f, 0.42f}, {-0.13f, -0.05f, 0.48f}, 0.022f, 0.008f, 5)
      .ball(c3, {0.12f, -0.08f, 0.2f}, {0.09f, 0.11f, 0.12f})
      .ball(c3, {-0.12f, -0.08f, 0.2f}, {0.09f, 0.11f, 0.12f})
      .cone(c2, {0.15f, 0.2f, 0.05f}, 0.075f, 0.17f, 4, {0.2f, 0, -0.55f}, {1, 1, 0.5f})
      .cone(c2, {-0.15f, 0.2f, 0.05f}, 0.075f, 0.17f, 4, {0.2f, 0, 0.55f}, {1, 1, 0.5f})
      .cone(dk, {0, 0.22f, 0.0f}, 0.05f, 0.16f, 4, {-0.4f, 0, 0}, {0.6f, 1, 1});
  eyes(m.head, "#2a160b", 0.135f, 0.07f, 0.29f, 0.016f);
  m.leg.rod(c2, {0, 0.04f, 0}, {0, -0.22f, 0.02f}, 0.085f, 0.06f, 6).rod(c2, {0, -0.22f, 0.02f}, {0, -0.38f, 0}, 0.055f, 0.045f, 6);
  hoof(m.leg, dk, -0.41f);
  m.headPivot = {0, 0.62f, 0.6f};
  V l[4] = {{0.17f, 0.44f, 0.34f}, {-0.17f, 0.44f, 0.34f}, {0.17f, 0.44f, -0.38f}, {-0.17f, 0.44f, -0.38f}};
  for (int i = 0; i < 4; i++) m.legs[i] = l[i];
  m.hb = {0.58f, 0.5f, 0.3f};
  m.hh = {0, 0.25f, 0.25f};
  m.shadow = 0.85f;
  return m;
}

SpeciesBuild goral() {
  SpeciesBuild m;
  const char *c1 = "#8a7a66", *c7 = "#7a6a58", *c2 = "#4a3f35", *c8 = "#ece4d4", *dk = "#1d1814";
  m.body.ball(c1, {0, 0.72f, 0}, {0.28f, 0.31f, 0.48f})
      .ball(c7, {0, 0.76f, 0.28f}, {0.27f, 0.32f, 0.27f})
      .ball(c7, {0.17f, 0.72f, 0.32f}, {0.1f, 0.19f, 0.15f})
      .ball(c7, {-0.17f, 0.72f, 0.32f}, {0.1f, 0.19f, 0.15f})
      .ball(c1, {0.16f, 0.74f, -0.34f}, {0.11f, 0.2f, 0.17f})
      .ball(c1, {-0.16f, 0.74f, -0.34f}, {0.11f, 0.2f, 0.17f})
      .ball("#3a3029", {0, 1.02f, 0}, {0.06f, 0.04f, 0.44f})
      .ball("#b8a990", {0, 0.58f, 0}, {0.2f, 0.14f, 0.38f})
      .ball(c2, {0, 0.86f, -0.5f}, {0.05f, 0.09f, 0.05f}, Z0, 0)
      .ball(c8, {0, 0.66f, 0.5f}, {0.13f, 0.14f, 0.08f});
  m.head.rod(c7, {0, -0.02f, 0}, {0, 0.22f, 0.12f}, 0.13f, 0.095f, 7)
      .ball(c8, {0, 0.13f, 0.15f}, {0.09f, 0.09f, 0.06f})
      .ball(c1, {0, 0.31f, 0.22f}, {0.11f, 0.12f, 0.16f})
      .rod(c2, {0, 0.3f, 0.32f}, {0, 0.25f, 0.4f}, 0.07f, 0.055f, 6)
      .ball(dk, {0, 0.26f, 0.42f}, {0.04f, 0.03f, 0.025f}, Z0, 0)
      .ball(c8, {0, 0.22f, 0.38f}, {0.045f, 0.03f, 0.04f}, Z0, 0)
      .cone(c1, {0.12f, 0.38f, 0.15f}, 0.038f, 0.14f, 4, {0, 0, -1.25f}, {1, 1, 0.5f})
      .cone(c1, {-0.12f, 0.38f, 0.15f}, 0.038f, 0.14f, 4, {0, 0, 1.25f}, {1, 1, 0.5f});
  for (float sx : {1.0f, -1.0f}) {
    m.head.rod("#2a2420", {sx * 0.04f, 0.4f, 0.2f}, {sx * 0.06f, 0.5f, 0.12f}, 0.026f, 0.02f, 5)
        .rod("#2a2420", {sx * 0.06f, 0.5f, 0.12f}, {sx * 0.07f, 0.53f, 0.02f}, 0.02f, 0.007f, 5);
  }
  eyes(m.head, "#3b2a12", 0.085f, 0.34f, 0.31f, 0.019f);
  m.leg.rod(c7, {0, 0.04f, 0}, {0, -0.27f, 0.02f}, 0.075f, 0.05f, 6).rod(c2, {0, -0.27f, 0.02f}, {0, -0.55f, 0}, 0.045f, 0.035f, 6);
  hoof(m.leg, dk, -0.58f);
  m.headPivot = {0, 0.88f, 0.38f};
  V l[4] = {{0.12f, 0.6f, 0.26f}, {-0.12f, 0.6f, 0.26f}, {0.12f, 0.62f, -0.3f}, {-0.12f, 0.62f, -0.3f}};
  for (int i = 0; i < 4; i++) m.legs[i] = l[i];
  m.hb = {0.72f, 0.42f, 0.22f};
  m.hh = {0.3f, 0.22f, 0.18f};
  m.shadow = 0.62f;
  return m;
}

SpeciesBuild leopard() {
  SpeciesBuild m;
  const char *y1 = "#c9984c", *y2 = "#a9783a", *w = "#efe4cc", *sp = "#2a1d12";
  m.body.ball(y1, {0, 0.62f, 0}, {0.2f, 0.22f, 0.58f})
      .ball(y1, {0, 0.65f, 0.34f}, {0.21f, 0.25f, 0.26f})
      .ball(y1, {0, 0.66f, -0.36f}, {0.2f, 0.23f, 0.24f})
      .ball(w, {0, 0.5f, 0.05f}, {0.15f, 0.1f, 0.46f})
      .ball(w, {0, 0.58f, 0.48f}, {0.14f, 0.17f, 0.12f})
      .ball(y2, {0, 0.82f, 0}, {0.12f, 0.05f, 0.48f});
  V tail[4] = {{0, 0.68f, -0.56f}, {0, 0.56f, -0.84f}, {0, 0.44f, -1.04f}, {0, 0.48f, -1.2f}};
  for (int i = 0; i < 3; i++) m.body.rod(y1, tail[i], tail[i + 1], 0.055f - i * 0.008f, 0.05f - i * 0.008f, 6);
  m.body.ball(sp, {0, 0.49f, -1.24f}, {0.05f, 0.05f, 0.07f});
  for (int i = 0; i < 14; i++) {
    float z = -0.5f + (i % 7) * 0.16f;
    float side = i < 7 ? 1.0f : -1.0f;
    float yy = 0.66f + std::sin(i * 2.3f) * 0.08f;
    float x = side * (0.17f + std::cos(i * 1.7f) * 0.02f);
    m.body.ball(sp, {x, yy, z}, {0.012f, 0.035f, 0.035f}, Z0, 0);
    m.body.ball(sp, {x * 0.92f, yy + 0.06f, z + 0.05f}, {0.012f, 0.03f, 0.03f}, Z0, 0);
    m.body.ball(sp, {x * 0.97f, yy - 0.06f, z + 0.04f}, {0.012f, 0.028f, 0.028f}, Z0, 0);
  }
  for (int i = 0; i < 6; i++) m.body.ball(sp, {i % 2 ? 0.07f : -0.07f, 0.82f, -0.4f + i * 0.16f}, {0.03f, 0.012f, 0.035f}, Z0, 0);
  for (int i = 0; i < 3; i++) m.body.tube(sp, {0, 0.62f - i * 0.06f, -0.72f - i * 0.14f}, 0.058f, 0.058f, 0.03f, 6, {0.75f, 0, 0});
  m.head.rod(y1, {0, -0.05f, -0.06f}, {0, 0.06f, 0.1f}, 0.13f, 0.11f, 8)
      .ball(y1, {0, 0.09f, 0.16f}, {0.135f, 0.12f, 0.13f})
      .ball(w, {0.06f, 0.03f, 0.25f}, {0.06f, 0.05f, 0.06f})
      .ball(w, {-0.06f, 0.03f, 0.25f}, {0.06f, 0.05f, 0.06f})
      .ball(w, {0, -0.02f, 0.22f}, {0.07f, 0.04f, 0.07f})
      .ball("#b06b5a", {0, 0.07f, 0.3f}, {0.03f, 0.022f, 0.02f}, Z0, 0)
      .ball(y1, {0.09f, 0.2f, 0.1f}, {0.045f, 0.05f, 0.025f})
      .ball(y1, {-0.09f, 0.2f, 0.1f}, {0.045f, 0.05f, 0.025f})
      .ball(sp, {0.09f, 0.2f, 0.08f}, {0.04f, 0.045f, 0.012f}, Z0, 0)
      .ball(sp, {-0.09f, 0.2f, 0.08f}, {0.04f, 0.045f, 0.012f}, Z0, 0)
      .ball(sp, {0.05f, 0.16f, 0.23f}, {0.01f, 0.01f, 0.01f}, Z0, 0)
      .ball(sp, {-0.05f, 0.16f, 0.23f}, {0.01f, 0.01f, 0.01f}, Z0, 0)
      .ball(sp, {0.1f, 0.1f, 0.2f}, {0.012f, 0.012f, 0.012f}, Z0, 0)
      .ball(sp, {-0.1f, 0.1f, 0.2f}, {0.012f, 0.012f, 0.012f}, Z0, 0);
  eyes(m.head, "#d9b23a", 0.058f, 0.125f, 0.272f, 0.024f);
  m.leg.rod(y1, {0, 0.04f, 0}, {0, -0.26f, 0.02f}, 0.065f, 0.045f, 6)
      .rod(y1, {0, -0.26f, 0.02f}, {0, -0.48f, 0}, 0.045f, 0.04f, 6)
      .ball(w, {0, -0.52f, 0.04f}, {0.055f, 0.035f, 0.07f})
      .ball(sp, {0.04f, -0.15f, 0.04f}, {0.012f, 0.025f, 0.025f}, Z0, 0);
  m.headPivot = {0, 0.76f, 0.56f};
  V l[4] = {{0.11f, 0.54f, 0.34f}, {-0.11f, 0.54f, 0.34f}, {0.11f, 0.56f, -0.36f}, {-0.11f, 0.56f, -0.36f}};
  for (int i = 0; i < 4; i++) m.legs[i] = l[i];
  m.hb = {0.62f, 0.32f, 0.3f};
  m.hh = {0.1f, 0.18f, 0.18f};
  m.shadow = 0.7f;
  return m;
}

const char *GREY1 = "#8b8478", *GREY2 = "#6b665d", *WOOD1 = "#7a4b2a", *WOOD2 = "#5a3820", *ROOF1 = "#4b5563", *ROOF2 = "#374151", *WALL = "#e8dcc4", *DOOR = "#3b2416", *WINDOW = "#1f160f";

float house(Shape& b, float w, float d, int courses, float ox = 0, float oz = 0) {
  b.box(GREY1, {ox, 0.25f, oz}, {w + 0.4f, 0.5f, d + 0.4f});
  float y = 0.5f;
  for (int i = 0; i < courses; i++) {
    b.box(WOOD1, {ox, y + 0.09f, oz}, {w + 0.08f, 0.18f, d + 0.08f});
    b.box(i % 2 ? GREY2 : GREY1, {ox, y + 0.34f, oz}, {w - 0.06f, 0.32f, d - 0.06f});
    y += 0.5f;
  }
  for (int sx = -1; sx <= 1; sx += 2)
    for (int sz = -1; sz <= 1; sz += 2) b.box(WOOD2, {ox + sx * (w / 2), 0.5f + (courses * 0.5f) / 2, oz + sz * (d / 2)}, {0.14f, courses * 0.5f, 0.14f});
  b.box(DOOR, {ox, 1.15f, oz + d / 2 + 0.03f}, {0.85f, 1.3f, 0.06f});
  b.box(WOOD2, {ox, 1.86f, oz + d / 2 + 0.05f}, {1.05f, 0.12f, 0.08f});
  for (int sx = -1; sx <= 1; sx += 2) {
    b.box(WINDOW, {ox + sx * w * 0.3f, 1.65f, oz + d / 2 + 0.03f}, {0.45f, 0.42f, 0.05f});
    b.box(WOOD1, {ox + sx * w * 0.3f, 1.65f, oz + d / 2 + 0.06f}, {0.55f, 0.06f, 0.04f});
  }
  float top = 0.5f + courses * 0.5f;
  b.box(WOOD1, {ox, top + 0.12f, oz}, {w + 0.9f, 0.24f, d + 0.9f});
  b.box(WALL, {ox, top + 0.52f, oz}, {w + 0.5f, 0.56f, d + 0.5f});
  b.box(WOOD2, {ox, top + 0.82f, oz}, {w + 0.6f, 0.06f, d + 0.6f});
  for (int i = -3; i <= 3; i++) b.box(WOOD2, {ox + (i * (w + 0.8f)) / 7, top + 0.52f, oz + (d + 0.9f) / 2}, {0.06f, 0.56f, 0.06f});
  float roofY = top + 0.85f;
  float r = std::max(w, d) * 0.92f + 0.6f;
  b.cone(ROOF1, {ox, roofY + 0.75f, oz}, r, 1.5f, 4, {0, PI / 4, 0}, {1, 1, d / w});
  b.cone(ROOF2, {ox, roofY + 0.06f, oz}, r + 0.08f, 0.12f, 4, {0, PI / 4, 0}, {1, 1, d / w});
  b.rod(WOOD2, {ox, roofY + 1.35f, oz}, {ox, roofY + 1.9f, oz}, 0.05f, 0.02f, 5);
  return roofY + 1.5f;
}

void stack(Shape& b, float x, float z) {
  for (int row = 0; row < 3; row++)
    for (int i = 0; i < 3 - row; i++) {
      float px = x + (i - (2 - row) / 2.0f) * 0.34f;
      b.rod("#6b4a32", {px, 0.17f + row * 0.29f, z - 0.8f}, {px, 0.17f + row * 0.29f, z + 0.8f}, 0.16f, 0.16f, 7);
      b.tube("#d9b98c", {px, 0.17f + row * 0.29f, z + 0.81f}, 0.14f, 0.14f, 0.02f, 7, {PI / 2, 0, 0});
    }
}

Shape buildingShape(const std::string& type) {
  Shape b;
  if (type == "campfire") {
    for (int i = 0; i < 9; i++) {
      float a = (i / 9.0f) * TAU;
      b.add(dodecaGeo(0.2f), i % 2 ? GREY1 : GREY2, {std::cos(a) * 0.85f, 0.12f, std::sin(a) * 0.85f}, {a, a * 2, 0}, {1, 0.7f, 1.1f});
    }
    b.tube("#2b2522", {0, 0.02f, 0}, 0.75f, 0.75f, 0.04f, 10);
    b.rod("#5a3820", {-0.5f, 0.1f, -0.2f}, {0.45f, 0.32f, 0.2f}, 0.07f, 0.06f);
    b.rod("#6b4a32", {0.5f, 0.1f, -0.25f}, {-0.4f, 0.34f, 0.25f}, 0.07f, 0.06f);
    b.rod("#5a3820", {0, 0.1f, 0.55f}, {0, 0.36f, -0.45f}, 0.07f, 0.06f);
    for (float a : {0.6f, 2.2f, 3.9f}) {
      float cx = std::cos(a) * 2.6f, cz = std::sin(a) * 2.6f;
      float tx = -std::sin(a) * 0.8f, tz = std::cos(a) * 0.8f;
      b.rod("#6b4a32", {cx - tx, 0.22f, cz - tz}, {cx + tx, 0.22f, cz + tz}, 0.2f, 0.2f, 7);
    }
  } else if (type == "hunter_hut") {
    house(b, 3.6f, 3.2f, 3);
    b.rod(WOOD2, {2.7f, 0, 0.6f}, {2.7f, 1.8f, 0.6f}, 0.05f, 0.05f).rod(WOOD2, {2.7f, 0, -0.9f}, {2.7f, 1.8f, -0.9f}, 0.05f, 0.05f);
    b.rod(WOOD1, {2.7f, 1.75f, 0.7f}, {2.7f, 1.75f, -1.0f}, 0.04f, 0.04f);
    b.box("#a8622f", {2.72f, 1.3f, -0.15f}, {0.04f, 0.8f, 0.9f});
    b.tube("#f5f0e6", {-2.9f, 1.0f, 1.4f}, 0.45f, 0.45f, 0.1f, 12, {PI / 2, 0, 0});
    b.tube("#dc2626", {-2.9f, 1.0f, 1.46f}, 0.3f, 0.3f, 0.02f, 12, {PI / 2, 0, 0});
    b.tube("#f5f0e6", {-2.9f, 1.0f, 1.475f}, 0.16f, 0.16f, 0.02f, 12, {PI / 2, 0, 0});
    b.rod(WOOD2, {-2.9f, 0, 1.3f}, {-2.9f, 0.65f, 1.38f}, 0.04f, 0.04f);
  } else if (type == "carpenter_workshop") {
    house(b, 3.4f, 3.0f, 3, -0.8f, 0);
    float posts[4][2] = {{1.4f, 1.6f}, {3.4f, 1.6f}, {1.4f, -1.4f}, {3.4f, -1.4f}};
    for (auto& p : posts) b.rod(WOOD2, {p[0], 0, p[1]}, {p[0], 2.3f, p[1]}, 0.08f, 0.08f);
    b.box(ROOF1, {2.4f, 2.45f, 0.1f}, {2.6f, 0.12f, 3.6f}, {0.18f, 0, 0});
    stack(b, 2.4f, 0.1f);
    b.box(WOOD1, {2.4f, 0.7f, 2.6f}, {1.4f, 0.1f, 0.4f});
    b.rod(WOOD2, {1.9f, 0, 2.6f}, {1.9f, 0.65f, 2.6f}, 0.04f, 0.04f).rod(WOOD2, {2.9f, 0, 2.6f}, {2.9f, 0.65f, 2.6f}, 0.04f, 0.04f);
  } else if (type == "herbalist_lodge") {
    house(b, 3.2f, 3.0f, 3);
    for (float x : {-2.6f, 2.6f}) {
      b.rod(WOOD2, {x, 0, 1.2f}, {x, 1.9f, 1.2f}, 0.05f, 0.05f).rod(WOOD2, {x, 0, -1.2f}, {x, 1.9f, -1.2f}, 0.05f, 0.05f);
      b.rod(WOOD1, {x, 1.85f, 1.3f}, {x, 1.85f, -1.3f}, 0.035f, 0.035f);
      for (int i = -2; i <= 2; i++) b.cone(i % 2 ? "#4d7c2f" : "#65a30d", {x, 1.55f, i * 0.45f}, 0.12f, 0.5f, 5, {PI, 0, 0});
    }
    for (float x : {-0.9f, 0.9f}) {
      b.tube("#9a3412", {x, 0.2f, 2.2f}, 0.18f, 0.13f, 0.4f, 7);
      b.ball(x < 0 ? "#e11d48" : "#a855f7", {x, 0.48f, 2.2f}, {0.2f, 0.15f, 0.2f});
    }
  } else if (type == "watchtower") {
    b.box(GREY1, {0, 0.5f, 0}, {3.4f, 1.0f, 3.4f});
    float y = 1.0f;
    for (int i = 0; i < 9; i++) {
      b.box(WOOD1, {0, y + 0.09f, 0}, {2.9f, 0.18f, 2.9f});
      b.box(i % 2 ? GREY2 : GREY1, {0, y + 0.34f, 0}, {2.75f, 0.32f, 2.75f});
      y += 0.5f;
    }
    b.box(WOOD1, {0, y + 0.12f, 0}, {4.0f, 0.24f, 4.0f});
    for (int sx = -1; sx <= 1; sx += 2)
      for (int sz = -1; sz <= 1; sz += 2) b.rod(WOOD2, {sx * 1.85f, y, sz * 1.85f}, {sx * 1.85f, y + 1.6f, sz * 1.85f}, 0.07f, 0.07f);
    for (int s = -1; s <= 1; s += 2) {
      b.box(WOOD2, {0, y + 0.85f, s * 1.9f}, {3.9f, 0.08f, 0.08f});
      b.box(WOOD2, {s * 1.9f, y + 0.85f, 0}, {0.08f, 0.08f, 3.9f});
    }
    b.cone(ROOF1, {0, y + 2.2f, 0}, 3.2f, 1.3f, 4, {0, PI / 4, 0});
    b.rod(WOOD2, {0, y + 2.8f, 0}, {0, y + 4.2f, 0}, 0.04f, 0.03f);
    b.box("#ea580c", {0.32f, y + 3.95f, 0}, {0.6f, 0.35f, 0.02f});
  } else if (type == "bridge") {
    float L = BRIDGE_LENGTH, W = 3.0f, deck = BRIDGE_DECK_Y;
    for (int i = 0; i < 22; i++) {
      float x = -L / 2 + (i + 0.5f) * (L / 22);
      b.box(i % 2 ? WOOD1 : "#8a5a34", {x, deck - 0.08f, 0}, {L / 22 - 0.04f, 0.16f, W});
    }
    b.box(WOOD2, {0, deck - 0.3f, W / 2 - 0.2f}, {L, 0.25f, 0.25f});
    b.box(WOOD2, {0, deck - 0.3f, -W / 2 + 0.2f}, {L, 0.25f, 0.25f});
    for (float x : {-5.5f, 5.5f}) b.box(GREY1, {x, deck - 2.4f, 0}, {1.4f, 4.4f, 3.4f});
    for (float x : {-L / 2 + 0.6f, L / 2 - 0.6f}) b.box(GREY2, {x, deck - 1.2f, 0}, {1.6f, 2.2f, 3.8f});
    // approach ramps: buried on the high bank, a walkable slope on the low bank
    const float RL = BRIDGE_RAMP, RD = BRIDGE_RAMP_DROP, ang = std::atan2(RD, RL), hyp = std::sqrt(RL * RL + RD * RD);
    for (int s = -1; s <= 1; s += 2) {
      b.box("#8a5a34", {s * (L / 2 + RL / 2), deck - RD / 2 - 0.12f, 0}, {hyp, 0.24f, W - 0.2f}, {0, 0, -s * ang});
      b.box(GREY2, {s * (L / 2 + RL * 0.35f), deck - RD * 0.35f - 0.9f, 0}, {RL * 0.7f, 1.6f, W - 0.4f}, {0, 0, -s * ang});
    }
    for (int i = 0; i <= 11; i++) {
      float x = -L / 2 + i * (L / 11);
      for (int s = -1; s <= 1; s += 2) b.rod(WOOD2, {x, deck, s * (W / 2 - 0.1f)}, {x, deck + 1.0f, s * (W / 2 - 0.1f)}, 0.06f, 0.05f);
    }
    for (int s = -1; s <= 1; s += 2) b.box(WOOD1, {0, deck + 0.95f, s * (W / 2 - 0.1f)}, {L, 0.1f, 0.1f});
  } else {
    house(b, 3.4f, 3.0f, 3);
  }
  return b;
}


struct LegStyle { const char *up, *low, *foot, *accent; float rTop, rKnee, rAnk; bool paw; };

const LegStyle LEG_STYLES[6] = {
    {"#a8622f", "#7c4420", "#2a1f1a", "#efe2c6", 0.078f, 0.042f, 0.028f, false},
    {"#2c221b", "#2c221b", "#151110", "#4b3a2c", 0.09f, 0.058f, 0.044f, false},
    {"#7a6a58", "#4a3f35", "#1d1814", "#ece4d4", 0.078f, 0.046f, 0.034f, false},
    {"#857f76", "#a88c66", "#2f2c28", "#1a1715", 0.1f, 0.062f, 0.048f, true},
    {"#1d1a18", "#2b2622", "#1d1a18", "#d9cfbd", 0.17f, 0.125f, 0.11f, true},
    {"#c9984c", "#c9984c", "#efe4cc", "#2a1d12", 0.07f, 0.044f, 0.038f, true},
};

void buildLegs(SpeciesModel& m, int sp, float hipF, float hipH) {
  const LegStyle& S = LEG_STYLES[sp];
  float fh = S.paw ? S.rAnk * 0.9f : 0.06f;
  m.footLen = fh;
  m.paw = S.paw;
  float tf = (hipF - fh) * 1.05f, th = (hipH - fh) * 1.07f;
  m.lenF1 = tf * 0.5f; m.lenF2 = tf * 0.5f;
  m.lenH1 = th * 0.46f; m.lenH2 = th * 0.54f;
  int seg = sp == 4 ? 9 : 7;
  {
    Shape u;
    float l = m.lenF1;
    u.ball(S.up, {0, 0.02f, 0}, {S.rTop * 1.25f, S.rTop * 1.6f, S.rTop * 1.4f}, Z0, 1)
        .rod(S.up, {0, 0, 0}, {0, -l * 0.55f, 0.01f}, S.rTop * 1.05f, S.rTop * 0.8f, seg)
        .rod(S.up, {0, -l * 0.55f, 0.01f}, {0, -l, 0}, S.rTop * 0.8f, S.rKnee, seg)
        .ball(S.up, {0, -l, 0}, {S.rKnee * 1.15f, S.rKnee * 1.25f, S.rKnee * 1.2f}, Z0, 1);
    m.upperF = uploadMesh(u.build(false));
  }
  {
    Shape u;
    float l = m.lenH1;
    u.ball(S.up, {0, 0.03f, 0.02f}, {S.rTop * 1.5f, S.rTop * 2.1f, S.rTop * 1.9f}, Z0, 1)
        .rod(S.up, {0, 0, 0.02f}, {0, -l * 0.6f, -0.01f}, S.rTop * 1.3f, S.rTop * 0.85f, seg)
        .rod(S.up, {0, -l * 0.6f, -0.01f}, {0, -l, 0}, S.rTop * 0.85f, S.rKnee * 0.95f, seg)
        .ball(S.low, {0, -l, -S.rKnee * 0.3f}, {S.rKnee * 1.1f, S.rKnee * 1.2f, S.rKnee * 1.5f}, Z0, 1);
    m.upperH = uploadMesh(u.build(false));
  }
  for (int h = 0; h < 2; h++) {
    Shape lo;
    float l = h ? m.lenH2 : m.lenF2;
    lo.rod(S.low, {0, 0, 0}, {0, -l * 0.7f, 0}, S.rKnee * 0.85f, S.rAnk * 1.05f, seg)
        .rod(S.low, {0, -l * 0.7f, 0}, {0, -l, 0}, S.rAnk * 1.05f, S.rAnk * 0.95f, seg)
        .ball(S.low, {0, -l, 0}, {S.rAnk * 1.25f, S.rAnk * 1.2f, S.rAnk * 1.3f}, Z0, 1);
    if (!S.paw) lo.ball(S.accent, {0, -l * 0.15f, S.rKnee * 0.4f}, {S.rKnee * 0.5f, l * 0.12f, S.rKnee * 0.4f}, Z0, 0);
    if (sp == 5) for (int k = 0; k < 3; k++) lo.ball(S.accent, {(k - 1) * 0.02f, -l * (0.25f + k * 0.2f), 0.03f}, {0.012f, 0.02f, 0.012f}, Z0, 0);
    (h ? m.lowerH : m.lowerF) = uploadMesh(lo.build(false));
  }
  for (int h = 0; h < 2; h++) {
    Shape f;
    float r = S.rAnk;
    if (S.paw) {
      f.ball(S.foot, {0, -fh * 0.55f, r * 0.5f}, {r * 1.35f, fh * 0.6f, r * 1.6f}, Z0, 1);
      for (int k = 0; k < 4; k++) {
        float x = (k - 1.5f) * r * 0.62f, z = r * (1.45f + (k == 1 || k == 2 ? 0.3f : 0));
        f.ball(S.foot, {x, -fh * 0.7f, z}, {r * 0.42f, fh * 0.42f, r * 0.5f}, Z0, 0);
        f.cone(S.accent, {x, -fh * 0.82f, z + r * 0.5f}, r * 0.12f, r * 0.45f, 4, {PI / 2, 0, 0});
      }
      if (!h) f.ball(S.foot, {0, -fh * 0.1f, -r * 0.9f}, {r * 0.35f, r * 0.35f, r * 0.3f}, Z0, 0);
    } else {
      float hw = r * 0.75f, hl = r * 2.2f;
      for (int sgn = -1; sgn <= 1; sgn += 2)
        f.box(S.foot, {sgn * hw * 0.55f, -fh * 0.5f, hl * 0.35f}, {hw, fh, hl}, {0.08f, 0, sgn * 0.05f});
      f.ball(S.low, {0, -fh * 0.05f, 0}, {r * 1.1f, fh * 0.45f, r * 1.1f}, Z0, 0);
      for (int sgn = -1; sgn <= 1; sgn += 2) f.ball(S.foot, {sgn * r * 0.7f, fh * 0.2f, -r * 0.9f}, {r * 0.3f, r * 0.35f, r * 0.3f}, Z0, 0);
    }
    (h ? m.footH : m.footF) = uploadMesh(f.build(false));
  }
}

std::map<int, std::unique_ptr<SpeciesModel>> g_species;
std::map<std::string, std::unique_ptr<Mesh>> g_meshes;

Mesh& cached(const std::string& key, const std::function<MeshData()>& make) {
  auto it = g_meshes.find(key);
  if (it != g_meshes.end()) return *it->second;
  auto m = std::make_unique<Mesh>(uploadMesh(make()));
  Mesh& r = *m;
  g_meshes[key] = std::move(m);
  return r;
}

}  // namespace

float footprint(const std::string& t) {
  if (t == "campfire") return 1.8f;
  if (t == "hunter_hut") return 3.0f;
  if (t == "carpenter_workshop") return 3.8f;
  if (t == "herbalist_lodge") return 3.0f;
  if (t == "watchtower") return 2.6f;
  if (t == "bridge") return 0;
  if (t == "shelter") return 2.5f;
  if (t == "stone_storehouse") return 3.0f;
  return 3.0f;
}

namespace models {

const Vec3 BOW_TIPS[2] = {{0, 0.565f, 0.152f}, {0, -0.565f, 0.152f}};

const SpeciesModel& species(int sp, bool lod) {
  int key = sp + (lod ? 1000 : 0);
  auto it = g_species.find(key);
  if (it != g_species.end()) return *it->second;
  g_geoLod = lod ? 1 : 0;
  SpeciesBuild b = sp == 0 ? deer() : sp == 1 ? boar() : sp == 2 ? goral() : sp == 3 ? wolf() : sp == 4 ? bear() : leopard();
  auto m = std::make_unique<SpeciesModel>();
  m->body = uploadMesh(b.body.build(false));
  m->head = uploadMesh(b.head.build(false));
  m->leg = uploadMesh(b.leg.build(false));
  m->headPivot = b.headPivot;
  for (int i = 0; i < 4; i++) m->legPivots[i] = b.legs[i];
  m->hitBody = b.hb;
  m->hitHead = b.hh;
  m->shadowRadius = b.shadow;
  buildLegs(*m, sp, b.legs[0].y, b.legs[2].y);
  if (std::getenv("HW_TRIS")) std::printf("species %d%s: body %d head %d leg %d x4 -> %d tris\n", sp, lod ? " (far LOD)" : "", m->body.count / 3, m->head.count / 3, m->leg.count / 3, (m->body.count + m->head.count + 4 * m->leg.count) / 3);
  g_geoLod = 0;
  SpeciesModel& r = *m;
  g_species[key] = std::move(m);
  return r;
}

const Mesh& building(const std::string& type) {
  return cached("b:" + type, [&] { return buildingShape(type).build(false); });
}

const Mesh& log() {
  return cached("log", [] {
    Shape s;
    s.rod("#6b4a32", {-1.1f, 0.27f, 0}, {1.1f, 0.3f, 0.05f}, 0.29f, 0.25f, 8)
        .tube("#d9b98c", {1.11f, 0.3f, 0.05f}, 0.235f, 0.235f, 0.03f, 8, {0, 0, PI / 2})
        .tube("#b9925f", {1.12f, 0.3f, 0.05f}, 0.13f, 0.13f, 0.03f, 8, {0, 0, PI / 2})
        .tube("#d9b98c", {-1.11f, 0.27f, 0}, 0.275f, 0.275f, 0.03f, 8, {0, 0, PI / 2})
        .rod("#5a3820", {0.2f, 0.42f, 0.1f}, {0.45f, 0.85f, 0.35f}, 0.07f, 0.04f, 5)
        .ball("#4d7c2f", {-0.3f, 0.52f, 0}, {0.32f, 0.07f, 0.2f});
    return s.build(false);
  });
}

const Mesh& cairn() {
  return cached("cairn", [] {
    Shape b;
    float stones[4][3] = {{0, 0.25f, 0.55f}, {0.1f, 0.62f, 0.42f}, {-0.05f, 0.92f, 0.32f}, {0.04f, 1.14f, 0.22f}};
    for (int i = 0; i < 4; i++) b.add(dodecaGeo(stones[i][2]), i % 2 ? GREY2 : GREY1, {stones[i][0], stones[i][1], 0}, {(float)i, i * 0.7f, 0}, {1, 0.6f, 1});
    b.rod(WOOD2, {0.7f, 0, 0.2f}, {0.7f, 1.85f, 0.2f}, 0.05f, 0.04f);
    b.box(WOOD1, {0.7f, 1.52f, 0.255f}, {0.92f, 0.5f, 0.06f});
    b.cone(WOOD2, {0.7f, 1.93f, 0.2f}, 0.06f, 0.12f, 5);
    return b.build(false);
  });
}

const Mesh& arrow() {
  return cached("arrow", [] {
    Shape s;
    s.rod("#c8a26b", {0, 0, 0.42f}, {0, 0, -0.38f}, 0.011f, 0.011f, 4)
        .cone("#4b5563", {0, 0, -0.43f}, 0.025f, 0.1f, 4, {-PI / 2, 0, 0})
        .box("#dc2626", {0, 0.018f, 0.36f}, {0.004f, 0.035f, 0.12f})
        .box("#f5f5f4", {0.016f, -0.009f, 0.36f}, {0.035f, 0.004f, 0.12f}, {0, 0, 0.5f})
        .box("#f5f5f4", {-0.016f, -0.009f, 0.36f}, {0.035f, 0.004f, 0.12f}, {0, 0, -0.5f});
    return s.build(true);
  });
}

void tagMat(Shape& s, const char* hex, uint8_t code);
const Mesh& bow() {
  return cached("bow2", [] {
    Shape b;
    const char *WOOD = "#7b5232", *BACK = "#8f6440", *HORN = "#bfae8c", *BRASS = "#33241a", *LEATHER = "#3b2a1d", *STITCH = "#2e2117";
    b.tube(LEATHER, {0, 0, 0}, 0.031f, 0.031f, 0.17f, 14);
    for (int k = 0; k < 6; k++) b.tube(STITCH, {0, -0.07f + k * 0.028f, 0}, 0.0325f, 0.0325f, 0.004f, 14);
    for (float s : {1.0f, -1.0f}) {
      b.tube(BRASS, {0, s * 0.09f, 0}, 0.03f, 0.03f, 0.014f, 14);
      V prev{0, s * 0.085f, 0};
      const int N = 9;
      for (int i = 1; i <= N; i++) {
        float t = (float)i / N;
        V p{0, s * (0.085f + t * 0.475f), 0.15f * t * t - 0.03f * std::sin(t * PI)};
        float r0 = 0.026f - (t - 1.0f / N) * 0.011f, r1 = 0.026f - t * 0.0115f;
        b.rod(WOOD, prev, p, r0, r1, 8);
        b.rod(BACK, {prev.x, prev.y, prev.z - r0 * 0.75f}, {p.x, p.y, p.z - r1 * 0.75f}, r0 * 0.45f, r1 * 0.45f, 6);
        prev = p;
      }
      V tip{0, s * 0.6f, 0.12f};
      b.rod(WOOD, prev, tip, 0.0115f, 0.009f, 7);
      b.rod(HORN, tip, {0, s * 0.63f, 0.085f}, 0.009f, 0.006f, 7);
      b.ball(HORN, {0, s * 0.565f, 0.152f}, {0.011f, 0.011f, 0.011f}, Z0, 1);
    }
    b.box(LEATHER, {0.026f, 0.1f, -0.004f}, {0.012f, 0.03f, 0.03f});
    tagMat(b, WOOD, 60); tagMat(b, BACK, 60); tagMat(b, HORN, 30);
    for (const char* c : {LEATHER, STITCH, BRASS}) tagMat(b, c, 150);
    return b.build(true);
  });
}

// ---- first-person hands: smooth swept fingers (3 phalanges each, real proportions, knuckles, nails and pads),
// a fleshy palm with thenar and heel pads, a wrist that narrows and a forearm that swells toward the elbow.
// Units: the view model is drawn at 0.5 scale, so 1 unit = ~0.6 m (a finger is ~0.028 units across).
// a tube swept along a polyline with a radius per point, rounded caps at both ends, slightly flattened
// (fingers and wrists are wider than they are deep); smooth normals so joints don't show seams
Geo sweep(const std::vector<V>& P, const std::vector<float>& R, int sides, float flat = 0.88f, V flatAxis = {0, 0, 0}, bool capStart = true) {
  Geo g;
  int n = (int)P.size();
  std::vector<V> T(n);
  for (int i = 0; i < n; i++) T[i] = (P[std::min(n - 1, i + 1)] - P[std::max(0, i - 1)]).normalized();
  V up = std::fabs(T[0].y) < 0.9f ? V(0, 1, 0) : V(1, 0, 0);
  if (flatAxis.lengthSq() > 0) up = flatAxis;
  V N = (up - T[0] * up.dot(T[0])).normalized();
  auto ring = [&](V c, V t, V nn, float r, float rf) {
    V b = t.cross(nn).normalized();
    for (int k = 0; k <= sides; k++) {
      float a = (float)k / sides * TAU;
      V d = nn * (std::cos(a) * flat) + b * std::sin(a);
      g.pos.push_back(c + d * r * rf);
      g.nrm.push_back((nn * (std::cos(a) / flat) + b * std::sin(a)).normalized());
    }
  };
  // cap rings (hemisphere) at the start
  int rings = 0;
  for (int c = 3; c >= 1 && capStart; c--) { float a = c / 4.0f * 1.5708f; ring(P[0] - T[0] * (R[0] * std::sin(a) * 0.9f), T[0], N, R[0], std::cos(a)); rings++; }
  for (int i = 0; i < n; i++) {
    if (i > 0) { N = (N - T[i] * N.dot(T[i])).normalized(); }  // parallel transport: no twisting
    ring(P[i], T[i], N, R[i], 1.0f); rings++;
  }
  for (int c = 1; c <= 3; c++) { float a = c / 4.0f * 1.5708f; ring(P[n - 1] + T[n - 1] * (R[n - 1] * std::sin(a) * 0.9f), T[n - 1], N, R[n - 1], std::cos(a)); rings++; }
  int W = sides + 1;
  for (int r = 0; r + 1 < rings; r++)
    for (int k = 0; k < sides; k++) {
      uint32_t a = r * W + k, b = a + 1, c = a + W, d = c + 1;
      g.idx.insert(g.idx.end(), {a, b, c, b, d, c});  // outward-facing (front faces are culled-in otherwise)
    }
  return g;
}
// material codes in the vertex alpha for the view-model shader: 250 skin, 200 nail, 150 leather, 100 cloth,
// 60 wood, 30 horn (255 = plain colour)
void tagMat(Shape& s, const char* hex, uint8_t code) {
  uint32_t k = hexToPacked(hex) & 0xffffffu;
  for (auto& v : s.verts) if ((v.c & 0xffffffu) == k) v.c = (v.c & 0xffffffu) | ((uint32_t)code << 24);
}
// smooth a coarse polyline into a Catmull-Rom curve (4 samples per span) so long limbs bend without visible kinks
template <class T> std::vector<T> densify(const std::vector<T>& p) {
  std::vector<T> o;
  int n = (int)p.size();
  for (int i = 0; i + 1 < n; i++) {
    const T &a = p[std::max(0, i - 1)], &b = p[i], &c = p[i + 1], &d = p[std::min(n - 1, i + 2)];
    for (int k = 0; k < 4; k++) {
      float t = k / 4.0f, t2 = t * t, t3 = t2 * t;
      o.push_back((b * 2.0f + (c - a) * t + (a * 2.0f - b * 5.0f + c * 4.0f - d) * t2 + (b * 3.0f - a - c * 3.0f + d) * t3) * 0.5f);
    }
  }
  o.push_back(p.back());
  return o;
}
Shape& addGeo(Shape& s, const Geo& g, const char* hex) { return s.addM(g, hexToPacked(hex), Mat4()); }
// one finger as a curve of joint points: base knuckle -> PIP -> DIP -> tip. The pad, nail and knuckle shading ride on it.
void finger(Shape& s, const V* J, float r, const char* skin, const char* knuckle, const char* nail, V back) {
  r *= 1.2f;  // sturdier, working hands
  std::vector<V> P;
  std::vector<float> R;
  float rr[4] = {r * 1.08f, r * 0.98f, r * 0.9f, r * 0.8f};
  for (int i = 0; i < 3; i++)
    for (int k = 0; k < 4; k++) {  // subdivide each phalanx, slightly fuller mid-bone (soft flesh), narrower at joints
      float t = k / 4.0f;
      P.push_back(J[i] + (J[i + 1] - J[i]) * t);
      R.push_back((rr[i] + (rr[i + 1] - rr[i]) * t) * (1.0f + 0.05f * std::sin(t * PI)));
    }
  P.push_back(J[3]); R.push_back(rr[3]);
  addGeo(s, sweep(P, R, 20, 0.86f, back), skin);
  (void)knuckle;
  // nail on the back of the last bone
  V d = (J[3] - J[2]).normalized();
  V c = J[2] + (J[3] - J[2]) * 0.62f + back * (rr[3] * 0.72f);
  Quat q = Quat::fromUnitVectors({0, 1, 0}, d);
  s.addM(sphereGeo(1, 10, 6), hexToPacked(nail), Mat4::compose(c, q, {r * 0.62f, r * 0.62f * 1.25f, r * 0.28f}));
}
// palm built like a real one: a broad flattened bone from the wrist to each knuckle (metacarpal), side by side, so
// the fingers grow out of one continuous palm instead of sitting on a ball
void palmBone(Shape& s, V W, V K, float rw, float rk, V back, const char* skin) {
  V m = W + (K - W) * 0.55f + back * (rk * 0.25f);
  std::vector<V> P = densify(std::vector<V>{W, m, K});
  std::vector<float> R = densify(std::vector<float>{rw, (rw + rk) * 0.55f, rk});
  addGeo(s, sweep(P, R, 24, 0.55f, back), skin);
}
V onArc(float rad, float ang, float y) { return {-rad * std::cos(ang), y, -rad * std::sin(ang)}; }
const char *H_SKIN = "#b98463", *H_KNUCK = "#ad7559", *H_PALM = "#c99474", *H_NAIL = "#d8b7a6", *H_SLEEVE = "#5a4d3b", *H_SLEEVE2 = "#4a3f30";
const char *H_LEATHER = "#3f2c1e", *H_GLOVE = "#5a3f29", *H_GLOVE2 = "#4e3622";
void tagHands(Shape& h) {
  for (const char* c : {H_SKIN, H_KNUCK, H_PALM}) tagMat(h, c, 250);
  tagMat(h, H_NAIL, 200);
  for (const char* c : {H_LEATHER, H_GLOVE, H_GLOVE2, "#4a2f1a"}) tagMat(h, c, 150);
  for (const char* c : {H_SLEEVE, H_SLEEVE2}) tagMat(h, c, 100);
}
// a cloth sleeve: tapered, a few soft folds, closed far end, then a cuff with trims
void sleeve(Shape& s, V a, V b, float r0, float r1, const char* trim, const char* band, Vec3 cuffRot) {
  std::vector<V> P;
  std::vector<float> R;
  for (int i = 0; i <= 24; i++) {
    float t = i / 24.0f;
    P.push_back(a + (b - a) * t);
    R.push_back((r0 + (r1 - r0) * t) * (1.0f + 0.06f * std::sin(t * 23.0f) * t));  // folds bunch further up the arm
  }
  R[0] *= 1.08f;  // open, slightly flared cuff (no closed end facing the camera)
  addGeo(s, sweep(P, R, 40, 0.92f, {0, 0, 0}, false), H_SLEEVE);
  (void)cuffRot;
  V d = (b - a).normalized();
  Quat q = Quat::fromUnitVectors({0, 0, 1}, d);
  (void)trim; (void)band;  // plain homespun cuff: a turned-back hem in the same wool, no bright trims
  s.addM(torusGeo(r0 * 1.05f, r0 * 0.09f, 10, 48), hexToPacked(H_SLEEVE2), Mat4::compose(a + d * 0.01f, q, {1, 0.92f, 1}));
}
}  // namespace

namespace models {
// ---- real hands: the WebXR generic hand mesh (MIT, Amazon), posed offline by tools/pose_hand.py into a bow grip and
// a string draw (assets/models/fp_hand_*.bin). The forearm, bracer and sleeve grow from its wrist.
bool loadPosedHand(const char* rel, Shape& h, V& wrist, V& fdir) {
  // built into the executable (an older assets folder without these files silently brought back the code-built hands);
  // a file in assets/models still overrides it, for re-posing with tools/pose_hand.py
  std::vector<uint8_t> b;
  if (!platform::readFile(platform::assetPath(rel), b) || b.size() < 8) {
    bool grip = std::strstr(rel, "grip") != nullptr;
    const uint8_t* src = grip ? FP_HAND_GRIP : FP_HAND_DRAW;
    size_t n = grip ? sizeof(FP_HAND_GRIP) : sizeof(FP_HAND_DRAW);
    b.assign(src, src + n);
  }
  uint32_t nv, ni;
  std::memcpy(&nv, b.data(), 4); std::memcpy(&ni, b.data() + 4, 4);
  size_t need = 8 + (size_t)nv * 24 + (size_t)ni * 4 + 24;
  if (b.size() < need) return false;
  const float* v = (const float*)(b.data() + 8);
  const uint32_t* id = (const uint32_t*)(b.data() + 8 + (size_t)nv * 24);
  const float* w = (const float*)(b.data() + 8 + (size_t)nv * 24 + (size_t)ni * 4);
  uint32_t col = hexToPacked(H_SKIN);
  for (uint32_t k = 0; k < ni; k++) {
    const float* q = v + (size_t)id[k] * 6;
    Shape::V o;
    o.p = {q[0], q[1], q[2]};
    o.n = V(q[3], q[4], q[5]).normalized();
    o.c = col;
    h.verts.push_back(o);
  }
  wrist = {w[0], w[1], w[2]};
  fdir = V(w[3], w[4], w[5]).normalized();
  return true;
}
// forearm from the wrist along `d`, bent toward `down` so the sleeve leaves the bottom of the screen
// forearm from the wrist: one straight, evenly thick limb (both arms built identically), then the sleeve
void armFrom(Shape& h, V W, V d, V away, float rw, bool bracer, float sleeveR) {
  (void)bracer;
  V D = (d * 0.7f + away * 0.3f).normalized();
  std::vector<V> P = {W - d * 0.015f, W + D * 0.04f, W + D * 0.12f, W + D * 0.22f};
  std::vector<float> R = {rw * 0.95f, rw, rw * 1.08f, rw * 1.15f};
  addGeo(h, sweep(densify(P), densify(R), 32, 0.84f), H_SKIN);
  V s0 = W + D * 0.16f;
  sleeve(h, s0, s0 + D * 0.7f, sleeveR, sleeveR * 1.2f, "", "", {0, 0, 0});
}
const Mesh& bowHand() {
  return cached("bowhand4", [] {
    Shape h;
    V W, d;
    if (!std::getenv("HW_PROCHAND") && loadPosedHand("models/fp_hand_grip.bin", h, W, d)) {
      armFrom(h, W, d, V(-0.25f, -0.85f, 0.45f).normalized(), 0.04f, false, 0.056f);
      tagHands(h);
      return h.build(true);
    }
    h = Shape();
    // fingers wrap the leather grip (radius ~0.031 at the origin, along y), knuckles toward the target (-z)
    const float Rg = 0.031f;
    const float ys[4] = {0.047f, 0.017f, -0.013f, -0.041f}, lens[4] = {1.0f, 1.07f, 1.0f, 0.82f}, rads[4] = {0.0142f, 0.0148f, 0.014f, 0.0122f};
    V back(0, 0, 0);
    for (int f = 0; f < 4; f++) {
      float r = rads[f], Rw = Rg + r + 0.002f, L = lens[f];
      V J[4] = {V(-0.07f, ys[f] + 0.002f, -0.036f + 0.004f * f), onArc(Rw, 1.62f, ys[f]), onArc(Rw, 1.62f + 1.05f * L, ys[f] - 0.002f), onArc(Rw * 0.98f, 1.62f + 1.82f * L, ys[f] - 0.004f)};
      V bk = (J[1] - V(0, J[1].y, 0)).normalized();  // back of the finger faces away from the grip
      finger(h, J, r, H_SKIN, H_KNUCK, H_NAIL, bk);
    }
    {
      V W(-0.092f, -0.03f, 0.035f), bk = V(-1, 0, 0.35f).normalized();
      for (int f = 0; f < 4; f++) {
        V K(-0.07f, ys[f] + 0.002f, -0.036f + 0.004f * f);
        V Wf = W + V(0, (1.5f - f) * 0.011f, 0);
        palmBone(h, Wf, K, 0.026f, rads[f] * 1.2f * 1.12f, bk, H_SKIN);
      }
    }
    // thumb: from the thenar mound over the top of the grip, tip resting on the index finger
    {
      V J[4] = {V(-0.055f, 0.045f, 0.034f), V(-0.026f, 0.066f, 0.04f), V(0.004f, 0.07f, 0.03f), V(0.027f, 0.064f, 0.016f)};
      palmBone(h, V(-0.086f, -0.012f, 0.045f), J[0], 0.028f, 0.0142f * 1.2f * 1.15f, V(-0.4f, 0.3f, 1).normalized(), H_SKIN);  // thenar: thumb grows from the heel of the hand
      finger(h, J, 0.0142f, H_SKIN, H_KNUCK, H_NAIL, V(0, 1, 0.2f).normalized());
    }
    // wrist and forearm: narrow at the wrist, swelling toward the elbow
    {
      std::vector<V> P = {V(-0.09f, -0.035f, 0.03f), V(-0.1f, -0.09f, 0.08f), V(-0.11f, -0.15f, 0.15f), V(-0.125f, -0.22f, 0.24f)};
      std::vector<float> R = {0.046f, 0.045f, 0.05f, 0.056f};  // thick wrist and forearm
      addGeo(h, sweep(densify(P), densify(R), 32, 0.82f), H_SKIN);
    }
    // leather bracer laced over the forearm, then the sleeve
    {
      std::vector<V> P = {V(-0.105f, -0.12f, 0.12f), V(-0.112f, -0.16f, 0.17f), V(-0.12f, -0.2f, 0.22f)};
      std::vector<float> R = {0.053f, 0.057f, 0.06f};
      addGeo(h, sweep(densify(P), densify(R), 32, 0.84f), H_LEATHER);
      Quat q = Quat::fromUnitVectors({0, 0, 1}, (P[2] - P[0]).normalized());
      for (int k = 0; k < 3; k++) { float t = 0.2f + k * 0.3f; h.addM(torusGeo(0.056f + t * 0.005f, 0.0022f, 4, 18), hexToPacked(H_GLOVE2), Mat4::compose(P[0] + (P[2] - P[0]) * t, q, {1, 0.86f, 1})); }  // thongs, same leather
    }
    sleeve(h, V(-0.122f, -0.215f, 0.235f), V(-0.24f, -0.62f, 0.78f), 0.068f, 0.085f, "#efe6d2", "#b91c1c", {0.9f, 0, 0.25f});
    tagHands(h);
    return h.build(true);
  });
}

const Mesh& drawHand() {
  return cached("drawhand4", [] {
    Shape h;
    V W, d;
    if (!std::getenv("HW_PROCHAND") && loadPosedHand("models/fp_hand_draw.bin", h, W, d)) {
      armFrom(h, W, d, V(0.35f, -0.85f, 0.4f).normalized(), 0.04f, false, 0.056f);
      tagHands(h);
      return h.build(true);
    }
    // Mediterranean draw: index above the arrow, middle and ring below, hooked round the string at the first joint;
    // the back of the hand faces out (+x), thumb and little finger tucked in
    const float ys[3] = {0.02f, -0.008f, -0.035f}, rads[3] = {0.0138f, 0.0144f, 0.0136f};
    for (int f = 0; f < 3; f++) {
      float r = rads[f], y = ys[f];
      V J[4] = {V(0.03f, y + 0.002f, 0.05f), V(0.008f, y, 0.006f), V(-0.016f, y - 0.002f, -0.006f), V(-0.026f, y - 0.004f, 0.016f)};
      finger(h, J, r, H_GLOVE, H_GLOVE2, H_GLOVE2, V(1, 0, 0));  // leather shooting glove on the drawing fingers
    }
    {  // little finger curled into the palm
      V J[4] = {V(0.034f, -0.058f, 0.05f), V(0.012f, -0.062f, 0.03f), V(0.004f, -0.062f, 0.045f), V(0.012f, -0.06f, 0.058f)};
      finger(h, J, 0.0118f, H_SKIN, H_KNUCK, H_NAIL, V(1, 0, 0));
    }
    {  // thumb tucked along the side of the index, under the jaw when at full draw
      V J[4] = {V(0.03f, 0.03f, 0.075f), V(0.012f, 0.045f, 0.055f), V(0.0f, 0.048f, 0.04f), V(-0.006f, 0.044f, 0.028f)};
      finger(h, J, 0.0158f, H_SKIN, H_KNUCK, H_NAIL, V(0, 1, 0));
    }
    {
      V W(0.05f, -0.022f, 0.104f), bk(1, 0, 0);
      for (int f = 0; f < 3; f++) palmBone(h, W + V(0, (1 - f) * 0.012f, 0), V(0.03f, ys[f] + 0.002f, 0.05f), 0.025f, rads[f] * 1.2f * 1.12f, bk, H_SKIN);
      palmBone(h, W + V(0, -0.022f, 0), V(0.034f, -0.058f, 0.05f), 0.023f, 0.0118f * 1.2f * 1.1f, bk, H_SKIN);
      palmBone(h, W + V(-0.004f, 0.018f, 0.004f), V(0.03f, 0.03f, 0.075f), 0.024f, 0.0158f * 1.2f * 1.1f, V(0.6f, 0.8f, 0).normalized(), H_SKIN);
    }
    h.box("#4a2f1a", {0.002f, -0.006f, 0.008f}, {0.006f, 0.068f, 0.024f});  // leather finger tab
    {
      // the forearm runs into the sleeve along the sleeve's own axis (it used to angle off and poke out of its side)
      V S0(0.078f, -0.053f, 0.18f), dn = (V(0.27f, -0.62f, 0.62f) - S0).normalized();
      std::vector<V> P = {V(0.05f, -0.02f, 0.1f), V(0.064f, -0.036f, 0.14f), S0 + dn * 0.04f, S0 + dn * 0.13f};
      std::vector<float> R = {0.043f, 0.044f, 0.048f, 0.052f};
      addGeo(h, sweep(densify(P), densify(R), 32, 0.82f), H_SKIN);
    }
    sleeve(h, V(0.078f, -0.053f, 0.18f), V(0.27f, -0.62f, 0.62f), 0.06f, 0.085f, "#efe6d2", "#b91c1c", {1.2f, 0, -0.3f});
    tagHands(h);
    return h.build(true);
  });
}

const Mesh& tent() {
  return cached("tent", [] {
    Shape s;
    s.add(translated(coneGeo(1.5f, 2.2f, 4), {0, 1.1f, 0}), "#cbb994");
    return s.build(false);
  });
}

const Mesh& unitBox() {
  return cached("unitbox", [] {
    Shape s;
    s.add(boxGeo(1, 1, 1), "#ffffff");
    return s.build(false);
  });
}

const Mesh& cone6() {
  return cached("cone6", [] {
    Shape s;
    s.add(translated(coneGeo(0.42f, 1.1f, 6), {0, 0.55f, 0}), "#ffffff");
    return s.build(false);
  });
}

const Mesh& cone5() {
  return cached("cone5", [] {
    Shape s;
    s.add(translated(coneGeo(0.24f, 0.75f, 5), {0, 0.38f, 0}), "#ffffff");
    return s.build(false);
  });
}

const Mesh& unitCylinder() {
  return cached("cyl5", [] {
    Shape s;
    s.add(cylinderGeo(0.0036f, 0.0036f, 1, 6), "#efe6d2");
    return s.build(false);
  });
}

void shutdown() {
  for (auto& s : g_species) { s.second->body.destroy(); s.second->head.destroy(); s.second->leg.destroy(); }
  g_species.clear();
  for (auto& m : g_meshes) m.second->destroy();
  g_meshes.clear();
}

}  // namespace models
