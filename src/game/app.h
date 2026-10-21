#pragma once
#include "engine.h"
#include "economy.h"

extern const int UNLOCK[SK_COUNT];
extern const Cost RECRUIT_COST_CARPENTER, RECRUIT_COST_HERBALIST, RECRUIT_COST_BUILDER;

namespace app {
void init();
void newGame();
bool continueGame();
void quitToTitle();
void update(float dt);
void saveNow();
bool readSaveSummary(std::string& summary);
bool has(const Cost& c);
std::string shortfall(const Cost& c);
void spend(const Cost& c);
Cost recruitCost(const std::string& id);
Cost upgradeCost(const Building& b);
void craftArrows(bool bundle);
void recruit(int specialistIndex);
void upgrade(Building& b);
void trade(const Trade& t);
void craftGear(const GearDef& g);
void startStructure(StructureKind k);
void moveStructure(int uid);
void removeStructure(int uid);
void restyleStructure(int uid, const Look& l);
void placeWorkshop(const std::string& id);
void setJob(int j, int n);
Look defaultLookFor(StructureKind k);
Look nextLook(StructureKind k, const Look& l, int slot);
int capacity();
void syncDerived();
bool uiOpen();
}
