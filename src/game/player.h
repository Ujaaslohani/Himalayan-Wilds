#pragma once
#include "../core/math.h"

namespace player {
void reset();
void update(float dt);
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
