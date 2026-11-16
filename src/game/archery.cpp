#include "player.h"
#include "secrets.h"
#include "skills.h"
#include "../render/water.h"
#include <cstdlib>
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
const Pose REST{{0.15f, -0.27f, -0.58f}, {0.12f, 0.22f, -1.0f}};  // bow canted low across the body, like a hunter at the ready
const Pose AIM{{-0.02f, -0.19f, -0.6f}, {0.08f, 0.04f, -1.2f}};  // bow laid across left to right below the reticle (canted hunting draw)  // grip below-left of the reticle: the arrow, not the fist, points at the target
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
  secrets::onShot(camPos.x, camPos.z);
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
    if (secrets::arrowStep(a.prev, a.pos)) { a.state = 0; continue; }
    if (extras::arrowHit(a.prev, a.pos, (28 + 62 * a.power) * (a.aimed ? 1.15f : 1) * (invCount("recurve_bow") > 0 ? 1.3f : 1.0f) * skills::arrowMult())) {
      a.state = 0; continue;
    }
    ArrowHit hit;
    if (wildlife::arrowHitTest(a.prev, a.pos, hit)) {
      float dmg = (28 + 62 * a.power) * (a.aimed ? 1.15f : 1) * (invCount("recurve_bow") > 0 ? 1.3f : 1.0f) * skills::arrowMult();
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
      water::ripple(a.pos.x, wsurf, a.pos.z, 0.9f, 0.6f);
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

// ---- view-model shader: the hands and bow are seen inches from the eye, so they get real material shading
// instead of flat vertex colour: skin with light bleeding red through its edges (subsurface), fine pores and a soft
// sheen; wood with long grain streaks and a waxed gloss; leather and wool with their own fine texture.
const char* VM_VS = R"(
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec4 a_col;
layout(location = 2) in vec4 a_nrm;
uniform mat4 u_model;
out vec3 v_wpos; out vec3 v_opos; out vec3 v_nrm; out vec4 v_col;
void main(){
  vec4 wp = u_model * vec4(a_pos, 1.0);
  v_wpos = wp.xyz; v_opos = a_pos;
  v_nrm = normalize(mat3(u_model) * a_nrm.xyz);
  v_col = a_col;
  gl_Position = u_viewProj * wp;
}
)";
const char* VM_FS = R"(
in vec3 v_wpos; in vec3 v_opos; in vec3 v_nrm; in vec4 v_col;
out vec4 fragColor;
float n3(vec3 p){ return vnoise(p.xy + p.z * 1.7) * 0.5 + vnoise(p.yz * 1.3 + p.x) * 0.5; }
void main(){
  g_hwVel = vec4(0.0, 0.0, 2.0, 1.0);
  int code = int(v_col.a * 255.0 + 0.5);
  vec3 alb = srgbToLinear(v_col.rgb);
  vec3 n = normalize(v_nrm);
  if (!gl_FrontFacing) n = -n;
  vec3 V = normalize(u_camPos - v_wpos);
  vec3 o = v_opos;
  float spec = 0.0, rough = 32.0, sss = 0.0;
  if (code >= 245) {          // skin
    float pores = n3(o * 420.0), blot = n3(o * 38.0 + 3.1);
    alb *= 0.93 + 0.12 * pores;
    alb = mix(alb, alb * vec3(1.1, 0.88, 0.85), smoothstep(0.55, 0.85, blot) * 0.5);  // ruddier patches
    n = normalize(n + (vec3(n3(o * 600.0), n3(o * 600.0 + 7.0), n3(o * 600.0 + 13.0)) - 0.5) * 0.12);
    spec = 0.06; rough = 18.0; sss = 1.0;
  } else if (code >= 195) {   // nail
    spec = 0.25; rough = 60.0; sss = 0.5;
  } else if (code >= 145) {   // leather
    float g = n3(o * 260.0), cr = n3(o * 55.0);
    alb *= 0.82 + 0.3 * g;
    alb *= mix(1.0, 0.8, smoothstep(0.6, 0.8, cr));
    n = normalize(n + (vec3(g, n3(o * 260.0 + 5.0), n3(o * 260.0 + 9.0)) - 0.5) * 0.25);
    spec = 0.08; rough = 20.0;
  } else if (code >= 95) {    // wool / linen: fine weave and fuzz
    vec2 w = vec2(dot(o, vec3(1.0, 0.3, 0.0)), dot(o, vec3(0.0, 0.4, 1.0))) * 900.0;
    float weave = 0.5 + 0.5 * sin(w.x) * sin(w.y);
    alb *= 0.86 + 0.14 * weave + 0.12 * (n3(o * 80.0) - 0.5);
    spec = 0.0;
  } else if (code >= 55) {    // wood: long streaks of grain running along the limb, waxed
    float along = o.y * 5.0;
    float grain = vnoise(vec2((o.x + o.z) * 420.0 + vnoise(vec2(along, o.x * 40.0)) * 4.0, along));
    float rings = 0.5 + 0.5 * sin(((o.x + o.z) * 900.0 + vnoise(vec2(along * 3.0, 1.0)) * 6.0));
    alb *= mix(0.72, 1.16, grain) * mix(0.9, 1.05, rings);
    spec = 0.18; rough = 45.0;
  } else if (code >= 25) {    // horn tips
    alb *= 0.85 + 0.25 * vnoise(vec2(o.y * 300.0, o.x * 80.0));
    spec = 0.3; rough = 70.0;
  }
  float sh = shadowAt(u_camPos - vec3(0.0, 0.25, 0.0), vec3(0.0, 1.0, 0.0));
  sh = mix(0.35, 1.0, sh);
  vec3 lit = lambert(alb, n, sh);
  // subsurface: light wraps past the terminator and glows warm-red through thin skin (fingers, edges)
  float ndl = dot(n, u_sunDir.xyz);
  float wrap = max(0.0, (ndl + 0.5) / 1.5) - max(ndl, 0.0);
  float through = pow(max(dot(-V, u_sunDir.xyz), 0.0), 3.0) * (1.0 - abs(dot(n, V)));
  lit += alb * vec3(1.0, 0.42, 0.28) * u_sunCol.rgb * (wrap * 0.45 + through * 0.35) * sss * sh * (1.0 / 3.14159);
  // specular sheen
  vec3 H = normalize(V + u_sunDir.xyz);
  float fr = 0.04 + 0.96 * pow(1.0 - max(dot(n, V), 0.0), 5.0);
  lit += u_sunCol.rgb * pow(max(dot(n, H), 0.0), rough) * spec * sh * (0.5 + fr);
  lit += u_hemiSky.rgb * fr * spec * 0.15;
  fragColor = vec4(outColor(lit), 1.0);
}
)";

void drawViewModel(float dt, const Mat4& camWorld) {
  auto& B = E.bow;
  if (const char* vt = std::getenv("HW_VMTEST")) { B.aiming = vt[0] != '0'; B.draw = vt[0] == '2' ? 0.85f : 0.0f; B.drawing = vt[0] == '2'; B.cooldown = 0; }  // test: hold a pose
  bool visible = E.playing && !E.uiOpen && E.world.placing.empty() && E.cutscene.dur == 0;
  if (!visible) return;
  float k = std::min(1.0f, (B.aiming ? 1.0f : 0.0f) + B.draw * 1.4f);
  Vec3 tgt = Vec3::lerp(REST.pos, AIM.pos, k);
  float walkBob = E.player.moving ? -std::fabs(std::sin(player::stridePhase())) * 0.012f * (1 - k * 0.7f) : 0;   // in step with the feet
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
  static const bool oldVm = std::getenv("HW_OLDVM") != nullptr;
  {
    Shader& vm = render::program("viewmodel", VM_VS, VM_FS);
    render::setModel(vm, holder);
    models::bow().draw();
    models::bowHand().draw();
  }
  Shader& s = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::None, 0);
  s.set1f("u_recv", 0);
  (void)oldVm;
  float pull = 0.15f + B.draw * 0.3f;
  bool hasArrow = invCount("arrow") > 0 && B.cooldown <= 0.15f;
  if (hasArrow) {
    render::setModel(s, holder * Mat4::translate({0.012f, 0.0f, pull - 0.42f}));
    models::arrow().draw();
  }
  if (B.draw > 0.03f || B.drawing || hasArrow) {  // the string hand stays on the nocked arrow at rest too
    Shader& vm = render::program("viewmodel", VM_VS, VM_FS);
    render::setModel(vm, holder * Mat4::translate({0, 0, pull}));
    models::drawHand().draw();
    render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::None, 0);
  }
  Vec3 nock(0, 0, pull);
  Color sc = Color::hex("cfc6b2");  // waxed linen string
  for (int i = 0; i < 2; i++) {
    Vec3 tip = models::BOW_TIPS[i];
    Vec3 mid = (tip + nock) * 0.5f;
    float len = tip.distanceTo(nock);
    Quat q = Quat::fromUnitVectors({0, 1, 0}, (tip - nock).normalized());
    fx::basicMesh(models::unitCylinder(), holder * Mat4::compose(mid, q, {1, len, 1}), sc, 1, false, false);
  }
}

}  // namespace archery
