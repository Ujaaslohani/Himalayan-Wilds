#pragma once
#include <string>

// Random village events: rolled each dawn, at least one per week.
namespace events {
void reset();
void dawn();                                   // called once per in-game morning
float workMult(const std::string& building = "");  // production multiplier from the active event
float tradeMult();                             // market trade bonus (travelling trader)
bool forceRain();                              // storm day
int active();
int festival();                                // 0 none, 1 Losar, 2 harvest festival                                  // current event id, -1 none
const char* activeName();
bool traderPos(float& x, float& y, float& z, float& yaw);  // the caravan, while it is in the village
void drawCaravan();
int daysSince();
void restore(int active, int daysSince);
}
