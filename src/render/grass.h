#pragma once

namespace grass {
void init();
void draw();
void setPreload(bool on);
int bladeCount();
// spring wildflowers: up to 4 flower fields (x, z, radius, 0) found once on open meadows, and the bloom
// strength (spring full, summer thin, none in autumn/winter) — the terrain shader paints the far view of the fields
const float* flowerFields();
float bloom();
void shutdown();
}
