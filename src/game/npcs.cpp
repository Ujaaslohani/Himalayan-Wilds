#include "npcs.h"
#include "engine.h"
#include "fx.h"
#include "../world/terrain.h"
#include "../world/forest.h"
#include "../render/render.h"
#include "../models/people.h"
#include "../models/models.h"
#include <map>

const Camp npcs::CAMPS[4] = {
    {"hunter", 5, -4, 5, -4, "the village"},
    {"carpenter", 71, 53, -7, 7, "the eastern pine woods"},
    {"herbalist", -95, -29, 9, 8, "the meadows west of the river"},
    {"builder", 22, -122, -6, -9, "the northern ruins"},
};

namespace {
enum { M_IDLE, M_WANDER, M_TRAVEL };
std::vector<NpcRt> g_npcs;

void homeOf(const std::string& id, bool recruited, float& x, float& z) {
  const Camp* c = npcs::camp(id);
  x = recruited ? c->homeX : c->campX;
  z = recruited ? c->homeZ : c->campZ;
}
}  // namespace

namespace npcs {

const Camp* camp(const std::string& id) {
  for (auto& c : CAMPS)
    if (id == c.id) return &c;
  return &CAMPS[0];
}

void sync() {
  for (auto& s : A.specialists) {
    NpcRt* n = nullptr;
    for (auto& m : g_npcs)
      if (m.id == s.id) n = &m;
    if (!n) {
      NpcRt r;
      r.id = s.id;
      r.name = s.name;
      homeOf(s.id, s.recruited, r.x, r.z);
      r.y = world::getTerrainHeight(r.x, r.z);
      r.yaw = PI;
      r.mode = M_IDLE;
      r.tx = r.x;
      r.tz = r.z;
      r.t = 2;
      r.recruited = s.recruited;
      g_npcs.push_back(r);
      n = &g_npcs.back();
    }
    if (!s.recruited && n->recruited) {
      const Camp* c = camp(s.id);
      n->x = c->campX;
      n->z = c->campZ;
      n->y = world::getTerrainHeight(n->x, n->z);
      n->recruited = false;
      n->mode = M_IDLE;
      n->t = 2;
    }
    if (s.recruited && !n->recruited) {
      n->recruited = true;
      n->mode = M_TRAVEL;
      const Camp* c = camp(s.id);
      n->tx = c->homeX;
      n->tz = c->homeZ;
      n->travelT = 0;
    }
  }
}

void reset() {
  g_npcs.clear();
  sync();
}

NpcRt* find(float x, float z, float r) {
  NpcRt* best = nullptr;
  float bd = r * r;
  for (auto& n : g_npcs) {
    float d2 = (n.x - x) * (n.x - x) + (n.z - z) * (n.z - z);
    if (d2 < bd) { bd = d2; best = &n; }
  }
  return best;
}

bool position(const std::string& id, float& x, float& z) {
  for (auto& n : g_npcs)
    if (n.id == id) { x = n.x; z = n.z; return true; }
  return false;
}

void update(float dt) {
  const Vec3& p = E.player.pos;
  for (auto& n : g_npcs) {
    float dx = p.x - n.x, dz = p.z - n.z;
    float dPlayer = hypot2(dx, dz);
    float hx, hz;
    homeOf(n.id, n.recruited, hx, hz);
    float wantSpeed = 0;
    if (n.mode == M_TRAVEL) {
      n.travelT += dt;
      float d = hypot2(n.tx - n.x, n.tz - n.z);
      if (d < 1 || (n.travelT > 45 && dPlayer > 60)) {
        if (d >= 1) { n.x = n.tx; n.z = n.tz; }
        n.mode = M_IDLE;
        n.t = 3;
      } else {
        wantSpeed = 2.6f;
        n.yaw = turnToward(n.yaw, std::atan2(n.tx - n.x, n.tz - n.z), dt * 4);
      }
    } else if (n.mode == M_WANDER) {
      float d = hypot2(n.tx - n.x, n.tz - n.z);
      if (d < 0.5f || dPlayer < 4) {
        n.mode = M_IDLE;
        n.t = 3 + randf() * 6;
      } else {
        wantSpeed = 1.1f;
        n.yaw = turnToward(n.yaw, std::atan2(n.tx - n.x, n.tz - n.z), dt * 3);
      }
    } else {
      n.t -= dt;
      if (dPlayer < 6) n.yaw = turnToward(n.yaw, std::atan2(dx, dz), dt * 2.5f);
      if (n.t <= 0 && dPlayer > 5) {
        float a = randf() * TAU, r = n.recruited ? 1.5f + randf() * 3.5f : 1 + randf() * 2;
        n.tx = hx + std::cos(a) * r;
        n.tz = hz + std::sin(a) * r;
        n.mode = M_WANDER;
      }
    }
    n.speed += (wantSpeed - n.speed) * std::min(1.0f, dt * 5);
    n.x += std::sin(n.yaw) * n.speed * dt;
    n.z += std::cos(n.yaw) * n.speed * dt;
    if (n.mode == M_TRAVEL) forest::resolveTreeCollision(n.x, n.z, 0.3f);
    n.y = world::getTerrainHeight(n.x, n.z);
    n.phase += dt * n.speed * 3.4f;
    float look = dPlayer < 7 ? clampf(wrapAngle(std::atan2(dx, dz) - n.yaw), -1, 1) : 0;
    n.headYaw += (look - n.headYaw) * std::min(1.0f, dt * 4);
  }
}

void initRender() {
  registerMarkers("npcs", [](std::vector<Marker>& out) {
    for (auto& n : g_npcs) out.push_back({n.x, n.y + 2.35f, n.z, n.recruited ? MI_NPC : MI_RECRUIT, n.name});
  });
  for (auto& s : A.specialists) people::person(s.id);
}

static void drawNpcs(bool shadow) {
  const Vec3& p = E.player.pos;
  for (auto& n : g_npcs) {
    if (std::fabs(n.x - p.x) > 110 || std::fabs(n.z - p.z) > 110) continue;
    if (shadow && hypot2(n.x - p.x, n.z - p.z) > 46) continue;
    const PersonModel& M = people::person(n.id);
    float bob = std::fabs(std::sin(n.phase)) * std::min(0.05f, n.speed * 0.02f);
    Mat4 root = Mat4::compose({n.x, n.y + bob, n.z}, Quat::axisAngle({0, 1, 0}, n.yaw), {BODY_SCALE, BODY_SCALE, BODY_SCALE});
    if (!shadow && !render::cam.sphere({n.x, n.y + 1, n.z}, 1.6f)) continue;
    float swing = std::sin(n.phase) * std::min(0.7f, n.speed * 0.3f);
    float idle = n.speed < 0.1f ? std::sin(E.clock * 1.6f + n.x) * 0.04f : 0;
    float d = hypot2(n.x - p.x, n.z - p.z);
    float breathe = std::sin(E.clock * 1.7f + n.x);
    float chat = n.speed < 0.1f && d < 6 ? std::sin(E.clock * 2.3f + n.z) * 0.06f : 0;
    Shader& s = render::object(VFmt::P3C4N4, IM_NONE, SeasonKind::None, OF_SOFT);
    render::setModel(s, root * Mat4::scale({1 + breathe * 0.008f, 1 + breathe * 0.006f, 1 + breathe * 0.012f}));
    M.torso.draw();
    Quat hq = Quat::fromEulerXYZ(chat * 0.6f - 0.04f * std::max(0.0f, 6 - d) / 6, n.headYaw, chat * 0.3f);
    render::setModel(s, root * Mat4::compose(PIV_HEAD, hq, {1, 1, 1}));
    M.head.draw();
    float armZ0 = 0.12f + (n.speed < 0.1f && d < 6 ? 0.08f + std::sin(E.clock * 1.9f) * 0.06f : 0);
    float ax[2] = {-swing + idle, swing - idle};
    float az[2] = {armZ0, -0.12f};
    for (int k = 0; k < 2; k++) {
      Vec3 pv = k == 0 ? PIV_ARM : Vec3(-PIV_ARM.x, PIV_ARM.y, 0);
      render::setModel(s, root * Mat4::compose(pv, Quat::fromEulerXYZ(ax[k], 0, az[k]), {1, 1, 1}));
      M.arm.draw();
    }
    float lx[2] = {swing, -swing};
    for (int k = 0; k < 2; k++) {
      Vec3 pv = k == 0 ? PIV_LEG : Vec3(-PIV_LEG.x, PIV_LEG.y, 0);
      render::setModel(s, root * Mat4::compose(pv, Quat::fromEulerXYZ(lx[k], 0, 0), {1, 1, 1}));
      M.leg.draw();
    }
  }
}

void draw() {
  glEnable(GL_CULL_FACE);
  drawNpcs(false);
  Shader& s = render::object(VFmt::P3C4, IM_NONE, SeasonKind::None, 0);
  for (auto& c : CAMPS) {
    if (std::string(c.id) == "hunter") continue;
    float tx = c.campX - 2.2f, tz = c.campZ - 1.2f;
    render::setModel(s, Mat4::compose({tx, world::getTerrainHeight(tx, tz), tz}, Quat::axisAngle({0, 1, 0}, 0.6f), {1, 1, 1}));
    models::tent().draw();
    float fx = c.campX + 2.5f, fz = c.campZ + 1.5f;
    render::setModel(s, Mat4::compose({fx, world::getTerrainHeight(fx, fz), fz}, Quat(), {0.6f, 0.6f, 0.6f}));
    models::building("campfire").draw();
    fx::campfire(fx, world::getTerrainHeight(fx, fz), fz, 0.6f);
  }
}

void drawShadow() { drawNpcs(true); }

void drawBlobs() {
  const Vec3& p = E.player.pos;
  for (auto& n : g_npcs) {
    if (std::fabs(n.x - p.x) > 110 || std::fabs(n.z - p.z) > 110) continue;
    fx::blob(n.x, n.y + 0.04f, n.z, 0, 0.45f, 0.45f);
  }
}

void forEach(const std::function<void(const NpcRt&)>& fn) {
  for (auto& n : g_npcs) fn(n);
}

}  // namespace npcs
