/*
 * BBD Chorus - Juno-60/106 inspired chorus for the Korg minilogue xd (modfx)
 *
 * Architecture (approximation of the Juno chorus):
 *   mono sum -> input soft-saturation -> pre-BBD low-pass (anti-alias)
 *   -> single delay buffer, two taps modulated by a triangle LFO in
 *      opposite phase (left = +LFO, right = -LFO)
 *   -> BBD noise -> post-BBD low-pass (reconstruction, 4th order)
 *   -> added to the dry stereo signal
 *
 * Modes:  0 = Chorus I   (slow)
 *         1 = Chorus II  (slightly faster)
 *         2 = Chorus I+II (fast, shallow: near-vibrato shimmer)
 *
 * Knobs:  Time  = mode select, in three zones: I | II | I+II
 *         Depth = wet level (0 = dry, max = full Juno-style chorus)
 * (The xd modfx slot only exposes Time and Depth; no extra user params.)
 *
 * Rates, delay times and filter corners are approximations: tune the
 * constants below against a recording of a real Juno.
 */

#include "usermodfx.h"
#include "fixed_math.h"
#include <math.h>

namespace {

constexpr float    kFs     = 48000.f;
constexpr uint32_t kBufLen = 1024;          // power of two
constexpr uint32_t kMask   = kBufLen - 1;
constexpr float    kPi     = 3.14159265358979f;

// ---- tunable constants ------------------------------------------------
constexpr float kPreFc    = 9000.f;   // pre-BBD anti-alias LPF
constexpr float kPostFc   = 7500.f;   // post-BBD reconstruction LPF
constexpr float kWetGain  = 0.90f;    // wet/dry ratio, measured from two recordings (0.85 / 0.95)
constexpr float kOutTrim  = 0.75f;    // ~-2.5 dB trim to compensate for dry+wet summing
constexpr float kNoiseAmt = 0.0015f;  // BBD noise at Grit = 100%
constexpr float kGrit     = 0.25f;    // fixed saturation/noise amount

struct ModeCfg { float rate_hz, center_ms, depth_ms; };
constexpr ModeCfg kModes[3] = {
  {0.51f, 3.5f, 2.10f},  // I    (delay sweeps ~1.4-5.6 ms; Jun-6 Chorus I measured ~0.41 Hz)
  {0.83f, 3.5f, 2.10f},  // II   (Juno recording measured ~0.81 Hz)
  {9.75f, 3.5f, 0.30f},  // I+II (not measured)
};

// ---- biquad low-pass --------------------------------------------------
struct Biquad {
  float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
  float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;

  void lowpass(float fc, float q) {
    const float w     = 2.f * kPi * fc / kFs;
    const float cw    = cosf(w);
    const float alpha = sinf(w) / (2.f * q);
    const float inv   = 1.f / (1.f + alpha);
    b0 = (1.f - cw) * 0.5f * inv;
    b1 = (1.f - cw) * inv;
    b2 = b0;
    a1 = -2.f * cw * inv;
    a2 = (1.f - alpha) * inv;
  }
  void reset() { x1 = x2 = y1 = y2 = 0.f; }
  inline float process(float x) {
    const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x;
    y2 = y1; y1 = y;
    return y;
  }
};

// ---- state ------------------------------------------------------------
float    s_buf[kBufLen] __sdram;   // delay line lives in SDRAM (SRAM is only 6K)
uint32_t s_wr;
float    s_phase;
uint32_t s_noise;

Biquad s_pre;
Biquad s_postL[2], s_postR[2];

// parameters (targets) and smoothed values
float   s_timeMult;
float   s_depthTarget, s_depth;
float   s_mixTarget,   s_mix;
float   s_grit;
uint8_t s_mode;

inline float clampf(float x, float lo, float hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

// 4-point, 3rd-order Hermite interpolation. pos must be positive.
inline float readHermite(const float *b, float pos) {
  const float    fl = floorf(pos);
  const float    f  = pos - fl;
  const uint32_t i  = (uint32_t)fl;
  const float xm1 = b[(i - 1) & kMask];
  const float x0  = b[i & kMask];
  const float x1  = b[(i + 1) & kMask];
  const float x2  = b[(i + 2) & kMask];
  const float c0 = x0;
  const float c1 = 0.5f * (x1 - xm1);
  const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
  const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
  return ((c3 * f + c2) * f + c1) * f + c0;
}

inline float triangle(float p) { return 4.f * fabsf(p - 0.5f) - 1.f; }

// soft saturation with unity small-signal gain
inline float softsat(float x) {
  const float xc = clampf(x * 1.5f, -1.f, 1.f);
  return (xc - xc * xc * xc * (1.f / 3.f)) * (1.f / 1.5f);
}

inline float whiteNoise() {
  s_noise ^= s_noise << 13;
  s_noise ^= s_noise >> 17;
  s_noise ^= s_noise << 5;
  return (float)(int32_t)s_noise * (1.f / 2147483648.f);
}

} // namespace

// ---- logue SDK entry points -------------------------------------------

void MODFX_INIT(uint32_t platform, uint32_t api) {
  (void)platform; (void)api;
  for (uint32_t i = 0; i < kBufLen; ++i) s_buf[i] = 0.f;
  s_wr = 0;
  s_phase = 0.f;
  s_noise = 0x12345678u;

  s_pre.lowpass(kPreFc, 0.7071f);
  s_pre.reset();
  // 4th-order Butterworth = two 2nd-order sections
  for (int i = 0; i < 2; ++i) {
    const float q = (i == 0) ? 0.5412f : 1.3066f;
    s_postL[i].lowpass(kPostFc, q); s_postL[i].reset();
    s_postR[i].lowpass(kPostFc, q); s_postR[i].reset();
  }

  s_timeMult    = 1.f;
  s_depthTarget = s_depth = 1.f;   // fixed Juno-style depth
  s_mixTarget   = s_mix   = 1.f;
  s_grit        = kGrit;
  s_mode        = 0;
}

void MODFX_PROCESS(const float *main_xn, float *main_yn,
                   const float *sub_xn,  float *sub_yn,
                   uint32_t frames) {
  (void)sub_xn; (void)sub_yn;   // sub timbre not processed by user modfx here

  const ModeCfg &m   = kModes[s_mode];
  const float    inc = m.rate_hz * s_timeMult / kFs;
  const float    centerSamp = m.center_ms * 0.001f * kFs;
  const float    depthSamp  = m.depth_ms  * 0.001f * kFs;
  const float    drive      = s_grit;
  const float    noiseAmt   = s_grit * kNoiseAmt;
  constexpr float kSmooth   = 0.002f;
  constexpr float kTiny     = 1e-15f;   // denormal guard

  for (uint32_t n = 0; n < frames; ++n) {
    const float inL = main_xn[2 * n];
    const float inR = main_xn[2 * n + 1];

    s_depth += kSmooth * (s_depthTarget - s_depth);
    s_mix   += kSmooth * (s_mixTarget   - s_mix);

    // mono sum -> input saturation (blended by Grit) -> pre filter
    float mono = 0.5f * (inL + inR);
    mono += drive * (softsat(mono) - mono);
    const float filtered = s_pre.process(mono + kTiny);

    s_buf[s_wr] = filtered;

    // opposite-phase triangle LFOs
    s_phase += inc;
    if (s_phase >= 1.f) s_phase -= 1.f;
    const float lfo = triangle(s_phase);

    const float dL = centerSamp + depthSamp * s_depth * lfo;
    const float dR = centerSamp - depthSamp * s_depth * lfo;

    float wL = readHermite(s_buf, (float)(s_wr + kBufLen) - dL);
    float wR = readHermite(s_buf, (float)(s_wr + kBufLen) - dR);

    // BBD noise, then reconstruction filters
    wL += noiseAmt * whiteNoise();
    wR += noiseAmt * whiteNoise();
    wL = s_postL[1].process(s_postL[0].process(wL));
    wR = s_postR[1].process(s_postR[0].process(wR));

    s_wr = (s_wr + 1) & kMask;

    const float wet = kWetGain * s_mix;
    main_yn[2 * n]     = kOutTrim * (inL + wet * wL);
    main_yn[2 * n + 1] = kOutTrim * (inR + wet * wR);
  }
}

void MODFX_PARAM(uint8_t index, int32_t value) {
  const float valf = q31_to_f32(value);
  switch (index) {
    case k_user_modfx_param_time:                    // mode, 3 zones
      s_mode = (valf < 0.333f) ? 0 : (valf < 0.667f ? 1 : 2);
      break;
    case k_user_modfx_param_depth:                   // wet level
      s_mixTarget = clampf(valf, 0.f, 1.f);
      break;
    default:
      break;
  }
}
