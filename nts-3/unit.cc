/*
 *  File: unit.cc
 *
 *  BBD Chorus - NTS-3 kaoss pad kit generic effect unit interface.
 *  Thin wrapper around the shared DSP core in ../bbd_chorus_dsp.h.
 */

#include "unit_genericfx.h"
#include "utils/int_math.h"   // clipminmaxi32()
#include "bbd_chorus_dsp.h"

namespace {

enum {
  kParamMode = 0U,
  kParamAmount,
  kParamRate,
  kParamDepth,
  kParamDelay,
  kParamPreLpf,
  kParamPostLpf,
  kParamNoise,
  kNumParams
};

bbd::Chorus s_chorus;
int32_t     s_cached[UNIT_GENERICFX_MAX_PARAM_COUNT];

// Value units match the descriptors in header.c.
void applyParam(uint8_t id, int32_t value) {
  switch (id) {
    case kParamMode:    s_chorus.setMode((uint8_t)value); break;
    case kParamAmount:  s_chorus.setMix(value / 1000.f); break;          // 0.0 .. 100.0 %
    case kParamRate:    s_chorus.setRateScale(value / 100.f); break;     // 25 .. 400 %
    case kParamDepth:   s_chorus.setDepthScale(value / 100.f); break;    // 0 .. 200 %
    case kParamDelay:   s_chorus.setCenterMs(value / 10.f); break;       // 1.0 .. 10.0 ms
    case kParamPreLpf:  s_chorus.setPreCutoff(value * 100.f); break;     // 1.0 .. 16.0 kHz
    case kParamPostLpf: s_chorus.setPostCutoff(value * 100.f); break;    // 1.0 .. 16.0 kHz
    case kParamNoise:   s_chorus.setNoise(value / 100.f); break;         // 0 .. 100 %
    default: break;
  }
}

} // namespace

// ---- Callbacks exposed to runtime ----------------------------------------------

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
  if (!desc)
    return k_unit_err_undef;
  if (desc->target != unit_header.common.target)
    return k_unit_err_target;
  if (!UNIT_API_IS_COMPAT(desc->api))
    return k_unit_err_api_version;
  if (desc->samplerate != (uint32_t)bbd::kFs)
    return k_unit_err_samplerate;
  if (desc->input_channels != 2 || desc->output_channels != 2)
    return k_unit_err_geometry;

  // Delay line lives in SDRAM, as on the minilogue xd build.
  if (!desc->hooks.sdram_alloc)
    return k_unit_err_memory;
  float *buf = (float *)desc->hooks.sdram_alloc(bbd::kBufLen * sizeof(float));
  if (!buf)
    return k_unit_err_memory;

  s_chorus.init(buf);   // clears the buffer

  for (int id = 0; id < UNIT_GENERICFX_MAX_PARAM_COUNT; ++id) {
    s_cached[id] = unit_header.common.params[id].init;
    applyParam(id, s_cached[id]);
  }

  return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() { s_chorus.reset(); }

__unit_callback void unit_resume() {}

__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
  s_chorus.process(in, out, frames);
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
  if (id >= kNumParams)
    return;
  value = clipminmaxi32(unit_header.common.params[id].min, value,
                        unit_header.common.params[id].max);
  s_cached[id] = value;
  applyParam(id, value);
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
  return (id < UNIT_GENERICFX_MAX_PARAM_COUNT) ? s_cached[id] : 0;
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
  static const char *const kModeNames[bbd::kNumModes] = {"I", "II", "I+II"};
  if (id == kParamMode && value >= 0 && value < bbd::kNumModes)
    return kModeNames[value];
  return nullptr;
}

__unit_callback void unit_touch_event(uint8_t id, uint8_t phase, uint32_t x, uint32_t y) {
  // Pad X/Y are already mapped to parameters by the default mappings.
  (void)id; (void)phase; (void)x; (void)y;
}

__unit_callback void unit_set_tempo(uint32_t tempo) { (void)tempo; }

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) { (void)counter; }
