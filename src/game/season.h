#pragma once
#include <string>

constexpr int DAYS_PER_SEASON = 7;
extern const char* SEASON_NAMES[4];
extern const char* SEASON_NOTES[4];

struct SeasonState {
  int start = 2, day = 0, index = 2, dayOfSeason = 1;
  float blend = 0;
  float autumn = 1, snow = 0, bare = 0.15f, spring = 0, dry = 0.2f, leaf = 1, fall = 0;
  float ice = 0;
};

struct WindState {
  float t = 0, strength = 1, dirX = 0.8f, dirZ = 0.6f;
};

extern SeasonState season;
extern WindState wind;

void updateWind(float dt);
void updateSeason(float time);
void setSeasonStart(int i, float time = 7.5f);
void restoreSeason(int start, int day, float time);
void nextDay();
float seasonOutput(const std::string& kind);
float settlerPace();
