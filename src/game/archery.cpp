#include "../render/envmap.h"
#include "extras.h"
#include "../render/sky.h"
#include "archery.h"
#include "engine.h"
#include "wildlife.h"
#include "fx.h"
#include "../world/terrain.h"
#include "../world/forest.h"
#include "../models/models.h"
#include "../render/render.h"
#include "../audio/audio.h"

namespace {
const int MAX = 40;
const float GRAVITY = 9.8f * 0.42f;
const Vec3 FWD{0, 0, -1};

struct Arrow {
  int state = 0;
  Vec3 pos, prev, vel;
  Quat quat;
  float life = 0, power = 0;
  bool aimed = false;
};
Arrow g_pool[MAX];
DynBatch g_batch;

struct Pose { Vec3 pos; Vec3 rot; };
const Pose REST{{0.26f, -0.24f, -0.62f}, {0.05f, 0.28f, -0.45f}};
const Pose AIM{{0.035f, -0.05f, -0.62f}, {0.0f, 0.03f, -0.32f}};
Vec3 g_holderPos = REST.pos;
Quat g_holderQ = Quat::fromEulerXYZ(REST.rot.x, REST.rot.y, REST.rot.z);
}  // namespace

namespace archery {

void reset() {
  for (auto& a : g_pool) a.state = 0;
}

void fire(const Vec3& camPos, const Vec3& camDir, float power, bool aimed) {
  sky::scareBirds(camPos);  // the twang of the bow flushes birds from nearby trees
  Arrow* slot = nullptr;
  for (auto& a : g_pool)
    if (a.state == 0) { slot = &a; break; }
  if (!slot) {
    slot = &g_pool[0];
    for (auto& a : g_pool)
      if (a.state == 2 && (slot->state != 2 || a.life < slot->life)) slot = &a;
  }
  Vec3 dir = camDir;
  float spread = (aimed ? 0.002f : 0.012f) * (1.2f - power);
  dir.x += (randf() - 0.5f) * spread;
  dir.y += (randf() - 0.5f) * spread;
  dir.z += (randf() - 0.5f) * spread;
  dir.normalize();
  float speed = 46 + 84 * power;
  slot->state = 1;
  slot->pos = camPos + dir * 0.6f;
  slot->pos.y -= 0.05f;
  slot->prev = slot->pos;
  slot->vel = dir * speed;
  slot->quat = Quat::fromUnitVectors(FWD, dir);
  slot->life = 7;
  slot->power = power;
  slot->aimed = aimed;
  makeNoise(camPos.x, camPos.z, 14);
}

void update(float dt) {
  for (auto& a : g_pool) {
    if (a.state == 0) continue;
    a.life -= dt;
    if (a.state == 2) {
      if (a.life <= 0) a.state = 0;
      continue;
    }
    if (a.state == 3) {  // floating: carried by the baked water flow, bobbing on the surface
      float vx, vz;
      envmap::flowAt(a.pos.x, a.pos.z, vx, vz);
      a.pos.x += vx * dt;
      a.pos.z += vz * dt;
      a.pos.y = world::waterHeightAt(a.pos.x, a.pos.z) + 0.02f + 0.015f * std::sin(a.life * 3.1f);
      if (vx * vx + vz * vz > 0.01f) a.quat = Quat::fromUnitVectors(FWD, Vec3(vx, 0, vz).normalized());  // swings round to point downstream
      if (a.life <= 0 || world::waterHeightAt(a.pos.x, a.pos.z) < world::getTerrainHeight(a.pos.x, a.pos.z)) a.state = 0;
      continue;
    }
    a.prev = a.pos;
    a.vel.y -= GRAVITY * dt;
    a.pos += a.vel * dt;
    a.quat = Quat::fromUnitVectors(FWD, a.vel.normalized());
    if (extras::arrowHit(a.prev, a.pos, (28 + 62 * a.power) * (a.aimed ? 1.15f : 1) * (invCount("recurve_bow") > 0 ? 1.3f : 1.0f))) { a.state = 0; continue; }
    ArrowHit hit;
    if (wildlife::arrowHitTest(a.prev, a.pos, hit)) {
      float dmg = (28 + 62 * a.power) * (a.aimed ? 1.15f : 1) * (invCount("recurve_bow") > 0 ? 1.3f : 1.0f);
      wildlife::damage(hit.animal, dmg, hit.headshot);
      audio::meleePunch();
      a.state = 0;
      continue;
    }
    float ground = world::getTerrainHeight(a.pos.x, a.pos.z);
    float wsurf = world::waterHeightAt(a.pos.x, a.pos.z);
    if (wsurf > ground + 0.08f && a.pos.y <= wsurf) {  // into a river or lake: it floats instead of sticking in the bed
      a.pos.y = wsurf + 0.02f;
      a.quat = Quat::fromUnitVectors(FWD, Vec3(a.vel.x, 0, a.vel.z).normalized());
      a.state = 3;
      a.life = 18;
      audio::footstepWater();
      makeNoise(a.pos.x, a.pos.z, 6);
      continue;
    }
    if (a.pos.y <= ground + 0.05f) {
      a.pos.y = ground + 0.12f;
      a.state = 2;
      a.life = 14;
      makeNoise(a.pos.x, a.pos.z, 9);
      continue;
    }
    if (forest::treeTrunkAt(a.pos.x, a.pos.y, a.pos.z)) {
      a.state = 2;
      a.life = 14;
      makeNoise(a.pos.x, a.pos.z, 8);
      audio::footstepWood();
      continue;
    }
    if (a.life <= 0) a.state = 0;
  }
}

void initRender() { g_batch.init(&models::arrow()); }

void draw() {
  g_batch.clear();
  for (auto& a : g_pool) {
    if (a.state == 0) continue;
    g_batch.add(Mat4::compose(a.pos, a.quat, {1, 1, 1}));
  }
  g_batch.upload();
  if (g_batch.batch.count) {
    glEnable(GL_CULL_FACE);
    render::object(VFmt::P3C4N4, IM_FULL, SeasonKind::None, 0);
    g_batch.draw();
  }
}

void drawViewModel(float dt, const Mat4& camWorld) {
  auto& B = E.bow;
  bool visible = E.playing && !E.uiOpen && E.world.placing.empty() && E.cutscene.dur == 0;
  if (!visible) return;
  float k = std::min(1.0f, (B.aiming ? 1.0f : 0.0f) + B.draw * 1.4f);
  Vec3 tgt = Vec3::lerp(REST.pos, AIM.pos, k);
  float walkBob = E.player.moving ? std::sin(E.clock * (E.player.sprinting ? 13 : 9)) * 0.012f * (1 - k * 0.7f) : 0;
  tgt.y += walkBob;
  tgt.z += B.cooldown > 0.2f ? 0.03f : 0;
  float f = std::min(1.0f, dt * 12);
  g_holderPos = Vec3::lerp(g_holderPos, tgt, f);
  Quat qa = Quat::fromEulerXYZ(REST.rot.x, REST.rot.y, REST.rot.z), qb = Quat::fromEulerXYZ(AIM.rot.x, AIM.rot.y, AIM.rot.z);
  g_holderQ = Quat::slerp(g_holderQ, Quat::slerp(qa, qb, k), f);
  Mat4 holder = camWorld * Mat4::compose(g_holderPos, g_holderQ, {0.5f, 0.5f, 0.5f});

  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  Shader& s = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::None, 0);
  s.set1f("u_recv", 0);
  render::setModel(s, holder);
  models::bow().draw();
  render::setModel(s, holder);
  models::bowHand().draw();
  float pull = 0.15f + B.draw * 0.3f;
  bool hasArrow = invCount("arrow") > 0 && B.cooldown <= 0.15f;
  if (hasArrow) {
    render::setModel(s, holder * Mat4::translate({0.012f, 0.0f, pull - 0.42f}));
    models::arrow().draw();
  }
  if (B.draw > 0.03f || B.drawing) {
    render::setModel(s, holder * Mat4::translate({0, 0, pull}));
    models::drawHand().draw();
  }
  Vec3 nock(0, 0, pull);
  Color sc = Color::hex("efe6d2");
  for (int i = 0; i < 2; i++) {
    Vec3 tip = models::BOW_TIPS[i];
    Vec3 mid = (tip + nock) * 0.5f;
    float len = tip.distanceTo(nock);
    Quat q = Quat::fromUnitVectors({0, 1, 0}, (tip - nock).normalized());
    fx::basicMesh(models::unitCylinder(), holder * Mat4::compose(mid, q, {1, len, 1}), sc, 1, false, false);
  }
}

}  // namespace archery
