#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#include "miniaudio.h"
#include "audio.h"
#include "../platform/platform.h"
#include "../game/engine.h"
#include "../game/season.h"
#include "../world/terrain.h"
#include <mutex>
#include <vector>
#include <random>
#include <cmath>
#include <atomic>

namespace {

const int SR = 48000;
const float TWO_PI = 6.28318530718f;

struct Biquad {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
  int type = 0;
  void set(int t, float f, float q) {
    type = t;
    f = std::max(10.0f, std::min(f, SR * 0.45f));
    float w = TWO_PI * f / SR, cs = std::cos(w), sn = std::sin(w), alpha = sn / (2 * std::max(0.05f, q));
    float a0;
    if (t == 0) { b0 = (1 - cs) / 2; b1 = 1 - cs; b2 = (1 - cs) / 2; }
    else if (t == 1) { b0 = (1 + cs) / 2; b1 = -(1 + cs); b2 = (1 + cs) / 2; }
    else { b0 = alpha; b1 = 0; b2 = -alpha; }
    a0 = 1 + alpha;
    a1 = -2 * cs;
    a2 = 1 - alpha;
    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
  }
  float run(float x) {
    float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
};

enum VKind { V_OSC, V_NOISE };
enum Env { E_BLIP, E_TONE };

struct Voice {
  VKind kind;
  Wave wave;
  Env env;
  double start;
  float dur, f0, f1, gain, attack, pan, vib, q, nf0, nf1;
  int ftype;
  double phase = 0, vphase = 0;
  Biquad bq;
  bool bus = false;
};

struct Bed {
  Biquad bq;
  int type;
  float freq, q;
  float gain = 0, target = 0, tau = 0.5f;
  float freqTarget, freqTau = 0.5f;
  float pan = 0, panTarget = 0;
  size_t pos = 0;
};

ma_device g_device;
bool g_ok = false;
std::mutex g_mtx;
std::vector<Voice> g_pending, g_voices;
std::vector<float> g_noise;
std::atomic<double> g_time{0};
Bed g_beds[5];
std::atomic<bool> g_sfxMuted{false}, g_musicMuted{false}, g_musicPlaying{false};
std::atomic<float> g_musicVol{0.22f}, g_sfxVol{0.6f};
std::vector<float> g_musicPcm;
size_t g_musicPos = 0;
bool g_musicOk = false;
std::mt19937 g_rng(99);

float rnd() { return std::uniform_real_distribution<float>(0, 1)(g_rng); }

float osc(Wave w, double ph) {
  double p = ph - std::floor(ph);
  switch (w) {
    case Wave::Sine: return (float)std::sin(p * TWO_PI);
    case Wave::Square: return p < 0.5 ? 0.6f : -0.6f;
    case Wave::Triangle: return (float)(p < 0.5 ? p * 4 - 1 : 3 - p * 4);
    case Wave::Sawtooth: return (float)(p * 2 - 1) * 0.7f;
    default: return 0;
  }
}

float expRamp(float a, float b, float t) {
  a = std::max(a, 1e-4f);
  b = std::max(b, 1e-4f);
  return a * std::pow(b / a, clampf(t, 0, 1));
}

void callback(ma_device*, void* out, const void*, ma_uint32 frames) {
  float* o = (float*)out;
  std::fill(o, o + frames * 2, 0.0f);
  {
    std::lock_guard<std::mutex> lk(g_mtx);
    for (auto& v : g_pending) g_voices.push_back(v);
    g_pending.clear();
  }
  double t0 = g_time.load();
  float sv = g_sfxMuted ? 0.0f : g_sfxVol.load();
  const float dt = 1.0f / SR;
  for (auto& v : g_voices) {
    float pl = std::sqrt(0.5f * (1 - v.pan)), pr = std::sqrt(0.5f * (1 + v.pan));
    for (ma_uint32 i = 0; i < frames; i++) {
      double t = t0 + i * (double)dt;
      float lt = (float)(t - v.start);
      if (lt < 0 || lt > v.dur + 0.03f) continue;
      float u = lt / v.dur;
      float g;
      if (v.env == E_BLIP) g = expRamp(v.gain, 0.0001f, u);
      else {
        float a = v.attack;
        g = lt < a ? expRamp(0.0001f, v.gain, lt / a) : expRamp(v.gain, 0.0001f, (lt - a) / std::max(1e-4f, v.dur - a));
      }
      if (lt > v.dur) g = 0;
      float s;
      if (v.kind == V_OSC) {
        float f = v.f1 > 0 ? expRamp(v.f0, v.f1, u) : v.f0;
        if (v.vib > 0) {
          f += std::sin(v.vphase * TWO_PI) * v.vib;
          v.vphase += 5.5 * dt;
        }
        v.phase += f * dt;
        s = osc(v.wave, v.phase);
      } else {
        if ((i & 31) == 0) v.bq.set(v.ftype, v.nf1 > 0 ? expRamp(v.nf0, v.nf1, u) : v.nf0, v.q);
        size_t idx = (size_t)(v.phase) % g_noise.size();
        v.phase += 1;
        s = v.bq.run(g_noise[idx]);
      }
      s *= g * sv;
      o[i * 2] += s * pl;
      o[i * 2 + 1] += s * pr;
    }
  }
  double tEnd = t0 + frames * (double)dt;
  g_voices.erase(std::remove_if(g_voices.begin(), g_voices.end(), [&](const Voice& v) { return tEnd > v.start + v.dur + 0.05; }), g_voices.end());
  for (auto& b : g_beds) {
    if (b.gain < 1e-5f && b.target < 1e-5f) continue;
    float kg = 1 - std::exp(-dt / b.tau), kf = 1 - std::exp(-dt / b.freqTau), kp = 1 - std::exp(-dt / 0.3f);
    for (ma_uint32 i = 0; i < frames; i++) {
      b.gain += (b.target * (g_sfxMuted ? 0 : 1) - b.gain) * kg;
      if ((i & 63) == 0) {
        b.freq += (b.freqTarget - b.freq) * (1 - std::exp(-dt * 64 / b.freqTau));
        b.bq.set(b.type, b.freq, b.q);
      }
      (void)kf;
      b.pan += (b.panTarget - b.pan) * kp;
      float s = b.bq.run(g_noise[b.pos++ % g_noise.size()]) * b.gain * sv * 1.6f;
      float pl = std::sqrt(0.5f * (1 - b.pan)), pr = std::sqrt(0.5f * (1 + b.pan));
      o[i * 2] += s * pl;
      o[i * 2 + 1] += s * pr;
    }
  }
  if (g_musicOk && g_musicPlaying && !g_musicMuted) {
    float mv = g_musicVol.load();
    size_t n = g_musicPcm.size();
    for (ma_uint32 i = 0; i < frames * 2; i++) {
      o[i] += g_musicPcm[g_musicPos] * mv;
      if (++g_musicPos >= n) g_musicPos = 0;
    }
  }
  for (ma_uint32 i = 0; i < frames * 2; i++) o[i] = std::tanh(o[i]);
  g_time = tEnd;
}

double now() { return g_time.load() + 0.03; }

void push(const Voice& v) {
  if (!g_ok) return;
  std::lock_guard<std::mutex> lk(g_mtx);
  if (g_pending.size() < 256) g_pending.push_back(v);
}

void blip(double at, Wave w, float f0, float f1, float dur, float gain) {
  Voice v{};
  v.kind = V_OSC;
  v.wave = w;
  v.env = E_BLIP;
  v.start = at;
  v.dur = dur;
  v.f0 = f0;
  v.f1 = f1;
  v.gain = gain;
  v.pan = 0;
  push(v);
}

void noiseBurst(double at, float dur, float gain, float from, float to) {
  Voice v{};
  v.kind = V_NOISE;
  v.env = E_BLIP;
  v.start = at;
  v.dur = dur;
  v.gain = gain * 2.0f;
  v.ftype = 0;
  v.nf0 = from;
  v.nf1 = to;
  v.q = 0.7f;
  v.phase = rnd() * g_noise.size();
  push(v);
}

void atone(double at, Wave w, float f0, float f1, float dur, float gain, float pan = 0, float vib = 0) {
  Voice v{};
  v.kind = V_OSC;
  v.wave = w;
  v.env = E_TONE;
  v.start = at;
  v.dur = dur;
  v.f0 = f0;
  v.f1 = f1;
  v.gain = gain;
  v.attack = std::min(0.05f, dur * 0.3f);
  v.pan = pan;
  v.vib = vib;
  push(v);
}

void aburst(double at, int ftype, float freq, float q, float dur, float gain, float pan = 0, float attack = 0.004f) {
  Voice v{};
  v.kind = V_NOISE;
  v.env = E_TONE;
  v.start = at;
  v.dur = dur;
  v.gain = gain * 2.0f;
  v.attack = attack;
  v.ftype = ftype;
  v.nf0 = freq;
  v.nf1 = 0;
  v.q = q;
  v.pan = pan;
  v.phase = rnd() * g_noise.size();
  push(v);
}

float vary(float a = 0.05f) { return 1 + (rnd() * 2 - 1) * a; }

}  // namespace

namespace audio {

bool init() {
  g_noise.resize(SR * 2);
  float last = 0;
  for (auto& s : g_noise) {
    float w = rnd() * 2 - 1;
    last = (last + 0.04f * w) / 1.04f;
    s = last * 3.2f * 0.6f + w * 0.4f;
  }
  int types[5] = {0, 2, 0, 2, 0};
  float freqs[5] = {420, 760, 210, 2100, 520}, qs[5] = {0.6f, 0.55f, 0.7f, 2.2f, 0.5f};
  for (int i = 0; i < 5; i++) {
    g_beds[i].type = types[i];
    g_beds[i].freq = g_beds[i].freqTarget = freqs[i];
    g_beds[i].q = qs[i];
    g_beds[i].bq.set(types[i], freqs[i], qs[i]);
    g_beds[i].pos = (size_t)(rnd() * g_noise.size());
  }
  g_beds[3].tau = 0.05f;
  ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
  cfg.playback.format = ma_format_f32;
  cfg.playback.channels = 2;
  cfg.sampleRate = SR;
  cfg.dataCallback = callback;
  cfg.periodSizeInFrames = 512;
  if (ma_device_init(nullptr, &cfg, &g_device) != MA_SUCCESS) return false;
  auto files = platform::listFiles(platform::assetPath("music"), ".mp3");
  if (files.empty()) files = platform::listFiles(platform::assetPath("music"), ".wav");
  if (!files.empty()) {
    ma_decoder_config dc = ma_decoder_config_init(ma_format_f32, 2, SR);
    for (auto& f : files) {
      ma_uint64 count = 0;
      void* data = nullptr;
      if (ma_decode_file(f.c_str(), &dc, &count, &data) != MA_SUCCESS || !data) continue;
      const float* pcm = (const float*)data;
      g_musicPcm.insert(g_musicPcm.end(), pcm, pcm + count * 2);
      ma_free(data, nullptr);
    }
    g_musicOk = g_musicPcm.size() >= 2;
  }
  g_musicVol = g_settings.musicVolume;
  g_sfxVol = g_settings.sfxVolume;
  g_ok = ma_device_start(&g_device) == MA_SUCCESS;
  return g_ok;
}

void shutdown() {
  if (g_ok) ma_device_uninit(&g_device);
  g_musicPcm.clear();
  g_musicPcm.shrink_to_fit();
  g_ok = false;
}

void tone(const ToneSpec& t) { blip(now() + t.delay, t.type, t.freq, t.freqTo, t.dur, t.gain); }
void tone(Wave type, float freq, float freqTo, float dur, float gain, float delay) { blip(now() + delay, type, freq, freqTo, dur, gain); }
void crispClick() { blip(now(), Wave::Sine, 1200 * vary(), -1, 0.015f, 0.1f); }
void confirmSelect() {
  float v = vary();
  double t = now();
  blip(t, Wave::Sine, 523.25f * v, -1, 0.1f, 0.08f);
  blip(t + 0.08, Wave::Sine, 783.99f * v, -1, 0.15f, 0.1f);
}
void coinPickup() {
  float v = vary();
  double t = now();
  blip(t, Wave::Square, 987.77f * v, -1, 0.1f, 0.08f);
  blip(t + 0.08, Wave::Square, 1318.51f * v, -1, 0.2f, 0.08f);
}
void footstepWood() {
  float v = vary();
  double t = now();
  blip(t, Wave::Triangle, 180 * v, 60 * v, 0.09f, 0.1f);
  noiseBurst(t, 0.04f, 0.05f, 800, 300);
}
void footstepStone() {
  float v = vary();
  double t = now();
  noiseBurst(t, 0.05f, 0.08f, 1000 * v, 600 * v);
  blip(t, Wave::Triangle, 300 * v, -1, 0.03f, 0.04f);
}
void meleePunch() {
  float v = vary();
  double t = now();
  blip(t, Wave::Triangle, 140 * v, 30 * v, 0.15f, 0.18f);
  noiseBurst(t, 0.08f, 0.1f, 600, 80);
}
void powerupChime() {
  float v = vary();
  double t = now();
  float n[5] = {523.25f, 659.25f, 783.99f, 1046.5f, 1318.51f};
  for (int i = 0; i < 5; i++) blip(t + i * 0.06, Wave::Sine, n[i] * v, -1, 0.15f, 0.06f);
}
void levelUpStinger() {
  float v = vary();
  double t = now();
  float n[7] = {261.63f, 329.63f, 392.0f, 523.25f, 659.25f, 783.99f, 1046.5f};
  for (int i = 0; i < 7; i++) blip(t + i * 0.07, Wave::Sine, n[i] * v, -1, 0.25f, 0.06f);
  noiseBurst(t + 7 * 0.07, 0.8f, 0.05f, 3000, 6000);
}
void victoryFanfare() {
  float v = vary();
  double t = now();
  float n[4] = {261.63f, 329.63f, 392.0f, 523.25f};
  for (int i = 0; i < 4; i++) {
    blip(t + i * 0.1, Wave::Sawtooth, n[i] * v, -1, 0.4f, 0.06f);
    blip(t + i * 0.1, Wave::Sine, n[i] * v, -1, 0.4f, 0.04f);
  }
}
void arcadeLaser() { blip(now(), Wave::Sawtooth, 1800 * vary(), 80, 0.2f, 0.12f); }
bool toggleSfxMute() { g_sfxMuted = !g_sfxMuted; return g_sfxMuted; }
bool sfxMuted() { return g_sfxMuted; }
void setMusicPlaying(bool p) { g_musicPlaying = p; }
void setMusicMuted(bool m) { g_musicMuted = m; }
void setMusicVolume(float v) { g_musicVol = v; }
void setSfxVolume(float v) { g_sfxVol = v; }
void update(float) {}

}  // namespace audio

namespace ambience {

namespace {
float babbleT = 0, dayT = 4, extraT = 6, crackleT = 1, nextEvent = 3, beatT = 0;
int beat = 0;
const float FIRES[3][2] = {{73.5f, 54.5f}, {-92.5f, -27.5f}, {24.5f, -120.5f}};
const float SCALE[7] = {293.7f, 329.6f, 370, 440, 493.9f, 587.3f, 659.3f};
const int TUNE[32] = {0, 2, 3, 4, 3, 2, 0, -1, 2, 3, 5, 4, 3, 2, 3, -1, 4, 5, 6, 5, 4, 3, 2, -1, 0, 2, 3, 2, 0, -1, 0, -1};
float pr() { return rnd() * 1.6f - 0.8f; }

void woodpecker() {
  float pan = pr();
  double t = now();
  int n = 10 + (int)(rnd() * 8);
  for (int i = 0; i < n; i++) { aburst(t, 2, 1500 + rnd() * 300, 3, 0.03f, 0.05f * (1 - (float)i / n * 0.5f), pan); t += 0.055; }
}
void cuckoo() {
  float pan = pr();
  double t = now();
  int reps = 2 + (int)(rnd() * 3);
  for (int i = 0; i < reps; i++) {
    atone(t, Wave::Sine, 720, 700, 0.26f, 0.03f, pan);
    atone(t + 0.3, Wave::Sine, 580, 560, 0.38f, 0.028f, pan);
    t += 0.95;
  }
}
void eagle() {
  float pan = pr();
  double t = now();
  atone(t, Wave::Sine, 2600, 1500, 0.7f, 0.018f, pan, 60);
  atone(t + 0.8, Wave::Sine, 2400, 1400, 0.55f, 0.014f, pan, 60);
}
void rustle(float k) { aburst(now(), 1, 2600 + rnd() * 1200, 0.8f, 0.9f + rnd() * 0.8f, 0.04f * k, pr(), 0.25f); }
void frogs(float k) {
  float pan = pr();
  double t = now();
  int n = 2 + (int)(rnd() * 3);
  float f = 170 + rnd() * 90;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < 4; j++) atone(t + j * 0.035, Wave::Square, f, f * 0.92f, 0.03f, 0.012f * k, pan);
    t += 0.32 + rnd() * 0.2;
  }
}
void buzz() { atone(now(), Wave::Sawtooth, 210 + rnd() * 40, 240, 1.4f, 0.006f, pr(), 12); }
void crackle(float k) {
  double t = now();
  int n = 2 + (int)(rnd() * 5);
  for (int i = 0; i < n; i++) { aburst(t, 1, 2200 + rnd() * 2500, 0.8f, 0.012f + rnd() * 0.02f, 0.05f * k, rnd() * 0.6f - 0.3f); t += 0.02 + rnd() * 0.12; }
  aburst(now(), 0, 300, 0.8f, 0.4f, 0.02f * k, 0, 0.05f);
}
void birds() {
  float pan = pr();
  float base = 2200 + rnd() * 1600;
  int n = 2 + (int)(rnd() * 4);
  double t = now();
  for (int i = 0; i < n; i++) {
    atone(t, Wave::Sine, base * (0.9f + rnd() * 0.25f), base * (1.2f + rnd() * 0.4f), 0.07f + rnd() * 0.08f, 0.025f, pan);
    t += 0.1 + rnd() * 0.12;
  }
}
void crickets() {
  float pan = rnd() * 1.4f - 0.7f;
  double t = now();
  for (int k = 0; k < 3; k++) {
    for (int i = 0; i < 3; i++) { atone(t, Wave::Square, 4400, -1, 0.025f, 0.006f, pan); t += 0.045; }
    t += 0.25;
  }
}
void owl() {
  double t = now();
  float pan = pr();
  atone(t, Wave::Sine, 400, 360, 0.35f, 0.03f, pan);
  atone(t + 0.55, Wave::Sine, 390, 340, 0.6f, 0.035f, pan, 4);
}
void festivalStep(int k, float nearAmt) {
  double t = now() + 0.02;
  float g = 0.06f * nearAmt;
  if (k % 4 == 0) atone(t, Wave::Sine, 120, 48, 0.22f, g * 1.6f);
  if (k % 4 == 2) atone(t, Wave::Sine, 180, 90, 0.12f, g * 0.9f);
  if (k % 8 == 7) atone(t, Wave::Sine, 210, 120, 0.1f, g * 0.8f);
  int n = TUNE[(k / 2) % 32];
  if (k % 2 == 0 && n >= 0) atone(t, Wave::Triangle, SCALE[n] * 2, -1, 0.42f, g * 0.55f, 0, 6);
}
}  // namespace

void start() {}

void ringBell(int strikes, float gap) {
  if (!g_ok || g_sfxMuted) return;
  double t0 = now() + 0.05;
  const float PART[9][3] = {{0.5f, 0.22f, 7}, {1, 0.32f, 5.5f}, {1.19f, 0.18f, 4.2f}, {1.5f, 0.12f, 3.6f}, {2, 0.14f, 3.2f}, {2.74f, 0.09f, 2.2f}, {3.76f, 0.06f, 1.6f}, {5.4f, 0.04f, 1.0f}, {6.8f, 0.03f, 0.7f}};
  for (int k = 0; k < strikes; k++) {
    double at = t0 + k * gap;
    float f = 392 * (k == strikes - 1 ? 0.84f : 1);
    float pan = k % 2 ? 0.25f : -0.25f;
    for (auto& p : PART) {
      for (int d = 0; d < 2; d++) {
        Voice v{};
        v.kind = V_OSC;
        v.wave = Wave::Sine;
        v.env = E_TONE;
        v.start = at;
        v.dur = p[2];
        v.f0 = f * p[0] * (d ? 1.0035f : 1 + (rnd() - 0.5f) * 0.002f);
        v.f1 = -1;
        v.gain = p[1] * 0.55f;
        v.attack = 0.006f;
        v.pan = pan;
        push(v);
      }
    }
    aburst(at, 2, 2400, 0.8f, 0.05f, 0.35f * 0.55f, pan, 0.002f);
  }
}

void howl(float dist) {
  if (!g_ok || g_sfxMuted) return;
  float g = std::max(0.01f, 0.07f * (1 - std::min(1.0f, dist / 220)));
  double t = now();
  float pan = rnd() * 1.4f - 0.7f;
  atone(t, Wave::Sine, 360, 640, 1.3f, g, pan, 9);
  atone(t + 1.25, Wave::Sine, 640, 470, 1.4f, g * 0.9f, pan, 9);
  if (rnd() < 0.6f) {
    atone(t + 0.6, Wave::Sine, 420, 700, 1.2f, g * 0.6f, -pan, 8);
    atone(t + 1.8, Wave::Sine, 700, 520, 1.2f, g * 0.5f, -pan, 8);
  }
}

void update(float dt) {
  if (!g_ok) return;
  float on = E.playing ? 1.0f : 0.3f;
  const Vec3& p = E.player.pos;
  float gust = 0.6f + 0.4f * std::sin(E.clock * 0.23f) * std::sin(E.clock * 0.071f + 1);
  float w = (0.025f + 0.07f * season.snow + 0.06f * season.fall + std::max(0.0f, (p.y - 40) / 80) * 0.06f) * gust * on;
  g_beds[0].target = w;
  g_beds[0].freqTarget = 300 + gust * 380 + season.fall * 200;
  RiverHit q = world::riverQuery(p.x, p.z);
  float rd = q.d < 1e8f ? std::max(0.0f, q.d - q.hw) : 1e4f;
  float frozen = 1 - 0.75f * std::min(1.0f, season.snow);
  float nearAmt = std::pow(std::max(0.0f, 1 - rd / 75), 2.0f) * frozen * on * (1 + q.flow * 0.6f);
  for (const Lake& L : world::LAKES) {
    float ld = std::max(0.0f, hypot2(p.x - L.x, p.z - L.z) - L.r);
    nearAmt = std::max(nearAmt, std::pow(std::max(0.0f, 1 - ld / 40), 2.0f) * 0.3f * frozen * on);
  }
  g_beds[1].target = nearAmt * 0.32f;
  g_beds[2].target = nearAmt * 0.35f;
  babbleT -= dt;
  if (babbleT <= 0) {
    babbleT = 0.08f + rnd() * 0.18f;
    g_beds[3].target = nearAmt * (0.04f + rnd() * 0.16f);
    g_beds[3].freqTarget = 1500 + rnd() * 1600;
  }
  if (q.d < 1e8f) {
    float yaw = E.player.yaw;
    float rx = std::cos(yaw), rz = -std::sin(yaw);
    float dx = q.cx - p.x, dz = q.cz - p.z;
    float len = std::max(1e-3f, hypot2(dx, dz));
    float side = (dx * rx + dz * rz) / len;
    float pv = clampf(side * std::min(1.0f, rd / 6), -0.85f, 0.85f);
    for (int b = 1; b <= 3; b++) g_beds[b].panTarget = pv;
  }
  const P2& hd = world::RIVER.head;
  float fd = hypot2(p.x - hd[0], p.z - (hd[1] - 6));
  g_beds[4].target = std::pow(std::max(0.0f, 1 - fd / 130), 2.0f) * 0.5f * frozen * on;

  if (!E.playing || E.uiOpen) return;
  float day = E.daylight;
  float forest = world::forestDensity(p.x, p.z);
  nextEvent -= dt;
  if (nextEvent <= 0) {
    float r2 = rnd();
    if (day > 0.5f) {
      float birdy = 0.35f + season.spring * 0.5f + season.dry * 0.3f - season.snow * 0.3f;
      if (r2 < birdy) birds();
      nextEvent = 2 + rnd() * 6;
    } else if (day < 0.2f) {
      if ((season.dry > 0.4f || season.spring > 0.5f) && r2 < 0.7f) crickets();
      if (r2 > 0.8f) owl();
      else if (r2 < 0.06f + season.snow * 0.12f) howl(120 + rnd() * 80);
      nextEvent = 2.5f + rnd() * 5;
    } else nextEvent = 3;
  }
  dayT -= dt;
  if (dayT <= 0) {
    dayT = 5 + rnd() * 9;
    float r3 = rnd();
    if (day > 0.5f) {
      if (forest > 0.35f && r3 < 0.35f && season.snow < 0.6f) woodpecker();
      else if (r3 < 0.6f && (season.spring > 0.4f || season.dry > 0.4f)) cuckoo();
      else if (r3 < 0.75f && (p.y > 40 || forest < 0.3f)) eagle();
      else if (r3 < 0.9f && season.dry > 0.5f && forest < 0.5f) buzz();
    } else if (day < 0.25f && nearAmt > 0.15f && (season.spring > 0.3f || season.dry > 0.3f)) frogs(std::min(1.0f, nearAmt * 2));
  }
  extraT -= dt;
  if (extraT <= 0) {
    extraT = 1.5f + rnd() * 3;
    float gusty = 0.4f + season.fall * 0.6f + (gust - 0.6f) * 1.5f;
    if (forest > 0.25f && season.snow < 0.7f && rnd() < gusty) rustle(std::min(1.0f, forest + 0.2f));
  }
  crackleT -= dt;
  if (crackleT <= 0) {
    crackleT = 0.25f + rnd() * 0.6f;
    float fire = std::max(0.0f, 1 - hypot2(p.x, p.z) / 16);
    for (auto& cp : FIRES) fire = std::max(fire, 1 - hypot2(p.x - cp[0], p.z - cp[1]) / 12);
    if (fire > 0) crackle(fire);
  }
  if (E.festival.t > 0) {
    float nr = std::max(0.0f, 1 - hypot2(p.x, p.z) / 70);
    if (nr > 0) {
      beatT -= dt;
      if (beatT <= 0) {
        beatT += 0.22f;
        if (beatT < -0.5f) beatT = 0.22f;
        festivalStep(beat++, nr);
      }
    }
  }
}

}  // namespace ambience
