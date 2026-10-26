#include "wildlife.h"
#include "economy.h"
#include "season.h"
#include "fx.h"
#include "../world/terrain.h"
#include "../world/forest.h"
#include "../models/models.h"
#include "../render/render.h"
#include "../audio/audio.h"
#include <memory>
#include <algorithm>

namespace {

enum AState { S_GRAZE, S_WALK, S_ALERT, S_FLEE, S_CHARGE, S_DOWNED, S_STALK, S_CHASE, S_ATTACK, S_POUNCE, S_RETREAT, S_HUNT, S_FEED, S_RAID };

struct Hear { float still, crouch, walk, sprint; };
struct Spec { const char* name; float hp, walk, run; Hear hear; int herdLo, herdHi, groups; float gait, grazePitch; Cost yields; };
const Spec SPEC[SP_COUNT] = {
    {"Deer", 80, 1.25f, 9.5f, {11, 8, 22, 36}, 3, 3, 4, 3.1f, 1.05f, {{"deer_pelt", 2}, {"bone", 2}, {"sinew", 1}, {"food", 6}}},
    {"Wild Boar", 140, 0.95f, 7.5f, {8, 6, 16, 28}, 2, 3, 5, 4.2f, 0.55f, {{"boar_hide", 2}, {"bone", 3}, {"sinew", 1}, {"food", 8}}},
    {"Goral", 70, 1.1f, 8.6f, {13, 9, 25, 40}, 2, 3, 5, 3.4f, 0.85f, {{"goral_horn", 2}, {"bone", 2}, {"sinew", 1}, {"food", 5}}},
    {"Himalayan Wolf", 220, 1.5f, 9.8f, {14, 10, 26, 40}, 3, 4, 3, 3.4f, 0.5f, {{"wolf_fur", 2}, {"bone", 3}, {"sinew", 2}, {"food", 3}}},
    {"Black Bear", 520, 1.0f, 8.2f, {9, 7, 18, 30}, 1, 1, 3, 2.6f, 0.6f, {{"bear_hide", 2}, {"bone", 5}, {"sinew", 3}, {"food", 12}}},
    {"Leopard", 260, 1.2f, 11, {12, 9, 24, 38}, 1, 1, 3, 3.6f, 0.4f, {{"leopard_pelt", 1}, {"bone", 3}, {"sinew", 2}, {"food", 3}}},
};

struct PredDef { float sight, reach, dmg, cd, windup, pounce, score, growl; };
const PredDef PREDS[3] = {
    {30, 1.7f, 12, 1.1f, 0.35f, 0, 300, 140},
    {20, 2.2f, 28, 1.8f, 0.6f, 0, 600, 70},
    {24, 1.8f, 16, 1.0f, 0.3f, 9, 450, 110},
};
const PredDef* PRED(Species sp) { return sp >= SP_WOLF ? &PREDS[sp - SP_WOLF] : nullptr; }
const float VILLAGE_SAFE = 40;

}  // namespace

struct Herd {
  Species sp;
  float ax, az;
  int size;
  std::vector<Animal*> members;
  float respawnT = 0, calmT = 0;
  float tx = 0, tz = 0, migT = 0;
  bool migrating = false;
};

namespace {

std::vector<std::unique_ptr<Herd>> herds;
std::vector<std::unique_ptr<Animal>> animals;
int nextId = 1;
bool raidRepelled = false;
std::vector<Animal*> raidPack;
float lastHour = -1, coldWarned = 0;

bool walkable(float x, float z) {
  if (std::fabs(x) > MAP_HALF - 8 || std::fabs(z) > MAP_HALF - 8) return false;
  float h = world::getTerrainHeight(x, z);
  if (h < -0.55f || h > 48) return false;
  if (world::lakeAt(x, z) >= 0) return false;
  float sx = world::getTerrainHeight(x + 1.2f, z) - world::getTerrainHeight(x - 1.2f, z);
  float sz = world::getTerrainHeight(x, z + 1.2f) - world::getTerrainHeight(x, z - 1.2f);
  return sx * sx + sz * sz < 2.6f;
}

float habitatScore(Species sp, float x, float z) {
  if (!walkable(x, z) || world::riverMask(x, z) > 0.02f || world::pathDistance(x, z) < 4) return -1;
  float h = world::getTerrainHeight(x, z);
  float f = world::forestDensity(x, z);
  if (sp == SP_DEER) return (1 - f) * 1.2f + (h < 24 ? 0.5f : 0);
  if (sp == SP_BOAR) return f * 1.4f + (h < 22 ? 0.3f : 0);
  float far = hypot2(x, z) > 95 ? 0 : -3;
  if (sp == SP_WOLF) return far + f * 0.8f + (1 - f) * 0.4f + (h > 14 ? 0.4f : 0);
  if (sp == SP_BEAR) return far + f * 1.6f;
  if (sp == SP_LEOPARD) return far + (h > 20 ? 1 + h / 35 : h / 35) + f * 0.3f;
  return h > 18 ? 1 + h / 40 : h / 30;
}

Animal* makeAnimal(Species sp, Herd* herd, float x, float z, Rng* rnd) {
  auto r = [&] { return rnd ? rnd->next() : randf(); };
  auto a = std::make_unique<Animal>();
  Animal& A0 = *a;
  A0.id = nextId++;
  A0.sp = sp;
  A0.herd = herd;
  A0.x = x;
  A0.z = z;
  A0.y = world::getTerrainHeight(x, z);
  A0.yaw = r() * TAU;
  A0.speed = 0;
  A0.state = S_GRAZE;
  A0.t = 1 + r() * 6;
  A0.hp = SPEC[sp].hp;
  A0.tx = x;
  A0.tz = z;
  A0.phase = r() * 6;
  A0.pitch = SPEC[sp].grazePitch;
  A0.aware = 0; A0.fall = 0; A0.downT = 0;
  A0.steerYaw = A0.yaw;
  A0.steerT = 0;
  A0.charged = false;
  A0.slow = 0; A0.cd = 0;
  A0.prey = nullptr;
  A0.leap = 0; A0.rear = 0;
  A0.hunger = 20 + r() * 60;
  A0.lunge = 0;
  A0.raid = false;
  Animal* p = a.get();
  animals.push_back(std::move(a));
  herd->members.push_back(p);
  return p;
}

const float PROBES[11] = {0, 0.45f, -0.45f, 0.9f, -0.9f, 1.4f, -1.4f, 2.0f, -2.0f, 2.7f, -2.7f};

float steer(Animal* a, float want, float dt) {
  a->steerT -= dt;
  if (a->steerT <= 0) {
    a->steerT = 0.25f;
    float look = 2.5f + a->speed * 0.45f;
    a->steerYaw = want + PI;
    for (float off : PROBES) {
      float ang = want + off;
      if (walkable(a->x + std::sin(ang) * look, a->z + std::cos(ang) * look)) {
        a->steerYaw = ang;
        break;
      }
    }
  }
  return a->steerYaw;
}

void panicHerd(Herd* h, float fx, float fz) {
  for (Animal* m : h->members) {
    if (m->state == S_DOWNED) continue;
    if (m->state != S_CHARGE) {
      m->state = S_FLEE;
      m->t = 5 + randf() * 3;
    }
    m->aware = 1.2f;
    m->tx = fx;
    m->tz = fz;
  }
  h->calmT = 0;
}

float hearingRadius(Species sp) {
  auto& p = E.player;
  const Hear& H = SPEC[sp].hear;
  float r = p.sprinting ? H.sprint : p.moving ? (p.crouching ? H.crouch : H.walk) : p.crouching ? H.crouch * 0.6f : H.still;
  if (E.isNight) r *= 0.75f;
  return r;
}

void updatePredator(Animal* a, float dt);

void updateAnimal(Animal* a, float dt) {
  if (PRED(a->sp)) { updatePredator(a, dt); return; }
  const Spec& S = SPEC[a->sp];
  const Vec3& p = E.player.pos;
  if (a->state == S_DOWNED) {
    a->fall = std::min(1.0f, a->fall + dt * 2.2f);
    a->speed = 0;
    a->downT += dt;
    return;
  }
  float dx = p.x - a->x, dz = p.z - a->z;
  float d = hypot2(dx, dz);
  float toPlayer = std::atan2(dx, dz);
  float R = hearingRadius(a->sp);
  float gain = d < R ? (1 - d / R) * 2.4f : -0.3f;
  auto& n = E.noise;
  if (E.clock - n.t < 0.15f && hypot2(n.x - a->x, n.z - a->z) < n.r) gain += 6;
  a->aware = clampf(a->aware + gain * dt, 0, 1.2f);
  switch (a->state) {
    case S_GRAZE: {
      a->speed = std::max(0.0f, a->speed - dt * 3);
      a->pitch += (S.grazePitch - a->pitch) * std::min(1.0f, dt * 3);
      a->t -= dt;
      if (a->t <= 0) {
        float ang = randf() * TAU, r = 3 + randf() * 9;
        a->tx = a->herd->ax + std::cos(ang) * r;
        a->tz = a->herd->az + std::sin(ang) * r;
        a->state = S_WALK;
        a->t = 12;
      }
      break;
    }
    case S_WALK: {
      a->speed += (S.walk - a->speed) * std::min(1.0f, dt * 2);
      a->pitch += (0.15f - a->pitch) * std::min(1.0f, dt * 3);
      float want = std::atan2(a->tx - a->x, a->tz - a->z);
      a->yaw = turnToward(a->yaw, steer(a, want, dt), dt * 2.2f);
      a->t -= dt;
      if (hypot2(a->tx - a->x, a->tz - a->z) < 1.2f || a->t <= 0) {
        a->state = S_GRAZE;
        a->t = 3 + randf() * 8;
      }
      break;
    }
    case S_ALERT: {
      a->speed = std::max(0.0f, a->speed - dt * 5);
      a->pitch += (-0.25f - a->pitch) * std::min(1.0f, dt * 6);
      a->yaw = turnToward(a->yaw, toPlayer, dt * 3);
      if (a->aware >= 1) panicHerd(a->herd, p.x, p.z);
      else if (a->aware < 0.15f) {
        a->state = S_GRAZE;
        a->t = 2 + randf() * 4;
      }
      break;
    }
    case S_FLEE: {
      a->speed += (S.run - a->speed) * std::min(1.0f, dt * 3);
      a->pitch += (-0.1f - a->pitch) * std::min(1.0f, dt * 6);
      float away = std::atan2(a->x - a->tx, a->z - a->tz) + std::sin(E.clock * 1.7f + a->id) * 0.35f;
      a->yaw = turnToward(a->yaw, steer(a, away, dt), dt * 4.5f);
      a->t -= dt;
      if (a->t <= 0 && d > 55) {
        a->state = S_WALK;
        a->aware = 0.3f;
        a->t = 12;
        a->herd->calmT = 0.01f;
      }
      break;
    }
    case S_CHARGE: {
      a->speed += (8.5f - a->speed) * std::min(1.0f, dt * 4);
      a->pitch = 0.25f;
      a->yaw = turnToward(a->yaw, toPlayer, dt * 3.5f);
      a->t -= dt;
      if (d < 1.5f) {
        float dd = std::max(d, 0.1f);
        E.player.knock = Vec3(-dx / dd, 0, -dz / dd) * -11.0f;
        HudEvent e;
        e.kind = HudEvent::Knock;
        emitEvent(e);
        actions::notify("The boar slammed into you! Back off and line up another shot.");
        a->state = S_FLEE;
        a->tx = p.x; a->tz = p.z; a->t = 6;
      } else if (a->t <= 0) {
        a->state = S_FLEE;
        a->tx = p.x; a->tz = p.z; a->t = 5;
      }
      break;
    }
  }
  if ((a->state == S_GRAZE || a->state == S_WALK) && a->aware > 0.4f) {
    a->state = S_ALERT;
    a->t = 0;
  }
  for (Animal* m : a->herd->members) {
    if (m == a || m->state == S_DOWNED) continue;
    float sx = a->x - m->x, sz = a->z - m->z;
    float s2 = sx * sx + sz * sz;
    if (s2 < 2.6f && s2 > 1e-4f) {
      float s = std::sqrt(s2);
      a->x += (sx / s) * (1.6f - s) * dt * 2;
      a->z += (sz / s) * (1.6f - s) * dt * 2;
    }
  }
  a->x += std::sin(a->yaw) * a->speed * dt;
  a->z += std::cos(a->yaw) * a->speed * dt;
  if (d < 140) forest::resolveTreeCollision(a->x, a->z, 0.35f);
  a->x = clampf(a->x, -MAP_HALF + 6, MAP_HALF - 6);
  a->z = clampf(a->z, -MAP_HALF + 6, MAP_HALF - 6);
  a->y = world::getTerrainHeight(a->x, a->z);
  a->phase += dt * (0.6f + a->speed) * S.gait;
}

void playerDown(Animal* a) {
  auto& P = E.player;
  P.hp = P.maxHp * 0.6f;
  P.hurtT = E.clock;
  P.pos = {0, 1.7f, 9};
  P.knock = {0, 0, 0};
  P.velY = 0;
  for (auto& m : animals) {
    if (!PRED(m->sp) || m->state == S_DOWNED) continue;
    m->state = S_WALK; m->t = 12; m->tx = m->herd->ax; m->tz = m->herd->az; m->aware = 0;
  }
  popup("Overpowered", rgbHex("#f87171"));
  std::string nm = SPEC[a->sp].name;
  for (auto& c : nm) c = (char)std::tolower((unsigned char)c);
  actions::notify("The " + nm + " overpowered you. Villagers carried you back to the hearth. Heal up, crouch past predators, or bring friends of the arrow kind.");
}

void hurtPlayer(Animal* a, float dmg) {
  auto& P = E.player;
  const Vec3& p = P.pos;
  float dx = p.x - a->x, dz = p.z - a->z;
  float d = std::max(0.1f, hypot2(dx, dz));
  if (E.world.gearArmour) dmg = std::max(1.0f, std::round(dmg * 0.6f));
  P.hp = std::max(0.0f, P.hp - dmg);
  P.hurtT = E.clock;
  P.knock = Vec3(dx / d, 0, dz / d) * (a->sp == SP_BEAR ? 9.0f : 5.0f);
  HudEvent e;
  e.kind = HudEvent::Hurt;
  e.amount = dmg;
  emitEvent(e);
  float g = PRED(a->sp)->growl;
  audio::tone(Wave::Sawtooth, g * 1.7f, g, 0.28f, 0.13f);
  if (P.hp <= 0) playerDown(a);
}

void growl(Animal* a) {
  const Vec3& p = E.player.pos;
  if (hypot2(p.x - a->x, p.z - a->z) > 35) return;
  float g = PRED(a->sp)->growl;
  audio::tone(Wave::Sawtooth, g, g * 0.7f, a->sp == SP_BEAR ? 0.7f : 0.45f, 0.09f);
}

void alertPack(Animal* a, int state) {
  for (Animal* m : a->herd->members) {
    if (m == a || m->state == S_DOWNED || m->state == S_RETREAT) continue;
    if (m->state == S_CHASE || m->state == S_ATTACK || m->state == S_POUNCE) continue;
    if (hypot2(m->x - a->x, m->z - a->z) > 45) continue;
    m->state = state;
    m->t = state == S_CHASE ? 18 : 10;
  }
}

void moveToward(Animal* a, float tx, float tz, float speed, float dt, float turn) {
  float want = std::atan2(tx - a->x, tz - a->z);
  a->yaw = turnToward(a->yaw, steer(a, want, dt), dt * turn);
  a->speed += (speed - a->speed) * std::min(1.0f, dt * 3);
}

Animal* findPrey(Animal* a) {
  Animal* best = nullptr;
  float bd = 45 * 45;
  for (auto& m : animals) {
    if (PRED(m->sp) || m->state == S_DOWNED) continue;
    if (a->sp == SP_BEAR && m->sp != SP_DEER && m->sp != SP_BOAR) continue;
    float d2 = (m->x - a->x) * (m->x - a->x) + (m->z - a->z) * (m->z - a->z);
    if (d2 < bd && hypot2(m->x, m->z) > VILLAGE_SAFE) { bd = d2; best = m.get(); }
  }
  return best;
}

std::string lowerName(Species sp) {
  std::string n = SPEC[sp].name;
  for (auto& c : n) c = (char)std::tolower((unsigned char)c);
  return n;
}

void endRaid(bool stole);

void updatePredator(Animal* a, float dt) {
  const Spec& S = SPEC[a->sp];
  const PredDef& D = *PRED(a->sp);
  auto& P = E.player;
  const Vec3& p = P.pos;
  if (a->state == S_DOWNED) {
    a->fall = std::min(1.0f, a->fall + dt * 2.2f);
    a->speed = 0;
    a->leap = 0;
    a->rear = std::max(0.0f, a->rear - dt * 3);
    a->downT += dt;
    return;
  }
  a->cd = std::max(0.0f, a->cd - dt);
  a->lunge = std::max(0.0f, a->lunge - dt * 3);
  a->rear += ((a->state == S_ALERT ? 1.0f : 0.0f) - a->rear) * std::min(1.0f, dt * 4);
  float dx = p.x - a->x, dz = p.z - a->z;
  float d = hypot2(dx, dz);
  float toPlayer = std::atan2(dx, dz);
  bool safe = !a->raid && hypot2(p.x, p.z) < VILLAGE_SAFE - 6;
  if (a->raid && a->state != S_RAID && a->state != S_CHASE && a->state != S_ATTACK && a->state != S_STALK) a->raid = false;
  float sight = D.sight * (P.crouching ? 0.6f : P.sprinting ? 1.3f : 1) * (E.isNight ? 1.3f : 1);
  if (a->state == S_FEED) sight *= 0.75f;
  auto& n = E.noise;
  bool heardShot = E.clock - n.t < 0.15f && hypot2(n.x - a->x, n.z - a->z) < n.r * 0.8f;
  bool noticed = !safe && (d < sight || (heardShot && d < sight * 2));
  switch (a->state) {
    case S_GRAZE:
    case S_WALK:
    case S_FLEE: {
      if (a->state == S_GRAZE) {
        a->speed = std::max(0.0f, a->speed - dt * 3);
        a->pitch += (S.grazePitch - a->pitch) * std::min(1.0f, dt * 3);
        a->t -= dt;
        if (a->t <= 0) {
          float ang = randf() * TAU, r = 6 + randf() * 22;
          a->tx = a->herd->ax + std::cos(ang) * r;
          a->tz = a->herd->az + std::sin(ang) * r;
          a->state = S_WALK;
          a->t = 16;
        }
      } else {
        a->pitch += (0.1f - a->pitch) * std::min(1.0f, dt * 3);
        moveToward(a, a->tx, a->tz, S.walk, dt, 2);
        a->t -= dt;
        if (hypot2(a->tx - a->x, a->tz - a->z) < 1.5f || a->t <= 0) { a->state = S_GRAZE; a->t = 3 + randf() * 7; }
      }
      a->hunger -= dt;
      if (a->hunger <= 0) {
        a->hunger = 25;
        Animal* prey = findPrey(a);
        if (prey) { a->prey = prey; a->state = S_HUNT; a->t = 16; a->hunger = 100 + randf() * 80; }
      }
      if (noticed) {
        if (a->sp == SP_BEAR) { a->state = S_ALERT; a->t = 1.6f; growl(a); }
        else { a->state = S_STALK; a->t = 14; if (a->sp == SP_WOLF) { alertPack(a, S_STALK); growl(a); } }
      }
      break;
    }
    case S_ALERT: {
      a->speed = std::max(0.0f, a->speed - dt * 6);
      a->yaw = turnToward(a->yaw, toPlayer, dt * 3);
      a->pitch += (-0.35f - a->pitch) * std::min(1.0f, dt * 5);
      a->t -= dt;
      if (a->t <= 0) {
        if (!safe && d < sight * 0.95f) { a->state = S_CHASE; a->t = 12; actions::notify("A " + lowerName(a->sp) + " is charging! Shoot or get back to the village."); }
        else { a->state = S_WALK; a->t = 12; a->tx = a->herd->ax; a->tz = a->herd->az; }
      }
      break;
    }
    case S_STALK: {
      a->pitch += (0.35f - a->pitch) * std::min(1.0f, dt * 4);
      moveToward(a, p.x, p.z, S.walk * 1.25f, dt, 2.5f);
      a->t -= dt;
      float strike = D.pounce > 0 ? D.pounce : 13;
      if (safe || a->t <= 0 || d > sight * 1.7f) { a->state = S_WALK; a->t = 12; a->tx = a->herd->ax; a->tz = a->herd->az; }
      else if (d < strike) {
        if (D.pounce > 0) {
          a->state = S_POUNCE;
          a->t = std::max(0.35f, d / 13);
          a->tx = p.x; a->tz = p.z;
          a->aware = a->t;
          growl(a);
        } else {
          a->state = S_CHASE;
          a->t = 18;
          alertPack(a, S_CHASE);
        }
      }
      break;
    }
    case S_POUNCE: {
      a->yaw = turnToward(a->yaw, std::atan2(a->tx - a->x, a->tz - a->z), dt * 6);
      a->speed = 13;
      a->pitch = -0.25f;
      a->t -= dt;
      float k = 1 - std::max(0.0f, a->t) / std::max(0.01f, a->aware);
      a->leap = std::sin(std::min(1.0f, k) * PI) * 1.1f;
      if (a->t <= 0) {
        a->leap = 0;
        a->speed = 2;
        if (d < 2.4f) { a->lunge = 1; hurtPlayer(a, std::round(D.dmg * 1.5f)); a->cd = D.cd; }
        a->state = S_CHASE;
        a->t = 7;
      }
      break;
    }
    case S_CHASE: {
      a->pitch += (-0.05f - a->pitch) * std::min(1.0f, dt * 5);
      a->t -= dt;
      if (safe || a->t <= 0 || d > sight * 2.4f) {
        a->state = S_WALK; a->t = 14; a->tx = a->herd->ax; a->tz = a->herd->az;
        break;
      }
      if (d < D.reach) {
        a->speed = std::max(0.0f, a->speed - dt * 10);
        a->yaw = turnToward(a->yaw, toPlayer, dt * 6);
        if (a->cd <= 0) { a->state = S_ATTACK; a->aware = D.windup; }
      } else moveToward(a, p.x, p.z, S.run * (d < 6 ? 0.75f : 1), dt, 4.5f);
      break;
    }
    case S_ATTACK: {
      a->speed = std::max(0.0f, a->speed - dt * 10);
      a->yaw = turnToward(a->yaw, toPlayer, dt * 7);
      a->aware -= dt;
      a->pitch = -0.2f - std::min(1.0f, (D.windup - a->aware) / D.windup) * 0.3f;
      if (a->aware <= 0) {
        a->lunge = 1;
        if (d < D.reach + 0.8f) hurtPlayer(a, D.dmg);
        a->cd = D.cd * (0.85f + randf() * 0.3f);
        a->state = S_CHASE;
        a->t = std::max(a->t, 8.0f);
      }
      break;
    }
    case S_RETREAT: {
      a->pitch += (0.2f - a->pitch) * std::min(1.0f, dt * 5);
      float away = std::atan2(a->x - p.x, a->z - p.z);
      a->yaw = turnToward(a->yaw, steer(a, away, dt), dt * 4);
      a->speed += (S.run * 0.85f - a->speed) * std::min(1.0f, dt * 3);
      a->t -= dt;
      a->hp = std::min(S.hp, a->hp + dt * 2);
      if (a->t <= 0 && d > 45) { a->state = S_WALK; a->t = 14; a->herd->calmT = 0.01f; }
      break;
    }
    case S_HUNT: {
      Animal* q = a->prey;
      a->t -= dt;
      if (!q || q->state == S_DOWNED || a->t <= 0) { a->prey = nullptr; a->state = S_WALK; a->t = 10; a->tx = a->herd->ax; a->tz = a->herd->az; break; }
      float pd = hypot2(q->x - a->x, q->z - a->z);
      a->pitch += (0.15f - a->pitch) * std::min(1.0f, dt * 4);
      moveToward(a, q->x + std::sin(q->yaw) * q->speed * 0.4f, q->z + std::cos(q->yaw) * q->speed * 0.4f, pd > 20 ? S.walk * 1.8f : S.run * 1.05f, dt, 4);
      if (pd < 25 && q->state != S_FLEE) panicHerd(q->herd, a->x, a->z);
      if (pd < 1.6f) {
        q->hp = 0; q->state = S_DOWNED; q->speed = 0; q->downT = 0;
        a->prey = nullptr;
        a->state = S_FEED;
        a->t = 25;
        a->tx = q->x; a->tz = q->z;
      }
      if (noticed && d < sight * 0.6f) { a->prey = nullptr; a->state = a->sp == SP_BEAR ? S_ALERT : S_STALK; a->t = a->sp == SP_BEAR ? 1.4f : 10; }
      break;
    }
    case S_FEED: {
      a->speed = std::max(0.0f, a->speed - dt * 6);
      a->pitch += (0.95f - a->pitch) * std::min(1.0f, dt * 3);
      a->t -= dt;
      if (noticed) { a->state = a->sp == SP_BEAR ? S_ALERT : S_CHASE; a->t = a->sp == SP_BEAR ? 1.2f : 12; growl(a); if (a->sp == SP_WOLF) alertPack(a, S_CHASE); }
      else if (a->t <= 0) { a->state = S_WALK; a->t = 12; a->tx = a->herd->ax; a->tz = a->herd->az; }
      break;
    }
    case S_RAID: {
      a->pitch += (0.2f - a->pitch) * std::min(1.0f, dt * 4);
      float td = hypot2(a->tx - a->x, a->tz - a->z);
      if (td > 3.5f) moveToward(a, a->tx, a->tz, td > 25 ? S.run * 0.7f : S.walk * 1.6f, dt, 3);
      else { a->speed = std::max(0.0f, a->speed - dt * 6); a->t -= dt; a->pitch = 0.8f; }
      if (raidRepelled && hypot2(a->x, a->z) < 58) { endRaid(false); break; }
      if (a->t <= 0) { endRaid(true); break; }
      if (noticed && d < sight * 0.8f) { a->state = S_CHASE; a->t = 14; growl(a); }
      E.raid.x = a->x; E.raid.z = a->z;
      break;
    }
    default:
      a->state = S_WALK;
  }
  for (Animal* m : a->herd->members) {
    if (m == a || m->state == S_DOWNED) continue;
    float sx = a->x - m->x, sz = a->z - m->z;
    float s2 = sx * sx + sz * sz;
    if (s2 < 2.2f && s2 > 1e-4f) {
      float s = std::sqrt(s2);
      a->x += (sx / s) * (1.5f - s) * dt * 2;
      a->z += (sz / s) * (1.5f - s) * dt * 2;
    }
  }
  if (a->state == S_CHASE || a->state == S_ATTACK) {
    for (Animal* m : a->herd->members) {
      if (m == a || (m->state != S_CHASE && m->state != S_ATTACK)) continue;
      float ox = m->x - p.x, oz = m->z - p.z, ax = a->x - p.x, az = a->z - p.z;
      if (ox * ox + oz * oz < 9 && ax * ax + az * az < 9) a->yaw += dt * 1.2f;
    }
  }
  a->x += std::sin(a->yaw) * a->speed * dt;
  a->z += std::cos(a->yaw) * a->speed * dt;
  if (d < 140 && a->state != S_POUNCE) forest::resolveTreeCollision(a->x, a->z, a->sp == SP_BEAR ? 0.55f : 0.35f);
  float hd = hypot2(a->x, a->z);
  if (hd < VILLAGE_SAFE && !a->raid && hd > 1e-3f) { a->x *= VILLAGE_SAFE / hd; a->z *= VILLAGE_SAFE / hd; }
  float near = a->sp == SP_BEAR ? 1.8f : 1.35f;
  if (d < near) { a->x -= (dx / std::max(d, 0.1f)) * (near - d); a->z -= (dz / std::max(d, 0.1f)) * (near - d); }
  a->x = clampf(a->x, -MAP_HALF + 6, MAP_HALF - 6);
  a->z = clampf(a->z, -MAP_HALF + 6, MAP_HALF - 6);
  a->y = world::getTerrainHeight(a->x, a->z);
  a->phase += dt * (0.6f + a->speed) * S.gait;
}

void raidTarget(float& tx, float& tz) {
  for (auto& s : A.structures)
    if (s.kind == SK_FOOD_STORE) { tx = s.x; tz = s.z; return; }
  for (auto& s : A.structures)
    if (s.kind == SK_ANIMAL_SHELTER) { tx = s.x; tz = s.z; return; }
  tx = 2;
  tz = 2;
}

void startRaid() {
  if (A.structures.empty() && A.population == 0) return;
  Herd* best = nullptr;
  float bd = 1e30f;
  for (auto& h : herds) {
    if (h->sp != SP_WOLF) continue;
    int live = 0;
    for (Animal* m : h->members) if (m->state != S_DOWNED) live++;
    if (live < 2) continue;
    float d = hypot2(h->ax, h->az);
    if (d < bd) { bd = d; best = h.get(); }
  }
  if (!best) return;
  raidRepelled = villageDefense() >= SAFE_DEFENSE;
  float tx, tz;
  raidTarget(tx, tz);
  raidPack.clear();
  for (Animal* m : best->members) if (m->state != S_DOWNED) raidPack.push_back(m);
  float sx = (best->ax >= 0 ? 1 : -1) * 70.0f, sz = (best->az >= 0 ? 1 : -1) * 70.0f;
  for (Animal* m : raidPack) {
    m->x = sx + (randf() - 0.5f) * 8; m->z = sz + (randf() - 0.5f) * 8; m->y = world::getTerrainHeight(m->x, m->z);
    m->state = S_RAID; m->raid = true; m->t = 9; m->prey = nullptr;
    m->tx = tx + (randf() - 0.5f) * 3; m->tz = tz + (randf() - 0.5f) * 3;
  }
  E.raid.active = true;
  E.raid.x = sx;
  E.raid.z = sz;
  ambience::howl(hypot2(E.player.pos.x - sx, E.player.pos.z - sz));
  actions::notify(raidRepelled ? "Wolves are howling at the edge of the village. Your lanterns and watch should keep them back."
                               : "A wolf pack is raiding the village stores! Defend the food — more lantern posts, fences and a watchtower will keep them away.");
}

void endRaid(bool stole) {
  int n = 0;
  for (Animal* m : raidPack) if (m->state != S_DOWNED) n++;
  for (Animal* m : raidPack) {
    if (m->state == S_DOWNED) continue;
    m->raid = false; m->state = S_RETREAT; m->t = 12;
  }
  raidPack.clear();
  E.raid.active = false;
  if (stole) {
    int food = invCount("food");
    int take = std::min(food, 6 + n * 3);
    if (take > 0) actions::addItems({{"food", -take}});
    actions::notify(take > 0 ? "The wolves ran off with " + std::to_string(take) + " food. Build lantern posts, fences or a watchtower to protect the village." : "The wolves found nothing to steal and slunk away.");
  } else actions::notify("The wolves saw the lanterns and turned back. The village is well defended.");
}

void nightEvents(float dt) {
  float h = E.time;
  if (lastHour >= 0 && lastHour < 22 && h >= 22 && !E.raid.active) {
    float chance = season.snow > 0.5f ? 0.7f : season.autumn > 0.6f ? 0.3f : 0.12f;
    if (randf() < chance) startRaid();
  }
  lastHour = h;
  if (E.raid.active) {
    bool done = true;
    int killed = 0;
    for (Animal* m : raidPack) {
      if (m->state == S_DOWNED) killed++;
      else if (m->raid) done = false;
    }
    if (done) {
      raidPack.clear();
      E.raid.active = false;
      if (killed) actions::notify("You drove off the raiding wolves!");
    }
  }
  auto& P = E.player;
  const Vec3& p = P.pos;
  if (season.snow > 0.55f && E.daylight < 0.3f && !E.world.gearCoat && hypot2(p.x, p.z) > VILLAGE_SAFE && E.playing && !E.uiOpen) {
    if (P.hp > 25) { P.hp -= dt * 0.8f; P.hurtT = E.clock - 4; }
    coldWarned -= dt;
    if (coldWarned <= 0) {
      coldWarned = 90;
      actions::notify("You are freezing. Head back to the village, or craft a Fur Coat from wolf fur.");
    }
  }
}

void removeAnimal(Animal* a) {
  for (auto& m : animals)
    if (m->prey == a) m->prey = nullptr;
  raidPack.erase(std::remove(raidPack.begin(), raidPack.end(), a), raidPack.end());
  auto& mem = a->herd->members;
  mem.erase(std::remove(mem.begin(), mem.end(), a), mem.end());
  animals.erase(std::remove_if(animals.begin(), animals.end(), [a](const std::unique_ptr<Animal>& p) { return p.get() == a; }), animals.end());
}

void migrate(Herd* h, float dt) {
  const Vec3& p = E.player.pos;
  bool busy = false;
  for (Animal* m : h->members) if (m->state != S_DOWNED && m->aware > 0.5f) busy = true;
  if (!h->migrating) {
    h->migT -= dt;
    if (h->migT > 0 || busy) return;
    float bs = -1;
    for (int k = 0; k < 24; k++) {
      float ang = randf() * TAU, r = 50 + randf() * 110;
      float x = h->ax + std::cos(ang) * r, z = h->az + std::sin(ang) * r;
      if (std::fabs(x) > MAP_HALF - 30 || std::fabs(z) > MAP_HALF - 30) continue;
      if (hypot2(x, z) < (PRED(h->sp) ? 110.0f : 45.0f)) continue;
      float s = habitatScore(h->sp, x, z);
      if (PRED(h->sp)) {
        for (auto& o : herds)
          if (!PRED(o->sp) && hypot2(o->ax - x, o->az - z) < 60) s += 1.5f;
      } else
        for (auto& o : herds)
          if (PRED(o->sp) && hypot2(o->ax - x, o->az - z) < 70) s -= 2;
      if (s > bs) { bs = s; h->tx = x; h->tz = z; }
    }
    if (bs < 0) { h->migT = 30; return; }
    h->migrating = true;
    return;
  }
  if (busy) return;
  float dx = h->tx - h->ax, dz = h->tz - h->az, d = hypot2(dx, dz);
  float sp = hypot2(p.x - h->ax, p.z - h->az) < 60 ? 0.35f : 0.9f;
  if (d < 2) { h->migrating = false; h->migT = 120 + randf() * 240; return; }
  float step = std::min(d, sp * dt);
  h->ax += dx / d * step;
  h->az += dz / d * step;
}

void updateHerds(float dt) {
  const Vec3& p = E.player.pos;
  for (auto& h : herds) {
    migrate(h.get(), dt);
    if (h->calmT > 0) {
      float cx = 0, cz = 0;
      int n = 0;
      for (Animal* m : h->members) if (m->state != S_DOWNED) { cx += m->x; cz += m->z; n++; }
      if (n) { h->ax = cx / n; h->az = cz / n; }
      h->calmT = 0;
    }
    std::vector<Animal*> stale;
    for (Animal* m : h->members) if (m->state == S_DOWNED && m->downT > 180) stale.push_back(m);
    for (Animal* m : stale) removeAnimal(m);
    int alive = 0;
    for (Animal* m : h->members) if (m->state != S_DOWNED) alive++;
    if (alive < h->size) {
      h->respawnT += dt;
      if (h->respawnT > 180 && hypot2(p.x - h->ax, p.z - h->az) > 90) {
        h->respawnT = 0;
        makeAnimal(h->sp, h.get(), h->ax + (randf() - 0.5f) * 8, h->az + (randf() - 0.5f) * 8, nullptr);
      }
    }
  }
}

float segSphere(const Vec3& p0, const Vec3& p1, const Vec3& c, float r) {
  Vec3 seg = p1 - p0;
  float len2 = std::max(seg.lengthSq(), 1e-6f);
  float t = clampf((c - p0).dot(seg) / len2, 0, 1);
  Vec3 q = p0 + seg * t;
  return q.distanceSq(c) <= r * r;
}

struct Batches { DynBatch body, head, upperF, lowerF, upperH, lowerH, footF, footH; };
Batches g_batch[SP_COUNT * 2];
const float W_MID = 26, W_BAND = 8;
Batches g_shadowBatch[SP_COUNT];
const float LEG_OFFSETS[4] = {0, PI, PI, 0};

}  // namespace

namespace wildlife {

const char* speciesName(Species sp) { return SPEC[sp].name; }
bool isPredator(Species sp) { return PRED(sp) != nullptr; }
int animalCount() { return (int)animals.size(); }

void spawn(uint32_t seed) {
  raidPack.clear();
  raidRepelled = false;
  herds.clear();
  animals.clear();
  Rng rnd(seed);
  std::vector<Species> order;
  for (int sp = 0; sp < SP_COUNT; sp++)
    for (int i = 0; i < SPEC[sp].groups * 3; i++) order.push_back((Species)sp);
  for (size_t gi = 0; gi < order.size(); gi++) {
    Species sp = order[gi];
    bool tutorial = gi == 0;
    float bs = -1, bx = 0, bz = 0;
    for (int k = 0; k < 160; k++) {
      float ang = rnd.next() * TAU;
      float r = tutorial ? 42 + rnd.next() * 30 : (PRED(sp) ? 100.0f : 55.0f) + rnd.next() * (gi < 30 ? 170 : 470);
      if (std::fabs(std::cos(ang) * r) > MAP_HALF - 30 || std::fabs(std::sin(ang) * r) > MAP_HALF - 30) continue;
      float x = std::cos(ang) * r, z = std::sin(ang) * r;
      float s = habitatScore(sp, x, z);
      if (s < 0) continue;
      for (auto& h : herds) if (hypot2(h->ax - x, h->az - z) < 45) s -= 2;
      if (s > bs) { bs = s; bx = x; bz = z; }
    }
    if (bs < 0) continue;
    auto herd = std::make_unique<Herd>();
    herd->sp = sp;
    herd->ax = bx;
    herd->az = bz;
    herd->migT = 60 + rnd.next() * 180;
    herd->size = SPEC[sp].herdLo + (int)std::floor(rnd.next() * (SPEC[sp].herdHi - SPEC[sp].herdLo + 1));
    for (int i = 0; i < herd->size; i++) {
      float ox = (rnd.next() - 0.5f) * 8, oz = (rnd.next() - 0.5f) * 8;
      makeAnimal(sp, herd.get(), herd->ax + ox, herd->az + oz, &rnd);
    }
    herds.push_back(std::move(herd));
  }
}

void update(float dt) {
  const Vec3& p = E.player.pos;
  auto& P = E.player;
  nightEvents(dt);
  if (P.hp < P.maxHp && E.clock - P.hurtT > 6) P.hp = std::min(P.maxHp, P.hp + dt * (hypot2(p.x, p.z) < VILLAGE_SAFE ? 10 : 2.5f));
  bool paused = E.uiOpen || !E.playing;
  for (size_t i = 0; i < animals.size(); i++) {
    Animal* a = animals[i].get();
    if (paused && PRED(a->sp)) continue;
    bool far = std::fabs(a->x - p.x) + std::fabs(a->z - p.z) > 220;
    if (far) {
      a->slow += dt;
      if (a->slow < 0.3f) continue;
      updateAnimal(a, a->slow);
      a->slow = 0;
    } else updateAnimal(a, dt);
  }
  updateHerds(dt);
}

bool arrowHitTest(const Vec3& p0, const Vec3& p1, ArrowHit& out) {
  for (auto& ap : animals) {
    Animal* a = ap.get();
    if (a->state == S_DOWNED) continue;
    if (std::fabs(a->x - p1.x) > 6 || std::fabs(a->z - p1.z) > 6) continue;
    const SpeciesModel& M = models::species(a->sp);
    float sy = std::sin(a->yaw), cy = std::cos(a->yaw);
    float lift = a->leap + a->rear * 0.4f;
    const Vec3& hp = M.headPivot;
    float hz = hp.z + std::cos(a->pitch) * M.hitHead.z + std::sin(a->pitch) * M.hitHead.y * 0.2f;
    float hy = hp.y + std::cos(a->pitch) * M.hitHead.y - std::sin(a->pitch) * M.hitHead.z;
    Vec3 hc(a->x + sy * hz, a->y + hy + lift, a->z + cy * hz);
    if (segSphere(p0, p1, hc, M.hitHead.r)) { out = {a, true}; return true; }
    for (int k = -1; k <= 1; k++) {
      float zo = k * M.hitBody.zOff;
      Vec3 c(a->x + sy * zo, a->y + M.hitBody.y + lift, a->z + cy * zo);
      if (segSphere(p0, p1, c, M.hitBody.r)) { out = {a, false}; return true; }
    }
  }
  return false;
}

void damage(Animal* a, float dmg, bool headshot) {
  const Spec& S = SPEC[a->sp];
  a->hp -= headshot ? dmg * 2 : dmg;
  const Vec3& p = E.player.pos;
  const PredDef* D = PRED(a->sp);
  HudEvent ev;
  ev.kind = HudEvent::Hit;
  ev.headshot = headshot;
  if (D) {
    if (a->hp <= 0) {
      a->state = S_DOWNED; a->speed = 0; a->downT = 0; a->leap = 0; a->prey = nullptr;
      ev.kill = true;
      emitEvent(ev);
      actions::notify(std::string(headshot ? "Clean headshot! " : "") + "The " + lowerName(a->sp) + " is down. Press [E] to harvest a prized trophy.");
      actions::addScore((int)std::round(D->score * (headshot ? 1.3f : 1)));
      std::vector<Animal*> rest;
      for (Animal* m : a->herd->members) if (m->state != S_DOWNED) rest.push_back(m);
      for (Animal* m : rest) { m->state = rest.size() <= 1 ? S_RETREAT : S_CHASE; m->t = rest.size() <= 1 ? 10.0f : 15.0f; }
      return;
    }
    emitEvent(ev);
    growl(a);
    if (a->hp < S.hp * 0.3f && a->sp != SP_BEAR) {
      a->state = S_RETREAT; a->t = 10; a->leap = 0;
      actions::notify("The wounded " + lowerName(a->sp) + " is retreating.");
    } else if (a->state != S_ATTACK && a->state != S_POUNCE) {
      a->state = S_CHASE; a->t = 16; a->prey = nullptr;
      if (a->sp == SP_WOLF) alertPack(a, S_CHASE);
      actions::notify(a->sp == SP_BEAR ? "You have angered the bear! Keep shooting or run for the village." : "Hit! The " + lowerName(a->sp) + " is coming for you.");
    }
    return;
  }
  if (a->hp <= 0) {
    a->state = S_DOWNED;
    a->speed = 0;
    a->downT = 0;
    ev.kill = true;
    emitEvent(ev);
    actions::notify(std::string(headshot ? "Clean headshot! " : "") + S.name + " down — walk over and press [E] to harvest.");
    actions::addScore(headshot ? 150 : 100);
    panicHerd(a->herd, p.x, p.z);
    return;
  }
  emitEvent(ev);
  float d = hypot2(p.x - a->x, p.z - a->z);
  if (a->sp == SP_BOAR && !a->charged && d < 26 && randf() < 0.6f) {
    a->charged = true;
    a->state = S_CHARGE;
    a->t = 3;
    actions::notify("The wounded boar is charging you!");
    for (Animal* m : a->herd->members)
      if (m != a && m->state != S_DOWNED) { m->state = S_FLEE; m->t = 6; m->tx = p.x; m->tz = p.z; }
  } else {
    panicHerd(a->herd, p.x, p.z);
    actions::notify("Hit! The " + lowerName(a->sp) + " is wounded and fleeing — follow it.");
  }
}

Animal* findCarcass(float x, float z, float r) {
  Animal* best = nullptr;
  float bd = r * r;
  for (auto& a : animals) {
    if (a->state != S_DOWNED) continue;
    float d2 = (a->x - x) * (a->x - x) + (a->z - z) * (a->z - z);
    if (d2 < bd) { bd = d2; best = a.get(); }
  }
  return best;
}

CarcassYield harvestCarcass(Animal* a) {
  CarcassYield out;
  out.name = SPEC[a->sp].name;
  out.yields = SPEC[a->sp].yields;
  for (auto& y : out.yields)
    if (y.first == "deer_pelt" || y.first == "boar_hide") y.second = (int)std::round(y.second * E.world.bonus.peltMult);
  removeAnimal(a);
  return out;
}

bool nearestHerd(Species sp, float x, float z, float& ox, float& oz) {
  Herd* best = nullptr;
  float bd = 1e30f;
  for (auto& h : herds) {
    if (h->sp != sp) continue;
    bool live = false;
    for (Animal* m : h->members) if (m->state != S_DOWNED) live = true;
    if (!live) continue;
    float d = hypot2(h->ax - x, h->az - z);
    if (d < bd) { bd = d; best = h.get(); }
  }
  if (!best) return false;
  float cx = 0, cz = 0;
  int n = 0;
  for (Animal* m : best->members) if (m->state != S_DOWNED) { cx += m->x; cz += m->z; n++; }
  ox = cx / n;
  oz = cz / n;
  return true;
}

void initRender() {
  registerMarkers("wildlife", [](std::vector<Marker>& out) {
    const Vec3& p = E.player.pos;
    float r2 = E.world.bonus.animalRadar * E.world.bonus.animalRadar;
    for (auto& ap : animals) {
      Animal* a = ap.get();
      float d2 = (a->x - p.x) * (a->x - p.x) + (a->z - p.z) * (a->z - p.z);
      if (a->state == S_DOWNED) out.push_back({a->x, a->y + 1.4f, a->z, MI_HARVEST, std::string(SPEC[a->sp].name) + " — harvest"});
      else if (PRED(a->sp)) { if (d2 < 60 * 60 || d2 < r2) out.push_back({a->x, a->y + 1.8f, a->z, MI_DANGER, SPEC[a->sp].name}); }
      else if (d2 < r2) out.push_back({a->x, a->y + 1.6f, a->z, MI_ANIMAL, ""});
    }
  });
  for (int sp = 0; sp < SP_COUNT * 2; sp++) {
    const SpeciesModel& M = models::species(sp % SP_COUNT, sp >= SP_COUNT);
    for (Batches* B : {&g_batch[sp], sp >= SP_COUNT ? &g_shadowBatch[sp - SP_COUNT] : (Batches*)nullptr}) {
      if (!B) continue;
      B->body.init(&M.body);
      B->head.init(&M.head);
      B->upperF.init(&M.upperF);
      B->lowerF.init(&M.lowerF);
      B->upperH.init(&M.upperH);
      B->lowerH.init(&M.lowerH);
      B->footF.init(&M.footF);
      B->footH.init(&M.footH);
    }
  }
}

static Mat4 partMatrix(const Vec3& pivot, float pitch) { return Mat4::compose(pivot, Quat::axisAngle({1, 0, 0}, pitch), {1, 1, 1}); }

struct GaitDef { float duty; float off[4]; };
const GaitDef WALK = {0.64f, {0.75f, 0.25f, 0.5f, 0.0f}};
const GaitDef TROT = {0.46f, {0.0f, 0.5f, 0.5f, 0.0f}};
const GaitDef GALLOP = {0.32f, {0.55f, 0.45f, 0.1f, 0.0f}};
const GaitDef BOUND = {0.3f, {0.5f, 0.6f, 0.05f, 0.0f}};

static float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

static void footAt(const GaitDef& G, float cycle, int L, float amp, float lift, float& fz, float& fy, float& curl) {
  float u = cycle + G.off[L];
  u -= std::floor(u);
  if (u < G.duty) {
    float s = u / G.duty;
    fz = amp * (1 - 2 * s);
    fy = 0;
    curl = 0;
  } else {
    float s = (u - G.duty) / (1 - G.duty);
    fz = -amp + 2 * amp * smooth01(s);
    float k = std::sin(PI * s);
    fy = lift * k;
    curl = k;
  }
}

static void solveLeg(float l1, float l2, float tz, float ty, bool kneeForward, float& a1, float& a2) {
  float d = std::sqrt(tz * tz + ty * ty);
  d = clampf(d, std::fabs(l1 - l2) + 1e-3f, (l1 + l2) * 0.999f);
  float base = std::atan2(tz, -ty);
  float c = clampf((l1 * l1 + d * d - l2 * l2) / (2 * l1 * d), -1, 1);
  float off = std::acos(c);
  a1 = kneeForward ? base + off : base - off;
  float kz = std::sin(a1) * l1, ky = -std::cos(a1) * l1;
  a2 = std::atan2(tz - kz, -(ty - ky));
}

static Mat4 rotAt(const Vec3& p, float a) { return Mat4::compose(p, Quat::axisAngle({1, 0, 0}, -a), {1, 1, 1}); }

static void fill(bool shadow) {
  static float lastClock = 0;
  float dt = 0;
  if (!shadow) { dt = clampf(E.clock - lastClock, 0, 0.1f); lastClock = E.clock; }
  const Vec3& p = E.player.pos;
  Batches* B = shadow ? g_shadowBatch : g_batch;
  int NB = shadow ? SP_COUNT : SP_COUNT * 2;
  for (int sp = 0; sp < NB; sp++) {
    Batches& b = B[sp];
    for (DynBatch* d : {&b.body, &b.head, &b.upperF, &b.lowerF, &b.upperH, &b.lowerH, &b.footF, &b.footH}) d->clear();
  }
  for (auto& ap : animals) {
    Animal* a = ap.get();
    float v = a->state == S_DOWNED ? 0 : std::fabs(a->speed);
    if (!shadow) {
      a->gaitV += (v - a->gaitV) * std::min(1.0f, dt * 6);
      const SpeciesModel& M0 = models::species(a->sp);
      float legLen = M0.legPivots[0].y;
      float f = a->gaitV < 0.05f ? 0 : clampf((0.75f + 0.32f * a->gaitV) * std::sqrt(0.7f / legLen), 0.6f, 2.8f);
      a->gaitT += dt * f;
    }
    if (std::fabs(a->x - p.x) > 140 || std::fabs(a->z - p.z) > 140) continue;
    if (shadow && hypot2(a->x - p.x, a->z - p.z) > 46) continue;
    const SpeciesModel& M = models::species(a->sp);
    if (!shadow && !render::cam.sphere({a->x, a->y + 0.8f, a->z}, 2.2f)) continue;
    bool predator = a->sp == SP_WOLF || a->sp == SP_LEOPARD || a->sp == SP_BEAR;
    float sv = a->gaitV;
    float cyc = a->gaitT;
    float legLen = M.legPivots[0].y;
    float freq = sv < 0.05f ? 1 : clampf((0.75f + 0.32f * sv) * std::sqrt(0.7f / legLen), 0.6f, 2.8f);
    float stride = sv / freq;
    float wTrot = smooth01((sv - 2.0f) / 1.5f), wRun = smooth01((sv - 5.5f) / 2.0f);
    const GaitDef& fast = predator && a->sp != SP_BEAR ? BOUND : GALLOP;
    float duty = WALK.duty + (TROT.duty - WALK.duty) * wTrot + (fast.duty - TROT.duty) * wRun;
    float amp = std::min(stride * duty * 0.5f, legLen * (0.42f + 0.25f * wRun));
    float lift = legLen * (0.1f + 0.1f * wTrot + 0.12f * wRun) * smooth01(sv / 0.4f);

    float spine = 0, bob = 0, roll = 0;
    if (a->state != S_DOWNED && sv > 0.05f) {
      float t2 = cyc * TAU;
      bob = (std::sin(2 * t2) * 0.012f * (1 - wTrot) + std::fabs(std::sin(t2)) * 0.03f * wTrot * (1 - wRun) + std::sin(t2) * 0.06f * wRun) * legLen;
      spine = std::sin(t2 + 0.6f) * (0.02f + 0.07f * wRun);
      roll = std::sin(t2) * 0.025f * (1 - wRun);
    } else if (a->state != S_DOWNED) {
      bob = std::sin(E.clock * 1.6f + a->id) * 0.004f;
    }
    Quat q = Quat::axisAngle({0, 1, 0}, a->yaw);
    if (a->fall > 0) q = q * Quat::axisAngle({0, 0, 1}, (a->fall * PI) / 2);
    if (a->rear > 0.01f) q = q * Quat::axisAngle({1, 0, 0}, -a->rear * 0.75f);
    else if (a->leap > 0.01f) q = q * Quat::axisAngle({1, 0, 0}, -0.25f * signf(a->speed));
    q = q * Quat::axisAngle({1, 0, 0}, spine) * Quat::axisAngle({0, 0, 1}, roll);
    Mat4 base = Mat4::compose({a->x, a->y + bob + a->fall * M.shadowRadius * 0.35f + a->leap + a->rear * 0.25f, a->z}, q, {1, 1, 1});
    float dCam = hypot2(a->x - render::cam.pos.x, a->z - render::cam.pos.z);
    Batches* T[2];
    int nT = 0;
    if (shadow) T[nT++] = &B[a->sp];
    else {
      if (dCam < W_MID + W_BAND + 1) T[nT++] = &B[a->sp];
      if (dCam > W_MID - 1) T[nT++] = &B[a->sp + SP_COUNT];
    }
#define WADD(field, mat) do { Mat4 m_ = (mat); for (int t_ = 0; t_ < nT; t_++) T[t_]->field.add(m_); } while (0)
    WADD(body, base);
    float headPitch = a->state == S_DOWNED ? 0.3f : a->pitch - a->lunge * 0.45f - bob * 1.5f / std::max(0.2f, legLen);
    WADD(head, base * partMatrix(M.headPivot, headPitch));

    for (int L = 0; L < 4; L++) {
      bool front = L < 2;
      float l1 = front ? M.lenF1 : M.lenH1, l2 = front ? M.lenF2 : M.lenH2;
      Vec3 hip = M.legPivots[L];
      float groundY = -(hip.y + bob) + M.footLen;
      float fz = 0, fy = 0, curl = 0;
      if (a->state == S_DOWNED) {
        fz = front ? 0.12f : -0.1f;
        fy = hip.y * 0.45f;
        curl = 0.8f;
      } else if (a->leap > 0.05f) {
        fz = front ? legLen * 0.55f : -legLen * 0.6f;
        fy = legLen * (front ? 0.45f : 0.2f);
        curl = front ? 1 : 0.3f;
      } else if (a->rear > 0.3f && front) {
        fz = legLen * 0.35f;
        fy = hip.y * 0.5f + std::sin(E.clock * 9 + L) * 0.08f;
        curl = 0.6f;
      } else if (sv < 0.05f) {
        fz = front ? 0.02f : -0.02f;
      } else {
        float z1, y1, c1, z2, y2, c2, z3, y3, c3;
        footAt(WALK, cyc, L, amp, lift, z1, y1, c1);
        footAt(TROT, cyc, L, amp, lift, z2, y2, c2);
        footAt(fast, cyc, L, amp, lift, z3, y3, c3);
        fz = z1 + (z2 - z1) * wTrot; fy = y1 + (y2 - y1) * wTrot; curl = c1 + (c2 - c1) * wTrot;
        fz += (z3 - fz) * wRun; fy += (y3 - fy) * wRun; curl += (c3 - curl) * wRun;
      }
      float tz = fz, ty = groundY + fy;
      float a1, a2;
      solveLeg(l1, l2, tz, ty, front, a1, a2);
      Mat4 hipM = base * Mat4::translate(hip);
      Mat4 up = hipM * rotAt({0, 0, 0}, a1);
      Vec3 knee(0, -std::cos(a1) * l1, std::sin(a1) * l1);
      Mat4 lo = hipM * rotAt(knee, a2);
      Vec3 ankle(0, knee.y - std::cos(a2) * l2, knee.z + std::sin(a2) * l2);
      float footA = -curl * (front ? (M.paw ? 1.1f : 1.6f) : (M.paw ? 0.7f : 1.0f)) * (a->state == S_DOWNED ? 0.6f : 1);
      Mat4 ft = hipM * rotAt(ankle, footA + spine);
      if (front) WADD(upperF, up); else WADD(upperH, up);
      if (front) WADD(lowerF, lo); else WADD(lowerH, lo);
      if (front) WADD(footF, ft); else WADD(footH, ft);
    }
    if (!shadow) fx::blob(a->x, a->y + 0.04f, a->z, a->yaw, M.shadowRadius * (a->fall > 0.5f ? 1.3f : 0.75f), M.shadowRadius * 1.25f);
  }
  for (int sp = 0; sp < NB; sp++) {
    Batches& b = B[sp];
    for (DynBatch* d : {&b.body, &b.head, &b.upperF, &b.lowerF, &b.upperH, &b.lowerH, &b.footF, &b.footH}) d->upload();
  }
}

static void drawBatches(Batches* B, int n) {
  Shader& s = render::object(VFmt::P3C4, IM_FULL, SeasonKind::None, OF_FUR | OF_SOFT);
  for (int sp = 0; sp < n; sp++) {
    if (n > SP_COUNT) {
      if (sp < SP_COUNT) s.set2f("u_fade", W_MID, W_MID + W_BAND);
      else { s.set2f("u_fade", 0, 0); s.set2f("u_fadeIn", W_MID, W_MID + W_BAND); }
    }
    Batches& b = B[sp];
    for (DynBatch* d : {&b.body, &b.head, &b.upperF, &b.lowerF, &b.upperH, &b.lowerH, &b.footF, &b.footH}) d->draw();
  }
  s.set2f("u_fade", 0, 0);
  s.set2f("u_fadeIn", 0, 0);
}

void draw() {
  fill(false);
  glEnable(GL_CULL_FACE);
  drawBatches(g_batch, SP_COUNT * 2);
}

void drawShadow() {
  fill(true);
  drawBatches(g_shadowBatch, SP_COUNT);
}

void drawBlobs() {}

}  // namespace wildlife
