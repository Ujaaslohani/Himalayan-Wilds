#include "../render/envmap.h"
#include "../render/sky.h"
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
#include <map>
#include <atomic>
#include <cctype>
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

enum VKind { V_OSC, V_NOISE, V_SAMPLE };
struct Clip { std::vector<float> pcm; };   // stereo f32 at SR
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
  bool bqSet = false;
  bool bus = false;
  const Clip* clip = nullptr;   // V_SAMPLE: recorded sound
  float rate = 1;               // V_SAMPLE playback rate (doppler)
  float lpFar = 0;              // V_SAMPLE positional: head-shadow low-pass state of the far ear
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
Bed g_beds[7];  // 0 wind, 1-3 river, 4 waterfall, 5 rain hiss, 6 rain body
std::atomic<bool> g_sfxMuted{false}, g_musicMuted{false}, g_musicPlaying{false};
std::atomic<float> g_musicVol{0.22f}, g_sfxVol{0.6f};
std::vector<float> g_musicPcm;
size_t g_musicPos = 0;
bool g_musicOk = false;
std::mt19937 g_rng(99);
// ---- always-on insect chorus, synthesised in the mixer: a field of crickets (pulsed sine carriers at 3.6-5.2 kHz,
// each with its own chirp rhythm, pitch and place in the stereo field) plus a daytime grasshopper / cicada shimmer.
// Levels are set from the game thread (time of day, season, forest, weather), the mixer only follows them smoothly.
// each cricket rubs its wings in pulses: a sharp onset, a quick decay, the pitch sliding down a touch within the
// pulse, a little rasp of noise, no two pulses or chirps identical, and pauses that come and go. Far ones are
// duller (high harmonics rolled off). This is what made the old pure-sine version sound like a machine.
struct Cricket {
  float f, pan, pulseHz, basePulses, baseGap, gain, dist;
  double ph = 0;
  float t = 0, chirpDur = 0, gap = 0, amp = 1, pulseAmp = 1, rest = 0, lp = 0, drift = 1;
  int pulses = 3, lastPulse = -1;
  bool trill = false;
  uint32_t rng = 1;
  float rnd() { rng = rng * 1664525u + 1013904223u; return (rng >> 8) / 16777216.0f; }
};
std::vector<Cricket> g_crick;
// day insects: a few grasshoppers, each rasping in short bursts ("zzzt ... zzzt-zzzt") from its own spot in the grass
struct Hopper {
  float t = 0, dur = 0, gap = 1, amHz = 70, pan = 0, gain = 1;
  Biquad bq;
  size_t pos = 0;
  uint32_t rng = 1;
  float rnd() { rng = rng * 1664525u + 1013904223u; return (rng >> 8) / 16777216.0f; }
};
Hopper g_hop[4];
std::atomic<float> g_crickTarget{0}, g_dayBugTarget{0};
float g_crickGain = 0, g_dayBugGain = 0;
Biquad g_bugBq, g_bugBq2;
size_t g_bugPos = 0;

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

// ---- recorded ambience loops (assets/sounds/ambient), mixed in the callback; gains set from the game thread ----
struct LoopPlayer { std::vector<const Clip*> clips; int cur = 0; size_t pos = 0; float gain = 0; std::atomic<float> target{0}; float tau = 1.5f; };
enum { L_DAY, L_BIRDSWATER, L_STREAM, L_RAINNIGHT, L_STORM, L_COUNT };
LoopPlayer g_loops[L_COUNT];
std::map<std::string, std::vector<Clip>> g_bank;   // foley/ambient one-shots by name prefix
const std::vector<Clip>* bank(const char* name) { auto it = g_bank.find(name); return it == g_bank.end() || it->second.empty() ? nullptr : &it->second; }
bool hasLoops() { for (auto& l : g_loops) if (!l.clips.empty()) return true; return false; }

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
  double tBlockEnd = t0 + frames * (double)dt;
  for (auto& v : g_voices) {
    if (v.start >= tBlockEnd || t0 > v.start + v.dur + 0.03) continue;  // not started yet / finished: skip the whole block
    float pl = std::sqrt(0.5f * (1 - v.pan)), pr = std::sqrt(0.5f * (1 + v.pan));
    float g = 0;
    if (v.kind == V_NOISE && v.nf1 <= 0 && !v.bqSet) { v.bq.set(v.ftype, v.nf0, v.q); v.bqSet = true; }
    for (ma_uint32 i = 0; i < frames; i++) {
      double t = t0 + i * (double)dt;
      float lt = (float)(t - v.start);
      if (lt < 0 || lt > v.dur) continue;
      float u = lt / v.dur;
      if ((i & 15) == 0 || g == 0) {  // envelope is smooth: evaluate the pow() every 16 samples, not every sample
        if (v.env == E_BLIP) g = expRamp(v.gain, 0.0001f, u);
        else {
          float a = v.attack;
          g = lt < a ? expRamp(0.0001f, v.gain, lt / a) : expRamp(v.gain, 0.0001f, (lt - a) / std::max(1e-4f, v.dur - a));
        }
      }
      float s;
      if (v.kind == V_SAMPLE) {
        double fpos = (double)lt * SR * v.rate;
        size_t k = (size_t)fpos;
        if ((k + 1) * 2 + 1 >= v.clip->pcm.size()) continue;
        float fr = (float)(fpos - k);
        const float* c = &v.clip->pcm[k * 2];
        float fade = std::min(1.0f, (v.dur - lt) / 0.05f);
        float sv2 = v.gain * fade * sv;
        if (std::fabs(v.pan) < 0.02f) {   // centred / non-positional: the recording's own stereo
          o[i * 2] += (c[0] + (c[2] - c[0]) * fr) * sv2 * pl * 1.41f;
          o[i * 2 + 1] += (c[1] + (c[3] - c[1]) * fr) * sv2 * pr * 1.41f;
          continue;
        }
        // positional (binaural-style): the near ear gets the sound first and full; the farS ear hears it up to
        // 0.65 ms later, quieter and muffled by the head. Hard left / right is (almost) only in that ear.
        float ap = std::min(1.0f, std::fabs(v.pan));
        float mono = 0.5f * ((c[0] + (c[2] - c[0]) * fr) + (c[1] + (c[3] - c[1]) * fr));
        double fp2 = fpos - 0.00065 * ap * SR * v.rate;
        float farS = 0;
        if (fp2 >= 0) { size_t k2 = (size_t)fp2; float f2 = (float)(fp2 - k2); const float* c2 = &v.clip->pcm[k2 * 2]; farS = 0.5f * ((c2[0] + (c2[2] - c2[0]) * f2) + (c2[1] + (c2[3] - c2[1]) * f2)); }
        float lpK = 1.0f - 0.85f * ap;          // one-pole low-pass: ~full band in front, ~1 kHz at the side
        v.lpFar += (farS - v.lpFar) * lpK;
        float nearG = 1.0f, farG = (1.0f - ap) * (1.0f - ap) * 0.9f + 0.04f;
        float L = v.pan < 0 ? mono * nearG : v.lpFar * farG, R = v.pan < 0 ? v.lpFar * farG : mono * nearG;
        o[i * 2] += L * sv2 * 1.1f;
        o[i * 2 + 1] += R * sv2 * 1.1f;
        continue;
      }
      if (v.kind == V_OSC) {
        float f = v.f1 > 0 ? expRamp(v.f0, v.f1, u) : v.f0;
        if (v.vib > 0) {
          f += std::sin(v.vphase * TWO_PI) * v.vib;
          v.vphase += 5.5 * dt;
        }
        v.phase += f * dt;
        s = osc(v.wave, v.phase);
      } else {
        if (v.nf1 > 0 && (i & 31) == 0) v.bq.set(v.ftype, expRamp(v.nf0, v.nf1, u), v.q);
        size_t idx = (size_t)(v.phase) % g_noise.size();
        v.phase += 1;
        s = v.bq.run(g_noise[idx]);
      }
      s *= g * sv;
      o[i * 2] += s * pl;
      o[i * 2 + 1] += s * pr;
    }
  }
  // ambience loops: each runs on, switching to another take of the same kind at the end of a take (0.4 s crossfade)
  {
    float sv0 = g_sfxMuted ? 0.0f : g_sfxVol.load();
    const float k = 1 - std::exp(-1.0f / (SR * 1.0f));
    for (auto& L : g_loops) {
      if (L.clips.empty()) continue;
      float tgt = L.target.load();
      if (tgt < 1e-4f && L.gain < 1e-4f) continue;
      const size_t XF = (size_t)(0.4f * SR);
      for (ma_uint32 i = 0; i < frames; i++) {
        L.gain += (tgt - L.gain) * k / L.tau;
        const Clip* c = L.clips[L.cur];
        size_t n = c->pcm.size() / 2;
        if (L.pos >= n) { L.pos = std::min(XF, n / 4); L.cur = (int)((L.cur + 1) % L.clips.size()); c = L.clips[L.cur]; n = c->pcm.size() / 2; }
        float a = c->pcm[L.pos * 2], b = c->pcm[L.pos * 2 + 1];
        if (L.pos + XF > n) {   // overlap the start of the next pass under the tail
          float u = (float)(L.pos + XF - n) / XF;
          size_t q = (L.pos + XF - n);
          a = a * (1 - u) + c->pcm[q * 2] * u; b = b * (1 - u) + c->pcm[q * 2 + 1] * u;
        }
        o[i * 2] += a * L.gain * sv0; o[i * 2 + 1] += b * L.gain * sv0;
        L.pos++;
      }
    }
  }
  // insect chorus
  {
    float ct = g_crickTarget.load() * (g_sfxMuted ? 0 : 1), dtg = g_dayBugTarget.load() * (g_sfxMuted ? 0 : 1);
    float kk = 1 - std::exp(-dt / 2.0f);
    if (ct > 1e-5f || g_crickGain > 1e-5f) {
      for (ma_uint32 i = 0; i < frames; i++) {
        g_crickGain += (ct - g_crickGain) * kk;
        float l = 0, rr = 0;
        for (auto& c : g_crick) {
          c.t += dt;
          if (c.rest > 0) { c.rest -= dt; continue; }
          if (c.t >= c.chirpDur + c.gap) {  // next chirp: fresh pulse count, loudness, spacing; now and then a pause
            c.t = 0;
            c.pulses = c.trill ? 40 + (int)(c.rnd() * 80) : std::max(2, (int)std::lround(c.basePulses + (c.rnd() - 0.5f) * 2.2f));
            c.chirpDur = c.pulses / c.pulseHz;
            c.gap = c.trill ? 0.15f + c.rnd() * 1.2f : c.baseGap * (0.75f + c.rnd() * 0.55f);
            c.amp = 0.65f + 0.35f * c.rnd();
            c.drift = 1.0f + (c.rnd() - 0.5f) * 0.012f;
            c.lastPulse = -1;
            if (c.rnd() < (c.trill ? 0.08f : 0.035f)) c.rest = 1.5f + c.rnd() * 6.0f;
          }
          float e = 0, glide = 1;
          if (c.t < c.chirpDur) {
            float pu = c.t * c.pulseHz;
            int pi = (int)pu;
            if (pi != c.lastPulse) { c.lastPulse = pi; c.pulseAmp = 0.7f + 0.3f * c.rnd(); }
            float pf = pu - pi;
            // fast attack, exponential decay, silent tail of each wing stroke
            { float w = pf < 0.68f ? (float)std::sin(pf / 0.68f * 3.14159265f) : 0.0f; e = w * w; }  // one smooth wing stroke, then the gap
            e *= c.pulseAmp * c.amp;
            float cp = c.t / std::max(1e-3f, c.chirpDur);
            e *= std::min(1.0f, cp * 6.0f + 0.4f) * std::min(1.0f, (1.0f - cp) * 6.0f + 0.5f);  // chirp swells in and out
            glide = 1.0f - 0.03f * pf;   // each stroke falls slightly in pitch, as the wing slows
          }
          if (e <= 0) continue;
          c.ph += c.f * c.drift * glide * dt;
          double ph = c.ph - std::floor(c.ph);
          float carrier = (float)std::sin(ph * TWO_PI) + 0.22f * (float)std::sin(ph * 2 * TWO_PI) * (1 - c.dist) + 0.08f * (float)std::sin(ph * 3 * TWO_PI) * (1 - c.dist);
          float raw = carrier * e;
          c.lp += (raw - c.lp) * (0.35f + 0.6f * (1 - c.dist));  // distance dulls the tone
          float sm = c.lp * c.gain;
          l += sm * std::sqrt(0.5f * (1 - c.pan)); rr += sm * std::sqrt(0.5f * (1 + c.pan));
        }
        o[i * 2] += l * g_crickGain * sv; o[i * 2 + 1] += rr * g_crickGain * sv;
      }
    }
    if (dtg > 1e-5f || g_dayBugGain > 1e-5f) {
      for (ma_uint32 i = 0; i < frames; i++) {
        g_dayBugGain += (dtg - g_dayBugGain) * kk;
        float l = 0, rr = 0;
        for (auto& h : g_hop) {
          h.t += dt;
          if (h.t > h.dur + h.gap) {
            h.t = 0;
            h.dur = 0.3f + h.rnd() * 1.2f;
            h.gap = h.rnd() < 0.3f ? 0.25f + h.rnd() * 0.4f : 1.5f + h.rnd() * 6.0f;   // sometimes a quick repeat, mostly a long wait
            h.amHz = 48 + h.rnd() * 40;
            h.gain = 0.45f + 0.55f * h.rnd();
          }
          if (h.t >= h.dur) continue;
          float u = h.t / h.dur;
          float env = std::min(1.0f, u * 6.0f) * std::min(1.0f, (1.0f - u) * 9.0f);   // the rasp swells in and stops short
          float ph = h.t * h.amHz; ph -= std::floor(ph);
          float syl = ph < 0.6f ? (float)std::sin(ph / 0.6f * 3.14159265f) : 0.0f;    // leg strokes: separate little scrapes
          float sm = h.bq.run(g_noise[h.pos++ % g_noise.size()]) * env * syl * syl * h.gain;
          l += sm * std::sqrt(0.5f * (1 - h.pan)); rr += sm * std::sqrt(0.5f * (1 + h.pan));
        }
        o[i * 2] += l * g_dayBugGain * sv; o[i * 2 + 1] += rr * g_dayBugGain * sv;
      }
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
  // smooth limiter: loud moments (thunder) turn the whole mix down briefly instead of
  // hard-saturating it into distortion; tanh only catches what slips through
  static float lim = 1.0f;
  for (ma_uint32 i = 0; i < frames; i++) {
    float pk = std::max(std::fabs(o[i * 2]), std::fabs(o[i * 2 + 1])) * lim;
    if (pk > 0.8f) lim *= 0.8f / pk;                    // instant attack
    else lim += (1.0f - lim) * 0.00004f;                 // ~0.5 s release
    o[i * 2] = std::tanh(o[i * 2] * lim);
    o[i * 2 + 1] = std::tanh(o[i * 2 + 1] * lim);
  }
  g_time = tEnd;
}

double now() { return g_time.load() + 0.03; }

void push(const Voice& v) {
  if (!g_ok) return;
  std::lock_guard<std::mutex> lk(g_mtx);
  if (g_pending.size() < 64 && g_voices.size() < 160) g_pending.push_back(v);  // hard cap: the mixer never falls behind
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

// recorded animal calls: assets/sounds/animals/<animal>_<warn|attack>*.wav|mp3|flac (any number of variants)
const char* ANIMAL_NAMES[6] = {"deer", "boar", "goral", "wolf", "bear", "leopard"};
std::vector<Clip> g_animalClips[6][2];
void loadAnimalClips() {
  std::string dir = platform::assetPath("sounds/animals");
  ma_decoder_config dc = ma_decoder_config_init(ma_format_f32, 2, SR);
  int n = 0;
  for (const char* ext : {".wav", ".mp3", ".flac"})
    for (auto& f : platform::listFiles(dir, ext)) {
      std::string base = f.substr(f.find_last_of("/\\") + 1);
      for (int sp = 0; sp < 6; sp++)
        for (int c = 0; c < 2; c++) {
          std::string pre = std::string(ANIMAL_NAMES[sp]) + (c ? "_attack" : "_warn");
          if (base.rfind(pre, 0) != 0) continue;
          ma_uint64 count = 0; void* data = nullptr;
          if (ma_decode_file(f.c_str(), &dc, &count, &data) != MA_SUCCESS || !data) continue;
          Clip cl; cl.pcm.assign((const float*)data, (const float*)data + count * 2);
          ma_free(data, nullptr);
          // normalise to a common peak so every recording sits at the same level in the mix
          float pk = 0; for (float x : cl.pcm) pk = std::max(pk, std::fabs(x));
          if (pk > 1e-4f) for (float& x : cl.pcm) x *= 0.9f / pk;
          g_animalClips[sp][c].push_back(std::move(cl)); n++;
        }
    }
  std::printf("audio: %d animal recordings\n", n);
  // ambience and foley: every file in assets/sounds/ambient and /foley, keyed by its name without trailing digits
  int m = 0;
  for (const char* sub : {"sounds/ambient", "sounds/foley"})
    for (const char* ext : {".wav", ".mp3", ".flac"})
      for (auto& f : platform::listFiles(platform::assetPath(sub), ext)) {
        std::string base = f.substr(f.find_last_of("/\\") + 1);
        base = base.substr(0, base.find_last_of('.'));
        while (!base.empty() && std::isdigit((unsigned char)base.back())) base.pop_back();
        ma_uint64 count = 0; void* data = nullptr;
        if (ma_decode_file(f.c_str(), &dc, &count, &data) != MA_SUCCESS || !data) continue;
        Clip cl; cl.pcm.assign((const float*)data, (const float*)data + count * 2);
        ma_free(data, nullptr);
        g_bank[base].push_back(std::move(cl)); m++;
      }
  const char* LN[L_COUNT] = {"day_outside", "birds_water", "stream", "rain_night", "storm"};
  for (int i = 0; i < L_COUNT; i++) if (auto* b = bank(LN[i])) for (auto& c : *b) g_loops[i].clips.push_back(&c);
  std::printf("audio: %d ambience/foley recordings\n", m);
}

namespace audio {

bool init() {
  g_noise.resize(SR * 2);
  float last = 0;
  for (auto& s : g_noise) {
    float w = rnd() * 2 - 1;
    last = (last + 0.04f * w) / 1.04f;
    s = last * 3.2f * 0.6f + w * 0.4f;
  }
  int types[7] = {0, 2, 0, 2, 0, 2, 0};
  float freqs[7] = {420, 760, 210, 2100, 520, 4200, 700}, qs[7] = {0.6f, 0.55f, 0.7f, 2.2f, 0.5f, 0.45f, 0.6f};
  for (int i = 0; i < 7; i++) {
    g_beds[i].type = types[i];
    g_beds[i].freq = g_beds[i].freqTarget = freqs[i];
    g_beds[i].q = qs[i];
    g_beds[i].bq.set(types[i], freqs[i], qs[i]);
    g_beds[i].pos = (size_t)(rnd() * g_noise.size());
  }
  g_beds[3].tau = 0.05f;
  // cricket field: each one a different pitch, rhythm and position; two tree crickets trill continuously
  for (int i = 0; i < 9; i++) {
    Cricket c;
    c.trill = i < 2;
    c.f = c.trill ? 2600 + rnd() * 600 : 3900 + rnd() * 1100;
    c.pan = (i / 8.0f) * 1.6f - 0.8f + (rnd() - 0.5f) * 0.15f;
    c.pulseHz = c.trill ? 42 + rnd() * 14 : 26 + rnd() * 18;
    c.basePulses = 2.5f + rnd() * 2.5f;
    c.baseGap = 0.35f + rnd() * 0.7f;
    c.dist = rnd() * 0.85f;  // most of the chorus is some way off
    c.gain = (c.trill ? 0.0035f : 0.007f) * (1.15f - c.dist * 0.75f);
    c.rng = 1234567u + i * 7919u;
    c.t = rnd() * 2.0f;
    g_crick.push_back(c);
  }
  for (int i = 0; i < 4; i++) {
    Hopper& h = g_hop[i];
    h.bq.set(2, 6500 + rnd() * 3500, 2.2f);
    h.pan = (i / 3.0f) * 1.5f - 0.75f;
    h.rng = 99991u + i * 104729u;
    h.gap = 1 + rnd() * 5;
    h.pos = (size_t)(rnd() * g_noise.size());
  }
  g_bugBq.set(2, 6400, 1.3f);
  g_bugBq2.set(2, 8600, 1.6f);
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
  loadAnimalClips();
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
// surface footsteps: soft swish on grass, crunch on snow, splash in shallow water
void footstepGrass() {
  if (playClip("step_grass", 0.32f * 0.8f * vary(0.12f), (rnd() * 2 - 1) * 0.1f)) return;
  float v = vary(0.12f);
  double t = now();
  aburst(t, 1, 3200 * v, 0.7f, 0.07f, 0.035f, (rnd() * 2 - 1) * 0.3f, 0.006f);
  aburst(t, 0, 420 * v, 0.8f, 0.05f, 0.03f, 0, 0.003f);
}
void footstepSnow() {
  float v = vary(0.1f);
  double t = now();
  for (int i = 0; i < 4; i++) aburst(t + i * 0.012, 2, 1700 * v + i * 260, 2.0f, 0.03f, 0.04f, (rnd() * 2 - 1) * 0.3f, 0.002f);
  aburst(t, 0, 260 * v, 0.7f, 0.1f, 0.035f, 0, 0.01f);
}
void footstepWater() {
  float v = vary(0.12f);
  double t = now();
  aburst(t, 2, 900 * v, 1.2f, 0.16f, 0.06f, (rnd() * 2 - 1) * 0.3f, 0.004f);
  aburst(t + 0.03, 1, 2400 * v, 0.8f, 0.2f, 0.035f, (rnd() * 2 - 1) * 0.3f, 0.01f);
}
// a predator has spotted you: low thump + rising strings stab (rate-limited)
bool dangerSting() {
  // "you've been spotted" stinger, built like stealth-game detection cues:
  // rising whoosh -> low boom + dissonant brass stab + high string screech -> two heartbeat thumps
  static double last = -100;
  double t = now();
  if (t - last < 8) return false;
  last = t;
  {
    Voice v{};  // riser: swelling band-passed noise sweeping upward
    v.kind = V_NOISE; v.env = E_TONE; v.start = t; v.dur = 0.34f; v.attack = 0.3f; v.gain = 0.22f;
    v.ftype = 2; v.nf0 = 300; v.nf1 = 4200; v.q = 1.2f; v.phase = rnd() * g_noise.size();
    push(v);
  }
  double h = t + 0.3;
  aburst(h, 0, 55, 0.9f, 0.9f, 0.55f, 0, 0.003f);                           // boom
  aburst(h, 2, 160, 1.5f, 0.12f, 0.25f, 0, 0.001f);                         // impact click
  const float chord[4] = {220.0f, 233.1f, 329.6f, 466.2f};                  // minor 2nd + tritone: alarm
  for (int i = 0; i < 4; i++) atone(h, Wave::Sawtooth, chord[i], chord[i] * 0.94f, 0.55f, 0.07f, (i - 1.5f) * 0.25f, 0);
  atone(h + 0.02, Wave::Triangle, 1760, 1990, 0.7f, 0.05f, 0.1f, 14);        // screech
  aburst(h + 0.55, 0, 48, 0.9f, 0.22f, 0.4f, 0, 0.002f);                    // heartbeat
  aburst(h + 0.78, 0, 44, 0.9f, 0.25f, 0.33f, 0, 0.002f);
  return true;
}
std::atomic<float> g_indoor{0};
void setIndoor(float k) { g_indoor = std::max(0.0f, std::min(1.0f, k)); }
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
// ---- bows: built from filtered noise and low, short plucked tones (no bright oscillators: those read as "laser")
namespace {
void pluck(double at, Wave w, float f0, float f1, float dur, float gain, float pan) {  // instant attack, ringing decay
  Voice v{};
  v.kind = V_OSC; v.wave = w; v.env = E_BLIP; v.start = at; v.dur = dur; v.f0 = f0; v.f1 = f1; v.gain = gain; v.pan = pan;
  push(v);
}
void sweep(double at, float f0, float f1, float q, float dur, float gain, float pan, float attack) {  // band-passed noise gliding f0->f1
  Voice v{};
  v.kind = V_NOISE; v.env = E_TONE; v.start = at; v.dur = dur; v.gain = gain * 2.0f; v.attack = attack;
  v.ftype = 2; v.nf0 = f0; v.nf1 = f1; v.q = q; v.pan = pan; v.phase = rnd() * g_noise.size();
  push(v);
}
}  // namespace
void bowCreak(float g, float pan) {
  if (g <= 0.002f) return;
  double t = now();
  // a real draw is nearly silent: a soft rustle as the arrow is nocked, then a faint low stretch of the limbs
  sweep(t, 1800, 1200, 0.9f, 0.14f, 0.02f * g * std::sqrt(g), pan, 0.05f);
  sweep(t + 0.2, 240, 330, 2.5f, 0.45f, 0.022f * g, pan, 0.3f);
}
void bowRelease(float g, float pan) {
  if (g <= 0.002f) return;
  double t = now();
  float hi = std::sqrt(g);
  // "thwip": one soft-edged slap of the string, a dull thump of the limbs, and the arrow's breath leaving
  aburst(t, 2, 900 * vary(0.08f), 0.7f, 0.04f, 0.26f * g, pan, 0.002f);
  aburst(t, 0, 160, 0.7f, 0.1f, 0.5f * g, pan, 0.003f);
  sweep(t + 0.005, 1700, 600, 0.9f, 0.22f, 0.09f * g * hi, pan, 0.03f);
}
void arrowWhiz(float g, float pan) {
  if (g <= 0.002f) return;
  double t = now();
  // one smooth breathy pass, pitch falling as it goes by
  sweep(t, 2200, 700, 1.1f, 0.28f, 0.42f * g, pan, 0.11f);
  sweep(t, 900, 350, 0.8f, 0.3f, 0.15f * g, pan, 0.12f);
}
void arrowHitBody() {
  double t = now();
  aburst(t, 0, 170, 0.9f, 0.11f, 0.45f, 0, 0.0008f);         // thud into the body
  aburst(t, 2, 1300, 1.0f, 0.05f, 0.12f, 0, 0.0008f);        // cloth tearing
  sweep(t + 0.06, 900, 520, 2.0f, 0.26f, 0.08f, 0, 0.03f);  // pained breath out
  pluck(t + 0.06, Wave::Sine, 150, 118, 0.22f, 0.035f, 0);
}
void arrowThunk(float g, float pan, bool wood) {
  if (g <= 0.002f) return;
  double t = now();
  float hi = std::sqrt(g);
  if (wood) {
    aburst(t, 2, 1500 * vary(0.1f), 1.0f, 0.012f, 0.2f * g * hi, pan, 0.0008f);  // point biting the bark
    aburst(t, 2, 380 * vary(0.1f), 2.2f, 0.09f, 0.42f * g, pan, 0.001f);         // hollow knock of the trunk
    aburst(t, 0, 140, 0.7f, 0.08f, 0.25f * g, pan, 0.002f);
  } else {
    aburst(t, 0, 200, 0.8f, 0.07f, 0.32f * g, pan, 0.002f);                      // dull thud into soil
    aburst(t, 2, 2200, 0.8f, 0.04f, 0.06f * g * hi, pan, 0.003f);                // leaf litter
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


// ---- animal voices (synthesised): sp 1 boar, 3 wolf, 4 bear, 5 leopard; call 0 = warning, 1 = attack ----
bool playClip(const char* name, float gain, float pan, double delay) { return playClipRate(name, gain, pan, delay, 1.0f); }
bool playClipRate(const char* name, float gain, float pan, double delay, float rate) {
  const std::vector<Clip>* b = bank(name);
  if (!b || !g_ok || g_sfxMuted) return false;
  const Clip& c = (*b)[(size_t)(rnd() * b->size()) % b->size()];
  Voice vo{};
  vo.kind = V_SAMPLE; vo.env = E_TONE; vo.start = now() + delay; vo.clip = &c;
  vo.rate = rate;
  vo.dur = (float)(c.pcm.size() / 2) / (SR * rate); vo.gain = gain; vo.pan = pan;
  push(vo);
  return true;
}
void foliageRustle(float gain) { playClip("foliage", gain * 0.8f * vary(0.1f), (rnd() * 2 - 1) * 0.3f); }

float animalVoiceLen(int sp, int call) {
  if (sp < 0 || sp >= 6) return 1.0f;
  auto& v = g_animalClips[sp][call];
  auto& use = !v.empty() ? v : g_animalClips[sp][1 - call];
  if (use.empty()) return 1.5f;
  float m = 0; for (auto& c : use) m = std::max(m, (float)(c.pcm.size() / 2) / SR);
  return m;
}
void animalVoice(int sp, int call, float gain, float pan, float rate, float delay) {
  if (!g_ok || g_sfxMuted) return;
  gain *= 0.8f;
  double t = now() + delay;
  if (sp >= 0 && sp < 6) {
    auto& v = g_animalClips[sp][call];
    auto& alt = g_animalClips[sp][1 - call];
    const std::vector<Clip>& use = !v.empty() ? v : alt;   // one kind recorded: use it for both
    if (!use.empty()) {
      const Clip& c = use[(size_t)(rnd() * use.size()) % use.size()];
      Voice vo{};
      vo.kind = V_SAMPLE; vo.env = E_TONE; vo.start = t; vo.clip = &c;
      vo.rate = rate;   // t already includes the travel delay
      vo.dur = (float)(c.pcm.size() / 2) / (SR * rate); vo.gain = gain * 0.6f; vo.pan = pan;
      push(vo);
      return;
    }
  }
  float g = gain;
  if (sp == 3) {            // wolf
    if (call == 0) {        // snarl, then a short rising howl
      aburst(t, 2, 900, 3.0f, 0.5f, g * 0.25f, pan, 0.05f);
      atone(t, Wave::Sawtooth, 150, 120, 0.5f, g * 0.35f, pan, 18);
      atone(t + 0.45, Wave::Sine, 380, 620, 0.9f, g * 0.5f, pan, 6);
      atone(t + 1.3, Wave::Sine, 620, 450, 0.9f, g * 0.42f, pan, 6);
    } else {                // bite: a sharp bark and snapping jaws
      atone(t, Wave::Sawtooth, 520, 260, 0.13f, g * 0.6f, pan);
      aburst(t, 2, 1800, 2.0f, 0.1f, g * 0.45f, pan, 0.003f);
      atone(t + 0.16, Wave::Sawtooth, 180, 120, 0.35f, g * 0.4f, pan, 22);
      aburst(t + 0.16, 2, 700, 2.5f, 0.35f, g * 0.25f, pan, 0.02f);
    }
  } else if (sp == 4) {     // bear: deep, rough, long
    float L = call ? 1.1f : 0.9f, k = call ? 1.0f : 0.7f;
    atone(t, Wave::Sawtooth, 95, 70, L, g * 0.55f * k, pan, 28);
    atone(t, Wave::Square, 48, 40, L, g * 0.3f * k, pan, 14);
    aburst(t, 0, 420, 1.2f, L, g * 0.4f * k, pan, 0.08f);
    aburst(t + 0.05, 2, 260, 4.0f, L * 0.9f, g * 0.3f * k, pan, 0.1f);
    if (call) atone(t + 0.1, Wave::Sawtooth, 160, 90, 0.6f, g * 0.35f, pan, 30);   // roar on the swipe
  } else if (sp == 5) {     // leopard: the rasping "sawing" call, a snarl-hiss when it strikes
    if (call == 0) {
      for (int i = 0; i < 5; i++) {
        double tt = t + i * 0.42;
        atone(tt, Wave::Sawtooth, 135, 105, 0.22f, g * 0.45f, pan, 35);
        aburst(tt, 2, 650, 2.0f, 0.22f, g * 0.35f, pan, 0.01f);
        atone(tt + 0.24, Wave::Sawtooth, 115, 95, 0.14f, g * 0.28f, pan, 30);
      }
    } else {
      aburst(t, 1, 2600, 0.8f, 0.4f, g * 0.45f, pan, 0.01f);   // hiss
      atone(t + 0.05, Wave::Sawtooth, 260, 140, 0.45f, g * 0.55f, pan, 26);
      aburst(t + 0.05, 2, 900, 2.0f, 0.45f, g * 0.35f, pan, 0.02f);
    }
  } else if (sp == 1) {     // boar: grunts when bothered, a squeal and a snort when it charges
    if (call == 0) {
      for (int i = 0; i < 3; i++) {
        double tt = t + i * 0.28;
        atone(tt, Wave::Square, 140, 95, 0.16f, g * 0.4f, pan, 20);
        aburst(tt, 0, 500, 1.5f, 0.16f, g * 0.3f, pan, 0.01f);
      }
    } else {
      atone(t, Wave::Sawtooth, 700, 1150, 0.35f, g * 0.45f, pan, 25);
      atone(t, Wave::Square, 360, 520, 0.35f, g * 0.25f, pan, 25);
      aburst(t + 0.38, 2, 1200, 1.2f, 0.2f, g * 0.4f, pan, 0.01f);   // snort
      atone(t + 0.4, Wave::Square, 120, 90, 0.2f, g * 0.35f, pan, 20);
    }
  }
}
}  // namespace audio

namespace ambience {

namespace {
float dropT = 0, babbleT = 0, dayT = 4, extraT = 6, crackleT = 1, nextEvent = 3, beatT = 0;
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
// more song types so the forest never repeats one call: a rapid trill, a slurred whistle, a two-note
// "tee-too" and a chattering descending run; each comes from somewhere in the stereo field at its own distance
void songbird() {
  float pan = pr(), g = 0.012f + rnd() * 0.022f;  // near or distant
  double t = now();
  int kind = (int)(rnd() * 4);
  if (kind == 0) {  // trill
    float f = 3200 + rnd() * 1800; int n = 8 + (int)(rnd() * 12);
    for (int i = 0; i < n; i++) { atone(t, Wave::Sine, f, f * 1.15f, 0.035f, g * 0.8f, pan); t += 0.045; }
  } else if (kind == 1) {  // slurred whistle
    float f = 1800 + rnd() * 1200;
    atone(t, Wave::Sine, f, f * (1.4f + rnd() * 0.4f), 0.35f, g, pan, 25);
    atone(t + 0.45, Wave::Sine, f * 1.5f, f * 0.9f, 0.4f, g * 0.8f, pan, 25);
  } else if (kind == 2) {  // tee-too, repeated
    float f = 2800 + rnd() * 900; int reps = 2 + (int)(rnd() * 3);
    for (int i = 0; i < reps; i++) { atone(t, Wave::Sine, f, f, 0.12f, g, pan); atone(t + 0.16, Wave::Sine, f * 0.75f, f * 0.72f, 0.16f, g * 0.9f, pan); t += 0.5; }
  } else {  // chatter running down
    float f = 4800 + rnd() * 1200; int n = 6 + (int)(rnd() * 6);
    for (int i = 0; i < n; i++) { atone(t, Wave::Sine, f, f * 0.85f, 0.05f, g * 0.7f, pan); f *= 0.94f; t += 0.07 + rnd() * 0.03; }
  }
}
void crickets() {
  // one close cricket over the chorus: soft sine pulses with varied spacing (the old square wave was harsh)
  float pan = rnd() * 1.4f - 0.7f, f = 4100 + rnd() * 700;
  double t = now();
  int chirps = 2 + (int)(rnd() * 3);
  for (int k = 0; k < chirps; k++) {
    int n = 2 + (int)(rnd() * 3);
    for (int i = 0; i < n; i++) { atone(t, Wave::Sine, f, f * 0.985f, 0.022f, 0.0035f * (0.75f + 0.25f * rnd()), pan); t += 0.032 + rnd() * 0.008; }
    t += 0.28 + rnd() * 0.35;
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

void thunder(float delay, float dist) {
  if (!g_ok || g_sfxMuted) return;
  double t = now() + delay;
  float g = 0.28f * (1.1f - dist);
  float pan = rnd() * 1.2f - 0.6f;
  if (dist < 0.5f) aburst(t, 2, 1800, 0.7f, 0.25f, g * 0.5f, pan, 0.003f);  // close strike: sharp crack first
  // long low rumble from a few filtered noise rolls, spaced so they don't pile up into one huge peak
  int rolls = 3 + (int)(rnd() * 3);
  for (int k = 0; k < rolls; k++) {
    double at = t + 0.1 + k * (0.55 + rnd() * 0.5);
    aburst(at, 0, 90 + rnd() * 120, 0.7f, 1.4f + rnd() * 1.4f, g * (1.0f - k * 0.15f), pan + (rnd() - 0.5f) * 0.4f, 0.12f);
  }
}

void howl(float dist) {
  if (!g_ok || g_sfxMuted) return;
  if (audio::playClip("wolf_howl_night", std::max(0.04f, 0.5f * (1 - std::min(1.0f, dist / 260))), rnd() * 1.2f - 0.6f)) return;
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
  w *= 1 + 3.0f * sky::blizzard();  // blizzard wind roar
  g_beds[0].target = w;
  g_beds[0].freqTarget = 300 + gust * 380 + season.fall * 200;
  RiverHit q = world::riverQuery(p.x, p.z);
  float rd = q.d < 1e8f ? std::max(0.0f, q.d - q.hw) : 1e4f;
  float frozen = 1 - 0.75f * std::min(1.0f, season.snow);
  float nearAmt = std::pow(std::max(0.0f, 1 - rd / 75), 2.0f) * frozen * on * (1 + q.flow * 0.6f);
  // lakes are still water: near a lake the rushing-river bed fades out and only a soft lapping remains
  // (the river's ends run into the lakes, so its nearest point used to be right under the lake)
  float lakeAmt = 0, lakeNear = 0;
  for (const Lake& L : world::LAKES) {
    float ld = std::max(0.0f, world::lakeDist(L, p.x, p.z) - L.r);
    lakeAmt = std::max(lakeAmt, std::pow(std::max(0.0f, 1 - ld / 40), 2.0f) * 0.3f * frozen * on);
    lakeNear = std::max(lakeNear, 1 - clampf((ld - 25) / 45, 0, 1));
  }
  nearAmt *= 1 - 0.9f * lakeNear;
  g_beds[1].target = nearAmt * 0.32f;
  g_beds[2].target = nearAmt * 0.35f + lakeAmt * 0.25f;
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
  // rain: steady hiss + body, scaled by strength; heavier drips under tree canopy
  float rain = sky::rainAmount() * on;
  float canopy = envmap::canopyAt(p.x, p.z);  // baked (envmap)
  float in = audio::g_indoor.load();  // under a roof: rain hiss and wind are muffled, the drum on the roof stays
  g_beds[5].target = rain * 0.22f * (1 - 0.85f * in);
  g_beds[6].target = rain * (0.1f + canopy * 0.08f) * (1 - 0.35f * in);
  g_beds[6].freqTarget = 700 - 350 * in;
  g_beds[0].target *= 1 - 0.7f * in;
  if (hasLoops()) {
    // recorded beds: day birdsong outdoors, birds by the water, the stream near the river, rain (night) and storm
    float day = sstep(E.daylight, 0.2f, 0.5f), dry = 1 - std::min(1.0f, rain * 2.5f), cold = 1 - 0.7f * std::min(1.0f, season.snow);
    float out = 1 - 0.8f * in;
    g_loops[L_DAY].target = 0.22f * day * dry * cold * out * on;
    g_loops[L_BIRDSWATER].target = (nearAmt + lakeAmt) * 0.4f * day * dry * cold * out;
    g_loops[L_STREAM].target = std::min(0.6f, nearAmt * 0.55f) * out;
    g_loops[L_RAINNIGHT].target = rain * (1 - day) * 0.45f * (1 - 0.5f * in);
    g_loops[L_STORM].target = rain * day * 0.42f * (1 - 0.5f * in);
    // the synthetic beds step back where a recording covers them
    if (!g_loops[L_STREAM].clips.empty()) { g_beds[1].target *= 0.35f; g_beds[2].target *= 0.35f; g_beds[3].target *= 0.3f; }
    if (!g_loops[L_STORM].clips.empty()) { g_beds[5].target *= 0.4f; g_beds[6].target *= 0.5f; }
  }
  {
    // baked sound zones: open ridges and meadows catch the full wind, forest and gorges muffle it and cut its hiss
    float open = envmap::openAt(p.x, p.z), cov = envmap::canopyAt(p.x, p.z);
    g_beds[0].target *= (0.55f + 0.55f * open) * (1 - 0.4f * cov);
    g_beds[0].freqTarget *= 1 - 0.3f * cov;
  }
  if (rain > 0.05f && E.playing && in < 0.5f) {
    dropT -= dt;
    if (dropT < -0.25f) dropT = 0;  // after a hitch, don't fire a backlog of drops in one go
    while (dropT <= 0) {
      dropT += 0.02f + rnd() * (0.12f - 0.08f * rain);
      float dg = 0.012f + rnd() * 0.02f;
      double at = now() + rnd() * 0.02;
      if (rnd() < 0.25f + canopy * 0.4f) aburst(at, 2, 900 + rnd() * 900, 3.0f, 0.05f, dg * 1.4f, rnd() * 1.6f - 0.8f, 0.001f);  // fat drip off leaves
      else aburst(at, 2, 2500 + rnd() * 3500, 2.5f, 0.025f, dg, rnd() * 1.6f - 0.8f, 0.001f);   // drop on grass / ground
    }
  }

  {
    // constant insect layer: cricket chorus fills the night (and murmurs by day), grasshoppers hiss on sunny days;
    // winter silences both, rain and being indoors damp them
    float dayL = E.daylight;
    float night = 1 - std::min(1.0f, std::max(0.0f, (dayL - 0.15f) / 0.45f));
    float warm = std::max(0.0f, 1 - season.snow * 1.2f) * (1 - 0.45f * season.fall);
    float wet = 1 - 0.7f * std::min(1.0f, sky::rainAmount() * 1.5f);
    float grass = 0.6f + 0.4f * envmap::openAt(p.x, p.z);
    float act = (E.playing && !E.uiOpen) ? 1.0f : 0.25f;
    g_crickTarget = 0.55f * (0.3f + 0.8f * night) * warm * wet * grass * (1 - 0.6f * in) * act * (p.y > 70 ? 0.4f : 1.0f);
    g_dayBugTarget = 0.0f;  // grasshoppers removed at the player's request (the synth stays, silent)
  }
  if (!E.playing || E.uiOpen) return;
  float day = E.daylight;
  float forest = envmap::canopyAt(p.x, p.z);
  nextEvent -= dt;
  if (nextEvent <= 0) {
    float r2 = rnd();
    if (day > 0.3f) {
      // daytime: birds almost always somewhere; mornings busiest (dawn chorus), quieter in snow and rain
      float birdy = 0.6f + season.spring * 0.35f + season.dry * 0.2f - season.snow * 0.35f - sky::rainAmount() * 0.4f;
      float morning = std::max(0.0f, 1 - std::fabs(E.time - 7.0f) / 3.0f);
      if (r2 < birdy) { if (rnd() < 0.55f) songbird(); else birds(); }
      if (morning > 0.3f && rnd() < morning * 0.6f) songbird();
      nextEvent = (0.9f + rnd() * 2.6f) * (1.4f - 0.6f * morning) * (1 + season.snow);
    } else if (day < 0.2f) {
      if (r2 < 0.5f && season.snow < 0.5f) crickets();  // a near cricket chirping over the chorus
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
    } else if (day < 0.25f && std::max(nearAmt, envmap::wetAt(p.x, p.z) * 0.5f) > 0.15f && (season.spring > 0.3f || season.dry > 0.3f)) frogs(std::min(1.0f, std::max(nearAmt, envmap::wetAt(p.x, p.z) * 0.5f) * 2));  // marshes croak too
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
