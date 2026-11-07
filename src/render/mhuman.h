#pragma once
// MakeHuman villagers: one low-poly skinned body (proxy741, 1.5K triangles; assets/people/person.hwp, converted
// offline from the user's MakeHuman FBX) with real skin, eye and brow textures. Clothes are painted on in the
// shader from a UV-space clothing mask (tunic / trousers / shoes / sash, each recoloured per villager) plus a few
// rigid garments for volume (tunic flare or long skirt, Himachali topi or headscarf). Posed procedurally on the
// CPU (23 bones): walking, idle sway, arm work, head turns.
#include "../core/math.h"

namespace mh {
enum Kind { MAN = 0, WOMAN = 1, ELDER = 2, BANDIT = 3 };
struct Look {
  Kind kind = MAN;
  float skin = 1.0f;  // skin tone multiplier around the Himalayan base tone (0.8 darker .. 1.15 fairer)
  Vec3 tunic{0.6f, 0.55f, 0.45f}, trousers{0.3f, 0.25f, 0.2f}, shoes{0.18f, 0.12f, 0.08f}, sash{0.5f, 0.1f, 0.1f}, cap{0.2f, 0.4f, 0.2f};  // linear RGB
};
struct Pose {
  float walkPhase = 0, walk = 0;          // stride phase (radians) and amount 0..1
  float speed = -1;                       // ground speed in m/s: sizes the stride so planted feet do not slide (-1: from walk)
  float armL = 0, armR = 0;               // extra forward raise of each arm (radians, + = forward/up)
  float elbowL = 0, elbowR = 0;           // extra elbow bend
  float armOutL = 0, armOutR = 0;         // sideways raise
  float foldR = 0;                        // right forearm folded in toward the face (drawing a bowstring)
  float headYaw = 0, headPitch = 0;
  float lean = 0;                         // forward lean of the spine
  float armSwing = 1;                     // scale of the arm swing while walking (hands busy: lower)
};
bool ready();                              // assets loaded (else callers fall back to the old models)
float height();                            // standing height (m) of the unscaled body
void begin(bool shadow);
// base: world placement (translation * yaw * uniform scale); dist: camera distance (drops eyes/brows when far)
void draw(const Mat4& base, const Look& look, const Pose& pose, float dist, bool shadow);
// world-space position of a joint after posing (for props held in a hand): "hand_l", "hand_r", "head", "spine_03"
Vec3 jointWorld(const Mat4& base, const Pose& pose, const char* joint);
// where to draw one of the game's stylised heads (models/people) on this body: the posed head-bone frame
constexpr float HEAD_SCALE = 0.9f;
Mat4 headFrame(const Mat4& base, const Pose& pose);
void end();
void shutdown();
}  // namespace mh
