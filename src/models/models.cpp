#include "models.h"
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

const Mesh& bow() {
  return cached("bow", [] {
    Shape b;
    const char *WOOD = "#6b3f1f", *BACK = "#b07a45", *HORN = "#e8dcc4", *BRASS = "#c8992e", *LEATHER = "#3b2416", *STITCH = "#c9a86a";
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
    return b.build(true);
  });
}

const Mesh& bowHand() {
  return cached("bowhand", [] {
    const char *SKIN = "#c48a62", *KNUCK = "#b07450", *NAIL = "#e6c2aa", *BRACER = "#5a3a22", *LACE = "#d6c49a", *SLEEVE = "#1f4a2c", *TRIM = "#efe6d2", *BAND = "#b91c1c", *GOLD2 = "#facc15";
    Shape h;
    h.ball(SKIN, {-0.05f, 0.0f, 0.004f}, {0.04f, 0.064f, 0.042f}, Z0, 2)
        .ball(SKIN, {-0.03f, -0.045f, 0.03f}, {0.036f, 0.034f, 0.034f}, Z0, 2);
    for (int f = 0; f < 4; f++) {
      float y = 0.044f - f * 0.029f;
      float len = f == 3 ? 0.82f : f == 0 ? 0.96f : 1;
      float r = f == 3 ? 0.0125f : 0.0148f;
      V a{-0.046f, y, -0.026f}, m{0.004f, y, -0.05f * len}, e{0.04f * len, y - 0.002f, -0.02f}, tp{0.043f * len, y - 0.004f, 0.014f};
      h.rod(SKIN, a, m, r * 1.05f, r, 8).ball(KNUCK, m, {r * 1.08f, r * 1.08f, r * 1.08f}, Z0, 1)
          .rod(SKIN, m, e, r, r * 0.92f, 8).ball(SKIN, e, {r * 0.92f, r * 0.92f, r * 0.92f}, Z0, 1)
          .rod(SKIN, e, tp, r * 0.92f, r * 0.8f, 8).ball(SKIN, tp, {r * 0.8f, r * 0.8f, r * 0.8f}, Z0, 1)
          .box(NAIL, {tp.x + 0.006f, tp.y, tp.z + 0.003f}, {0.007f, r * 1.1f, r * 1.1f});
    }
    h.rod(SKIN, {-0.068f, 0.035f, 0.032f}, {-0.018f, 0.068f, 0.05f}, 0.019f, 0.016f, 8).ball(SKIN, {-0.018f, 0.068f, 0.05f}, {0.016f, 0.016f, 0.016f}, Z0, 1)
        .rod(SKIN, {-0.018f, 0.068f, 0.05f}, {0.028f, 0.066f, 0.046f}, 0.016f, 0.012f, 8).ball(SKIN, {0.028f, 0.066f, 0.046f}, {0.012f, 0.012f, 0.012f}, Z0, 1);
    h.rod(SKIN, {-0.06f, -0.05f, 0.035f}, {-0.085f, -0.12f, 0.1f}, 0.042f, 0.046f, 12)
        .ball(SKIN, {-0.085f, -0.12f, 0.1f}, {0.046f, 0.046f, 0.046f}, Z0, 1);
    h.rod(BRACER, {-0.082f, -0.115f, 0.095f}, {-0.125f, -0.25f, 0.27f}, 0.052f, 0.058f, 14);
    for (int k = 0; k < 5; k++) {
      float t = 0.12f + k * 0.18f;
      V p{-0.082f + (-0.043f) * t, -0.115f - 0.135f * t, 0.095f + 0.175f * t};
      h.add(torusGeo(0.054f + t * 0.005f, 0.0034f, 4, 16), LACE, p, {0.9f, 0, 0.25f});
    }
    h.rod(SLEEVE, {-0.12f, -0.235f, 0.25f}, {-0.19f, -0.46f, 0.56f}, 0.062f, 0.07f, 14);
    h.add(torusGeo(0.063f, 0.009f, 6, 16), TRIM, {-0.123f, -0.245f, 0.262f}, {0.9f, 0, 0.25f});
    h.add(torusGeo(0.064f, 0.006f, 4, 16), BAND, {-0.128f, -0.262f, 0.284f}, {0.9f, 0, 0.25f});
    h.add(torusGeo(0.065f, 0.004f, 4, 16), GOLD2, {-0.133f, -0.278f, 0.304f}, {0.9f, 0, 0.25f});
    return h.build(true);
  });
}

const Mesh& drawHand() {
  return cached("drawhand", [] {
    const char *SKIN = "#c48a62", *GLOVE = "#6b4423", *TAB = "#4a2f1a", *SLEEVE = "#1f4a2c", *TRIM = "#efe6d2", *BAND = "#b91c1c";
    Shape h;
    h.ball(SKIN, {0.024f, -0.014f, 0.058f}, {0.04f, 0.048f, 0.056f}, Z0, 2);
    for (int f = 0; f < 3; f++) {
      float y = 0.018f - f * 0.026f;
      float r = 0.0138f;
      h.rod(GLOVE, {0.016f, y, 0.034f}, {0.004f, y, -0.008f}, r, r * 0.95f, 8).ball(GLOVE, {0.004f, y, -0.008f}, {r, r, r}, Z0, 1)
          .rod(GLOVE, {0.004f, y, -0.008f}, {-0.018f, y - 0.003f, 0.006f}, r * 0.95f, r * 0.85f, 8).ball(GLOVE, {-0.018f, y - 0.003f, 0.006f}, {r * 0.85f, r * 0.85f, r * 0.85f}, Z0, 1);
    }
    h.box(TAB, {0.0f, -0.006f, 0.0f}, {0.012f, 0.07f, 0.03f});
    h.rod(SKIN, {0.04f, -0.08f, 0.05f}, {0.016f, -0.088f, 0.022f}, 0.013f, 0.011f, 8).ball(SKIN, {0.016f, -0.088f, 0.022f}, {0.011f, 0.011f, 0.011f}, Z0, 1);
    h.rod(SKIN, {0.045f, 0.026f, 0.07f}, {0.012f, 0.04f, 0.04f}, 0.016f, 0.013f, 8).ball(SKIN, {0.012f, 0.04f, 0.04f}, {0.013f, 0.013f, 0.013f}, Z0, 1);
    h.rod(SKIN, {0.035f, -0.025f, 0.095f}, {0.07f, -0.06f, 0.2f}, 0.038f, 0.044f, 12).ball(SKIN, {0.07f, -0.06f, 0.2f}, {0.044f, 0.044f, 0.044f}, Z0, 1);
    h.rod(SLEEVE, {0.065f, -0.055f, 0.185f}, {0.13f, -0.12f, 0.42f}, 0.056f, 0.064f, 14);
    h.add(torusGeo(0.057f, 0.008f, 6, 16), TRIM, {0.068f, -0.058f, 0.192f}, {1.2f, 0, -0.3f});
    h.add(torusGeo(0.058f, 0.005f, 4, 16), BAND, {0.072f, -0.062f, 0.206f}, {1.2f, 0, -0.3f});
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
