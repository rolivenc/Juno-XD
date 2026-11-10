/*
 * BBD Chorus - Juno-60/106 inspired chorus for the Korg minilogue xd (modfx)
 *
 * minilogue xd wrapper around the shared DSP core in bbd_chorus_dsp.h.
 *
 * Knobs:  Time  = mode select, in three zones: I | II | I+II
 *         Depth = wet level (0 = dry, max = full Juno-style chorus)
 * (The xd modfx slot only exposes Time and Depth; no extra user params.)
 */

#include "usermodfx.h"
#include "fixed_math.h"
#include "bbd_chorus_dsp.h"

namespace {

float       s_buf[bbd::kBufLen] __sdram;   // delay line lives in SDRAM (SRAM is only 6K)
bbd::Chorus s_chorus;

} // namespace

// ---- logue SDK entry points -------------------------------------------

void MODFX_INIT(uint32_t platform, uint32_t api) {
  (void)platform; (void)api;
  s_chorus.init(s_buf);
}

void MODFX_PROCESS(const float *main_xn, float *main_yn,
                   const float *sub_xn,  float *sub_yn,
                   uint32_t frames) {
  (void)sub_xn; (void)sub_yn;   // sub timbre not processed by user modfx here
  s_chorus.process(main_xn, main_yn, frames);
}

void MODFX_PARAM(uint8_t index, int32_t value) {
  const float valf = q31_to_f32(value);
  switch (index) {
    case k_user_modfx_param_time:                    // mode, 3 zones
      s_chorus.setMode((valf < 0.333f) ? 0 : (valf < 0.667f ? 1 : 2));
      break;
    case k_user_modfx_param_depth:                   // wet level
      s_chorus.setMix(valf);
      break;
    default:
      break;
  }
}
