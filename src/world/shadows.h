#pragma once
#include "../core/math.h"
#include <vector>

struct CastInstance { float c[3]; float r; float n[3]; float h; };

CastInstance castInstance(float x, float y, float z, float r, float h);
void drawCasts(const std::vector<CastInstance>& list);
void shutdownCasts();
