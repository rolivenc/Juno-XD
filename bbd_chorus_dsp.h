#pragma once
/*
 * BBD Chorus - Juno-60/106 inspired chorus DSP core (platform independent)
 *
 * Shared by the minilogue xd (modfx) and NTS-3 kaoss pad (genericfx) builds.
 * The platform wrappers own the delay buffer (it must live in SDRAM on the
 * minilogue xd) and translate their parameter formats into setMode/setMix.
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
 * Rates, delay times and filter corners are approximations: tune the
 * constants below against a recording of a real Juno. The defaults can also
 * be changed at runtime through the setters on Chorus (the NTS-3 build
 * exposes them as unit parameters; the minilogue xd build uses the defaults).
 */

#include <stdint.h>
#include <math.h>

// Mode depth glide: when 1, switching modes glides the modulation depth to
// the new mode's value (same ~10 ms smoothing as the depth parameter) instead
// of jumping, which avoids a pitch jump/click between II and I+II.
// Set to 0 to restore the original instant switch. Can also be set from the
// build: UDEFS = -DBBD_MODE_GLIDE=0 (project.mk / nts-3/config.mk).
#ifndef BBD_MODE_GLIDE
#define BBD_MODE_GLIDE 1
#endif

namespace bbd {

constexpr float    kFs     = 48000.f;       // both platforms run at 48 kHz
constexpr uint32_t kBufLen = 1024;          // power of two
constexpr uint32_t kMask   = kBufLen - 1;
constexpr float    kPi     = 3.14159265358979f;

// ---- tunable constants ------------------------------------------------
constexpr float kPreFc    = 9000.f;   // pre-BBD anti-alias LPF
constexpr float kPostFc   = 7500.f;   // post-BBD reconstruction LPF
constexpr float kCenterMs = 3.5f;     // centre delay time, shared by all modes
constexpr float kWetGain  = 0.90f;    // wet/dry ratio, measured from two recordings (0.85 / 0.95)
constexpr float kOutTrim  = 0.75f;    // ~-2.5 dB trim to compensate for dry+wet summing
constexpr float kNoiseAmt = 0.0015f;  // BBD noise at Grit = 100%
constexpr float kGrit     = 0.25f;    // fixed saturation amount, and default noise amount

// Limits for the runtime setters
constexpr float kMinFc = 500.f;
constexpr float kMaxFc = 20000.f;

constexpr uint8_t kNumModes = 3;
constexpr bool    kModeGlide = (BBD_MODE_GLIDE != 0);

struct ModeCfg { float rate_hz, depth_ms; };
constexpr ModeCfg kModes[kNumModes] = {
  {0.51f, 2.10f},  // I    (delay sweeps ~1.4-5.6 ms; Jun-6 Chorus I measured ~0.41 Hz)
  {0.83f, 2.10f},  // II   (Juno recording measured ~0.81 Hz)
  {9.75f, 0.30f},  // I+II (not measured)
};

inline float clampf(float x, float lo, float hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

// sin(x) for |x| <= pi/2, Taylor series to x^11 (error < 1e-7).
// Used instead of libm sinf/cosf, whose argument reduction tables alone
// take ~1 KB of the minilogue xd's 6 KB modfx SRAM.
inline float sinPoly(float x) {
  const float x2 = x * x;
  return x * (1.f + x2 * (-1.f / 6.f + x2 * (1.f / 120.f + x2 * (-1.f / 5040.f
           + x2 * (1.f / 362880.f + x2 * (-1.f / 39916800.f))))));
}

// ---- biquad low-pass --------------------------------------------------
struct Biquad {
  float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
  float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;

  // fc must be below Nyquist (w in [0, pi)); setters clamp to kMaxFc.
  void lowpass(float fc, float q) {
    const float w     = 2.f * kPi * fc / kFs;
    const float sw    = sinPoly(w > 0.5f * kPi ? kPi - w : w);
    const float cw    = sinPoly(0.5f * kPi - w);
    const float alpha = sw / (2.f * q);
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

class Chorus {
public:
  // buf must point to kBufLen floats that stay valid while the chorus runs.
  void init(float *buf) {
    buf_ = buf;

    preFc_  = kPreFc;
    postFc_ = kPostFc;
    preDirty_ = postDirty_ = false;
    updatePre();
    updatePost();

    timeMult_    = 1.f;
    depthTarget_ = depth_ = 1.f;   // Juno-style depth
    centerTarget_ = center_ = kCenterMs * 0.001f * kFs;
    mixTarget_   = mix_   = 1.f;
    grit_        = kGrit;
    noiseAmt_    = kGrit * kNoiseAmt;
    mode_        = 0;
    modeDepthTarget_ = modeDepth_ = modeDepthSamples(mode_);

    reset();
  }

  // Clears audio state (delay line, filters, LFO, noise) but keeps parameters.
  void reset() {
    if (buf_)
      for (uint32_t i = 0; i < kBufLen; ++i) buf_[i] = 0.f;
    wr_    = 0;
    phase_ = 0.f;
    noise_ = 0x12345678u;
    pre_.reset();
    for (int i = 0; i < 2; ++i) { postL_[i].reset(); postR_[i].reset(); }
  }

  void setMode(uint8_t mode) {
    mode_ = mode < kNumModes ? mode : kNumModes - 1;
    modeDepthTarget_ = modeDepthSamples(mode_);
    if (!kModeGlide) modeDepth_ = modeDepthTarget_;
  }
  uint8_t mode() const { return mode_; }

  // Wet level, 0 = dry, 1 = full Juno-style chorus. Smoothed internally.
  void setMix(float mix) { mixTarget_ = clampf(mix, 0.f, 1.f); }

  // LFO rate multiplier applied to the current mode's rate (1 = default).
  void setRateScale(float scale) { timeMult_ = clampf(scale, 0.f, 16.f); }

  // Modulation depth multiplier applied to the current mode's depth
  // (1 = default). Smoothed internally.
  void setDepthScale(float scale) { depthTarget_ = clampf(scale, 0.f, 4.f); }

  // Centre delay time in milliseconds. Smoothed internally. The swept delay
  // is clamped to what the delay line can hold (~21 ms).
  void setCenterMs(float ms) {
    centerTarget_ = clampf(ms, 0.f, 20.f) * 0.001f * kFs;
  }

  // Filter corners in Hz. Coefficients are recalculated at the start of the
  // next process() call, so these are safe to call between audio blocks.
  void setPreCutoff(float hz)  { preFc_  = clampf(hz, kMinFc, kMaxFc); preDirty_  = true; }
  void setPostCutoff(float hz) { postFc_ = clampf(hz, kMinFc, kMaxFc); postDirty_ = true; }

  // BBD noise level, 0 = off, 1 = kNoiseAmt (default is kGrit = 0.25).
  void setNoise(float amt) { noiseAmt_ = clampf(amt, 0.f, 1.f) * kNoiseAmt; }

  // Interleaved stereo in / out. in and out may alias.
  void process(const float *in, float *out, uint32_t frames) {
    if (preDirty_)  { preDirty_  = false; updatePre(); }
    if (postDirty_) { postDirty_ = false; updatePost(); }

    const ModeCfg &m   = kModes[mode_];
    const float    inc = m.rate_hz * timeMult_ / kFs;
    const float    drive      = grit_;
    const float    noiseAmt   = noiseAmt_;
    constexpr float kSmooth   = 0.002f;
    constexpr float kTiny     = 1e-15f;   // denormal guard
    // Hermite reads one sample behind and two ahead of the read position;
    // keep the taps inside the samples already written.
    constexpr float kMinDelay = 2.f;
    constexpr float kMaxDelay = (float)kBufLen - 3.f;

    for (uint32_t n = 0; n < frames; ++n) {
      const float inL = in[2 * n];
      const float inR = in[2 * n + 1];

      depth_     += kSmooth * (depthTarget_     - depth_);
      modeDepth_ += kSmooth * (modeDepthTarget_ - modeDepth_);   // no-op unless gliding
      center_ += kSmooth * (centerTarget_ - center_);
      mix_    += kSmooth * (mixTarget_    - mix_);

      // mono sum -> input saturation (blended by Grit) -> pre filter
      float mono = 0.5f * (inL + inR);
      mono += drive * (softsat(mono) - mono);
      const float filtered = pre_.process(mono + kTiny);

      buf_[wr_] = filtered;

      // opposite-phase triangle LFOs
      phase_ += inc;
      if (phase_ >= 1.f) phase_ -= 1.f;
      const float lfo = triangle(phase_);

      const float sweep = modeDepth_ * depth_ * lfo;
      const float dL = clampf(center_ + sweep, kMinDelay, kMaxDelay);
      const float dR = clampf(center_ - sweep, kMinDelay, kMaxDelay);

      float wL = readHermite((float)(wr_ + kBufLen) - dL);
      float wR = readHermite((float)(wr_ + kBufLen) - dR);

      // BBD noise, then reconstruction filters
      wL += noiseAmt * whiteNoise();
      wR += noiseAmt * whiteNoise();
      wL = postL_[1].process(postL_[0].process(wL));
      wR = postR_[1].process(postR_[0].process(wR));

      wr_ = (wr_ + 1) & kMask;

      const float wet = kWetGain * mix_;
      out[2 * n]     = kOutTrim * (inL + wet * wL);
      out[2 * n + 1] = kOutTrim * (inR + wet * wR);
    }
  }

private:
  static inline float modeDepthSamples(uint8_t mode) {
    return kModes[mode].depth_ms * 0.001f * kFs;
  }

  void updatePre() { pre_.lowpass(preFc_, 0.7071f); }

  // 4th-order Butterworth = two 2nd-order sections
  void updatePost() {
    for (int i = 0; i < 2; ++i) {
      const float q = (i == 0) ? 0.5412f : 1.3066f;
      postL_[i].lowpass(postFc_, q);
      postR_[i].lowpass(postFc_, q);
    }
  }

  // 4-point, 3rd-order Hermite interpolation. pos must be positive.
  inline float readHermite(float pos) const {
    const float    fl = floorf(pos);
    const float    f  = pos - fl;
    const uint32_t i  = (uint32_t)fl;
    const float xm1 = buf_[(i - 1) & kMask];
    const float x0  = buf_[i & kMask];
    const float x1  = buf_[(i + 1) & kMask];
    const float x2  = buf_[(i + 2) & kMask];
    const float c0 = x0;
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + c0;
  }

  static inline float triangle(float p) { return 4.f * fabsf(p - 0.5f) - 1.f; }

  // soft saturation with unity small-signal gain
  static inline float softsat(float x) {
    const float xc = clampf(x * 1.5f, -1.f, 1.f);
    return (xc - xc * xc * xc * (1.f / 3.f)) * (1.f / 1.5f);
  }

  inline float whiteNoise() {
    noise_ ^= noise_ << 13;
    noise_ ^= noise_ >> 17;
    noise_ ^= noise_ << 5;
    return (float)(int32_t)noise_ * (1.f / 2147483648.f);
  }

  float   *buf_ = nullptr;
  uint32_t wr_ = 0;
  float    phase_ = 0.f;
  uint32_t noise_ = 0x12345678u;

  Biquad pre_;
  Biquad postL_[2], postR_[2];

  // parameters (targets) and smoothed values
  float   timeMult_ = 1.f;
  float   depthTarget_ = 1.f, depth_ = 1.f;
  float   modeDepthTarget_ = kModes[0].depth_ms * 0.001f * kFs;
  float   modeDepth_       = kModes[0].depth_ms * 0.001f * kFs;
  float   centerTarget_ = kCenterMs * 0.001f * kFs, center_ = kCenterMs * 0.001f * kFs;
  float   mixTarget_ = 1.f,   mix_ = 1.f;
  float   grit_ = kGrit;
  float   noiseAmt_ = kGrit * kNoiseAmt;
  float   preFc_ = kPreFc, postFc_ = kPostFc;
  volatile bool preDirty_ = false, postDirty_ = false;
  uint8_t mode_ = 0;
};

} // namespace bbd
