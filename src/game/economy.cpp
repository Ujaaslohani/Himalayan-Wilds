#include "economy.h"
#include "../models/houses.h"

const JobDef JOBS[JOB_COUNT] = {
    {"woodcutter", "Woodcutter", "axe", "+2 timber every 40 s", {{"wood", 2}}},
    {"quarry", "Stone Gatherer", "pick", "+2 slate every 40 s", {{"stone", 2}}},
    {"herbalist", "Forager", "herb", "+1 herb and +1 fibre every 40 s", {{"herb", 1}, {"fiber", 1}}},
    {"hunter", "Hunter", "bow", "+3 food every 40 s", {{"food", 3}}},
};

const Trade TRADES[9] = {
    {"t1", {{"wolf_fur", 1}}, {{"wood", 15}}},
    {"t2", {{"wolf_fur", 1}}, {{"stone", 10}}},
    {"t3", {{"bear_hide", 1}}, {{"food", 25}}},
    {"t4", {{"bear_hide", 1}}, {{"wood", 20}, {"stone", 15}}},
    {"t5", {{"leopard_pelt", 1}}, {{"arrow", 30}, {"fiber", 10}}},
    {"t6", {{"deer_pelt", 2}}, {{"food", 8}}},
    {"t7", {{"boar_hide", 2}}, {{"stone", 12}}},
    {"t8", {{"goral_horn", 2}}, {{"herb", 6}, {"fiber", 6}}},
    {"t9", {{"bone", 4}, {"sinew", 2}}, {{"arrow", 20}}},
};

const GearDef GEAR[2] = {
    {"fur_coat", "Fur Coat", "Keeps you warm on winter nights outside the village. Without it the cold slowly drains your health.", {{"wolf_fur", 2}, {"sinew", 2}, {"fiber", 4}}},
    {"leather_armour", "Leather Armour", "Takes 40% less damage from predators.", {{"bear_hide", 1}, {"boar_hide", 2}, {"sinew", 4}}},
};

const Production* production(const std::string& id) {
  static const Production HUT{60, {{"arrow", 8}}, "+8 arrows / min"};
  static const Production CARP{45, {{"wood", 4}}, "+4 timber / 45 s"};
  static const Production HERB{50, {{"herb", 2}, {"fiber", 2}}, "+2 herbs, +2 fibre / 50 s"};
  static const Production TOWER{75, {{"stone", 3}}, "+3 slate / 75 s, reveals wildlife on radar"};
  if (id == "hunter_hut") return &HUT;
  if (id == "carpenter_workshop") return &CARP;
  if (id == "herbalist_lodge") return &HERB;
  if (id == "watchtower") return &TOWER;
  return nullptr;
}

int foodCap() {
  int n = 0;
  for (auto& s : A.structures) if (s.kind == SK_FOOD_STORE) n++;
  return FOOD_BASE_CAP + n * FOOD_PER_STORE;
}

int dailyNeed(int population, bool winter) { return (int)std::ceil(population * (winter ? 1.5f : 1.0f)); }

void fitJobs(int* jobs, int population) {
  int total = 0;
  for (int j = 0; j < JOB_COUNT; j++) total += jobs[j];
  for (int i = JOB_COUNT - 1; i >= 0 && total > population; i--) {
    int cut = std::min(jobs[i], total - population);
    jobs[i] -= cut;
    total -= cut;
  }
}

float villageDefense() {
  float d = 0;
  for (auto& s : A.structures) {
    if (s.kind == SK_LAMP_POST) d += 1.5f;
    else if (s.kind == SK_FENCE) d += 0.4f;
    else if (s.kind == SK_FLAGPOLE) d += 0.5f;
  }
  for (auto& b : A.buildings) {
    if (!b.built) continue;
    if (b.type == "watchtower") d += 5.0f * std::max(1, b.level);
    else if (b.type == "hunter_hut") d += 2;
  }
  return std::round(d * 10) / 10;
}

int housingCapacity() {
  int c = 0;
  for (auto& s : A.structures) c += KINDS[s.kind].capacity;
  return c;
}

int villageAppeal() {
  int a = 0;
  for (auto& s : A.structures) a += KINDS[s.kind].appeal;
  return a;
}
