#pragma once
#include "../core/math.h"

namespace player {
void reset();
void update(float dt);
Vec3 velocity();
float stridePhase();   // walk cycle (one step per pi)   // horizontal, m/s
void onKeyPressed(int key);
void onMouseDown(int button);
void onMouseUp(int button);
void releaseBow();
void interact();
void jump();
void tryPlace();
Mat4 cameraWorld();
Vec3 cameraForward();
float fov();
void updateCamera(float aspect);
}
