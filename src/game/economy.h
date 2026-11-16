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
constexpr int CARAVAN_N = 8, CARAVAN_BASIC = 4;   // the last four come only with the seasonal bridge caravan
extern const Trade CARAVAN[CARAVAN_N];
struct GearDef { const char *id, *name, *blurb; Cost cost; };
constexpr int GEAR_COUNT = 6;
extern const GearDef GEAR[GEAR_COUNT];

struct Production { float every; Cost give; const char* label; };
const Production* production(const std::string& buildingId);

int foodCap();
int dailyNeed(int population, bool winter);
void fitJobs(int* jobs, int population);
float villageDefense();
int housingCapacity();
int villageAppeal();
