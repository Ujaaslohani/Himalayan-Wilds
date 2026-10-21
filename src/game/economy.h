#pragma once
#include "engine.h"

enum Job { JOB_WOODCUTTER, JOB_QUARRY, JOB_HERBALIST, JOB_HUNTER, JOB_COUNT };
struct JobDef { const char *key, *name, *icon, *blurb; Cost give; };
extern const JobDef JOBS[JOB_COUNT];
constexpr float JOB_EVERY = 40;
constexpr int FOOD_BASE_CAP = 40;
constexpr int FOOD_PER_STORE = 80;
constexpr float SAFE_DEFENSE = 10;

struct Trade { const char* id; Cost give, get; };
extern const Trade TRADES[9];
struct GearDef { const char *id, *name, *blurb; Cost cost; };
extern const GearDef GEAR[2];

struct Production { float every; Cost give; const char* label; };
const Production* production(const std::string& buildingId);

int foodCap();
int dailyNeed(int population, bool winter);
void fitJobs(int* jobs, int population);
float villageDefense();
int housingCapacity();
int villageAppeal();
