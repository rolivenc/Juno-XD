# BBD Chorus — Juno-Style Chorus for Korg minilogue xd and NTS-3 kaoss pad

Emulating the warm character of the classic Roland Juno chorus, BBD Chorus combines subtle saturation, noise, and stereo modulation to recreate the imperfect, organic feel of vintage bucket-brigade chorus circuits.

Inspired by the Juno-60 and Juno-106, it brings that familiar wide, lush sound to the Korg minilogue xd and NTS-3 kaoss pad kit, with an adjustable chorus amount for greater control over the effect.

## Controls

### minilogue xd

**Time — Chorus Mode**

Select between three chorus modes inspired by the original Juno chorus:

- **Chorus I** — A slow, soft, warm stereo swirl that adds subtle width and depth to pads, leads, and sustained sounds.
- **Chorus II** — A faster, deeper, and more energetic modulation that creates a more intense, shimmering effect.
- **Chorus I + II** — Combines both modes for a thicker, more pronounced modulation with a denser, more complex swirling character.

**Depth — Chorus Amount**

Controls the amount of chorus applied to the signal, from subtle movement to a lush, heavily modulated sound. Unlike the original Juno chorus, BBD Chorus lets you dial in the effect to suit your patch.

### NTS-3 kaoss pad kit

The NTS-3 build exposes the same two controls as unit parameters, mapped to the touch pad by default (the assignments can be changed on the device):

- **Pad X — MODE**: three zones, left to right: **I**, **II**, **I+II**.
- **Pad Y — AMOUNT**: chorus amount, 0–100 % (defaults to 100 %).

It also exposes the chorus voicing as extra parameters. These are not assigned to a control by default; assign them on the device to tweak them. The defaults match the minilogue xd version exactly.

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| MOD RATE  | 25–400 % | 100 % | LFO rate, relative to the selected mode (I ≈ 0.51 Hz, II ≈ 0.83 Hz, I+II ≈ 9.75 Hz) |
| MOD DEPTH | 0–200 % | 100 % | Delay sweep depth, relative to the selected mode (I/II ±2.1 ms, I+II ±0.3 ms) |
| DELAY     | 1.0–10.0 ms | 3.5 ms | Centre delay time |
| PRE LPF   | 1.0–16.0 kHz | 9.0 kHz | Anti-alias low-pass before the delay line |
| POST LPF  | 1.0–16.0 kHz | 7.5 kHz | 4th-order reconstruction low-pass after the delay line |
| NOISE     | 0–100 % | 25 % | BBD hiss level |

## Repository Layout

- `bbd_chorus_dsp.h` — platform-independent chorus DSP, shared by both builds.
- `bbd_chorus.cpp`, `Makefile`, `project.mk`, `manifest.json`, `ld/`, `tpl/` — minilogue xd (modfx) build.
- `nts-3/` — NTS-3 kaoss pad kit (genericfx) build: `Makefile`, `config.mk`, `header.c`, `unit.cc`.

## Building (minilogue xd)

Requires the [Korg logue SDK](https://github.com/korginc/logue-sdk) and the ARM embedded GCC toolchain.

```bash
git clone https://github.com/korginc/logue-sdk.git
cd logue-sdk
git submodule update --init

cp -r /path/to/bbd-chorus platform/minilogue-xd/
cd platform/minilogue-xd/bbd-chorus

make GCC_BIN_PATH=$(dirname $(which arm-none-eabi-gcc))
make install GCC_BIN_PATH=$(dirname $(which arm-none-eabi-gcc))
```

Alternatively, use the SDK's Docker build flow.

The output is `bbd_chorus.mnlgxdunit`.

## Building (NTS-3 kaoss pad kit)

Requires the logue SDK v2 and gcc-arm-none-eabi 10.3-2021.10 (the toolchain the SDK specifies for the NTS-3).

Copy or clone this repository into the SDK's NTS-3 platform directory, then build from the `nts-3/` subdirectory:

```bash
cp -r /path/to/bbd-chorus logue-sdk/platform/nts-3_kaoss/
cd logue-sdk/platform/nts-3_kaoss/bbd-chorus/nts-3

make GCC_BIN_PATH=$(dirname $(which arm-none-eabi-gcc))
make install GCC_BIN_PATH=$(dirname $(which arm-none-eabi-gcc))
```

To build in place without copying, point `PLATFORMDIR` at the SDK instead:

```bash
make install PLATFORMDIR=/path/to/logue-sdk/platform/nts-3_kaoss GCC_BIN_PATH=$(dirname $(which arm-none-eabi-gcc))
```

The output is `nts-3/bbd_chorus.nts3unit`.

## Building on Windows (MSYS2)

Install [MSYS2](https://www.msys2.org/), run `pacman -S make zip unzip` in the MSYS2 MSYS shell, and install both toolchains into `logue-sdk/tools/gcc` (`get_gcc_msys2.sh`). The Windows-native compilers don't understand MSYS-style `/d/...` paths inside linker options, so pass the project and SDK locations as `D:/...` paths:

```bash
make install PROJECTDIR=D:/path/to/bbd-chorus PLATFORMDIR=D:/path/to/logue-sdk/platform/minilogue-xd
```

```bash
cd nts-3 && make install PROJECT_ROOT=D:/path/to/bbd-chorus/nts-3/ PLATFORMDIR=D:/path/to/logue-sdk/platform/nts-3_kaoss
```

## Loading

**minilogue xd:** load the `.mnlgxdunit` file onto your minilogue xd using `logue-cli` or Korg Sound Librarian.

```bash
logue-cli load -u bbd_chorus.mnlgxdunit -s 0
```

**NTS-3 kaoss pad kit:** load the `.nts3unit` file with KORG Kontrol Editor or the newer `loguecli`, as described in the logue SDK NTS-3 README.

## Technical Notes

- On the minilogue xd the delay buffer is allocated in SDRAM using `__sdram`, since the modfx SRAM region is limited to 6 KB. On the NTS-3 it is allocated through the runtime's `sdram_alloc` hook.
- The effect is designed around the minilogue xd's Mod FX interface, which exposes the Time and Depth parameters.
- Switching modes glides the modulation depth to the new mode's value instead of jumping, avoiding a pitch jump between II and I+II. Build with `UDEFS = -DBBD_MODE_GLIDE=0` (in `project.mk` or `nts-3/config.mk`), or change the default in `bbd_chorus_dsp.h`, to switch instantly as in the original version.
- The chorus characteristics can be adjusted in `bbd_chorus_dsp.h`, including modulation rates, delay times, filter cutoff frequencies, and noise level.
