#include "season.h"
#include "engine.h"
#include <cmath>

const char* SEASON_NAMES[4] = {"Spring", "Summer", "Autumn", "Winter"};
const char* SEASON_NOTES[4] = {
    "Meadows bloom. Herbs grow back fast and settlers arrive sooner.",
    "Long warm days. Workshops produce more.",
    "Leaves turn and fall. Timber comes easier.",
    "Snow covers the valley. Work slows and settlers arrive less often.",
};

SeasonState season;
WindState wind;

namespace {
struct Params { float autumn, snow, bare, spring, dry, leaf, fall; };
const Params TABLE[4] = {
    {0, 0, 0, 1, 0, 0.35f, 0},
    {0, 0, 0, 0, 1, 0, 0},
    {1, 0, 0.15f, 0, 0.2f, 1, 0},
    {0.25f, 1, 0.8f, 0, 0, 0, 1},
};
float smooth(float a, float b, float x) {
  float t = clampf((x - a) / (b - a), 0, 1);
  return t * t * (3 - 2 * t);
}
}  // namespace

void updateWind(float dt) {
  wind.t += dt;
  float t = wind.t;
  const float bases[4] = {0.8f, 0.6f, 1.0f, 1.25f};
  float base = bases[season.index & 3];
  float gust = 0.7f + 0.22f * std::sin(t * 0.13f) + 0.12f * std::sin(t * 0.37f + 1.3f) + 0.08f * std::sin(t * 0.91f + 2.1f);
  wind.strength = base * gust * (1 + 0.6f * season.fall);
  float a = 0.65f + 0.5f * std::sin(t * 0.017f);
  wind.dirX = std::cos(a);
  wind.dirZ = std::sin(a);
}

void updateSeason(float time) {
  float p = (season.start * DAYS_PER_SEASON + season.day + time / 24) / DAYS_PER_SEASON;
  int idx = ((int)std::floor(p)) % 4;
  float f = p - std::floor(p);
  float w = smooth(1 - 1.6f / DAYS_PER_SEASON, 1, f);
  const Params &A0 = TABLE[idx], &B0 = TABLE[(idx + 1) % 4];
  season.autumn = A0.autumn + (B0.autumn - A0.autumn) * w;
  season.snow = A0.snow + (B0.snow - A0.snow) * w;
  season.bare = A0.bare + (B0.bare - A0.bare) * w;
  season.spring = A0.spring + (B0.spring - A0.spring) * w;
  season.dry = A0.dry + (B0.dry - A0.dry) * w;
  season.leaf = A0.leaf + (B0.leaf - A0.leaf) * w;
  season.fall = A0.fall + (B0.fall - A0.fall) * w;
  float gust = 0.35f + 0.65f * std::max(0.0f, std::sin(p * DAYS_PER_SEASON * 2.1f + 1.3f));
  season.fall *= gust;
  season.blend = w;
  season.dayOfSeason = (int)std::floor(f * DAYS_PER_SEASON) + 1;
  if (idx != season.index) {
    season.index = idx;
    popup(SEASON_NAMES[idx], rgbHex("#fde68a"));
    actions::notify(std::string(SEASON_NAMES[idx]) + " has come to the valley. " + SEASON_NOTES[idx]);
  }
  season.ice = smooth(0.4f, 1, season.snow);
}

void setSeasonStart(int i, float time) {
  season.start = ((i % 4) + 4) % 4;
  season.day = 0;
  season.index = season.start;
  updateSeason(time);
}

void restoreSeason(int start, int day, float time) {
  season.start = ((start % 4) + 4) % 4;
  season.day = std::max(0, day);
  season.index = (int)std::floor((season.start * DAYS_PER_SEASON + season.day + time / 24) / DAYS_PER_SEASON) % 4;
  updateSeason(time);
}

void nextDay() { season.day += 1; }

float seasonOutput(const std::string& kind) {
  int i = season.index;
  if (i == 0) return kind == "herb" || kind == "fiber" ? 1.5f : 1;
  if (i == 1) return 1.25f;
  if (i == 2) return kind == "wood" ? 1.5f : 1;
  return 0.75f;
}

float settlerPace() {
  const float p[4] = {0.75f, 1, 1, 1.5f};
  return p[season.index & 3];
}
