#pragma once
#include <string>

enum class Wave { Sine, Square, Triangle, Sawtooth, Noise };

struct ToneSpec {
  Wave type = Wave::Sine;
  float freq = 440, freqTo = -1, dur = 0.1f, gain = 0.1f, delay = 0;
};

namespace audio {
bool init();
void shutdown();
void update(float dt);
void tone(const ToneSpec& t);
void tone(Wave type, float freq, float freqTo, float dur, float gain, float delay = 0);
void crispClick();
void confirmSelect();
void coinPickup();
void footstepWood();
void footstepStone();
void footstepGrass();
void footstepSnow();
void footstepLeaves();   // v1.1.11: dry leaf litter crunch (autumn)
void footstepMud();      // v1.1.11: wet squelch on the river bank / after rain
void footstepWater();
bool dangerSting();  // true if it played (rate-limited)
void setIndoor(float k);  // 0 outside .. 1 under a roof: muffles rain and wind
void levelUpStinger();
void meleePunch();
// spatial archery sounds (gain 0..1 from distance, pan -1..1 left/right of the listener)
void bowCreak(float gain, float pan);   // wood and string creak as a bow is drawn
void bowRelease(float gain, float pan); // string slap, limb thump, short low hum, fletching hiss
void arrowWhiz(float gain, float pan);
void arrowHitBody();  // you are struck: thud into cloth and a short breathy grunt  // an arrow passing close by
void arrowThunk(float gain, float pan, bool wood);  // hitting a trunk (wood) or the ground
void powerupChime();
bool playClip(const char* name, float gain, float pan, double delay = 0);
bool playClipRate(const char* name, float gain, float pan, double delay, float rate);   // recorded one-shot by name (false if none)
void foliageRustle(float gain);
void animalVoice(int sp, int call, float gain, float pan, float rate = 1.0f, float delay = 0.0f);
float animalVoiceLen(int sp, int call);   // longest recording for that call, s   // rate: doppler pitch   // 1 boar, 3 wolf, 4 bear, 5 leopard; call 0 warn, 1 attack
void victoryFanfare();
void arcadeLaser();
bool toggleSfxMute();
bool sfxMuted();
void setMusicPlaying(bool playing);
void setMusicMuted(bool muted);
void setMusicVolume(float v);
void setSfxVolume(float v);
}

namespace ambience {
void start();
void ringBell(int strikes = 3, float gap = 2.4f);
void howl(float dist = 80);
void thunder(float delay, float dist);  // dist 0 near .. 1 far
void update(float dt);
}
