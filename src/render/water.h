#pragma once

namespace water {
void init();
void draw();
void drawParticles();
// a disturbance on the water surface: expanding ripple rings, plus a burst of droplets when splash > 0
void ripple(float x, float y, float z, float strength, float splash = 0);
void shutdown();
}
