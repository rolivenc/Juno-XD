/*
 *  File: header.c
 *
 *  BBD Chorus - NTS-3 kaoss pad kit generic effect unit header
 *
 *  Pad X  = chorus mode, in three zones: I | II | I+II
 *  Pad Y  = chorus amount (wet level)
 *  The remaining parameters tune the chorus and are unassigned by default;
 *  assignments can be changed by the user on the device.
 *  Defaults match the minilogue xd build (see bbd_chorus_dsp.h).
 */

#include "unit_genericfx.h"

const __unit_header genericfx_unit_header_t unit_header = {
  .common = {
    .header_size = sizeof(genericfx_unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_genericfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0,      // See https://github.com/korginc/logue-sdk/blob/master/developer_ids.md
    .unit_id = 0x0U,
    .version = 0x00010001U,   // 1.0.1, matches manifest.json
    .name = "Juno XD",
    .num_params = 8,

    .params = {
      // Format: min, max, center, default, type, frac. bits, frac. mode, <reserved>, name
      {0, 2, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"MODE"}},
      {0, 1000, 0, 1000, k_unit_param_type_percent, 1, 1, 0, {"AMOUNT"}},
      // LFO rate, % of the current mode's rate
      {25, 400, 100, 100, k_unit_param_type_percent, 0, 0, 0, {"MOD RATE"}},
      // Modulation depth, % of the current mode's depth
      {0, 200, 100, 100, k_unit_param_type_percent, 0, 0, 0, {"MOD DEPTH"}},
      // Centre delay time, 1.0 .. 10.0 ms
      {10, 100, 35, 35, k_unit_param_type_msec, 1, 1, 0, {"DELAY"}},
      // Pre-BBD anti-alias low-pass, 1.0 .. 16.0 kHz
      {10, 160, 90, 90, k_unit_param_type_khertz, 1, 1, 0, {"PRE LPF"}},
      // Post-BBD reconstruction low-pass, 1.0 .. 16.0 kHz
      {10, 160, 75, 75, k_unit_param_type_khertz, 1, 1, 0, {"POST LPF"}},
      // BBD noise level
      {0, 100, 25, 25, k_unit_param_type_percent, 0, 0, 0, {"NOISE"}}},
  },
  .default_mappings = {
    // Format: assign, curve, curve polarity, min, max, default value
    {k_genericfx_param_assign_x, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, 2, 0},
    {k_genericfx_param_assign_y, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, 1000, 1000},
    {k_genericfx_param_assign_none, k_genericfx_curve_exp, k_genericfx_curve_unipolar, 25, 400, 100},
    {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, 200, 100},
    {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 10, 100, 35},
    {k_genericfx_param_assign_none, k_genericfx_curve_exp, k_genericfx_curve_unipolar, 10, 160, 90},
    {k_genericfx_param_assign_none, k_genericfx_curve_exp, k_genericfx_curve_unipolar, 10, 160, 75},
    {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, 100, 25}
  }
};
