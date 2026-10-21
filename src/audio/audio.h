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
void levelUpStinger();
void meleePunch();
void powerupChime();
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
void update(float dt);
}
