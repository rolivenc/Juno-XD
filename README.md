# BBD Chorus — Juno-Style Chorus for Korg minilogue xd

Emulating the warm character of the classic Roland Juno chorus, BBD Chorus combines subtle saturation, noise, and stereo modulation to recreate the imperfect, organic feel of vintage bucket-brigade chorus circuits.

Inspired by the Juno-60 and Juno-106, it brings that familiar wide, lush sound to the Korg minilogue xd, with an adjustable chorus amount for greater control over the effect.

## Controls

**Time — Chorus Mode**

Select between three chorus modes inspired by the original Juno chorus:

- **Chorus I** — A slow, soft, warm stereo swirl that adds subtle width and depth to pads, leads, and sustained sounds.
- **Chorus II** — A faster, deeper, and more energetic modulation that creates a more intense, shimmering effect.
- **Chorus I + II** — Combines both modes for a thicker, more pronounced modulation with a denser, more complex swirling character.

**Depth — Chorus Amount**

Controls the amount of chorus applied to the signal, from subtle movement to a lush, heavily modulated sound. Unlike the original Juno chorus, BBD Chorus lets you dial in the effect to suit your patch.

## Building

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

## Loading

Load the `.mnlgxdunit` file onto your minilogue xd using `logue-cli` or Korg Sound Librarian.

```bash
logue-cli load -u bbd_chorus.mnlgxdunit -s 0
```

## Technical Notes

- The delay buffer is allocated in SDRAM using `__sdram`, since the modfx SRAM region is limited to 6 KB.
- The effect is designed around the minilogue xd's Mod FX interface, which exposes the Time and Depth parameters.
- The chorus characteristics can be adjusted in `bbd_chorus.cpp`, including modulation rates, delay times, filter cutoff frequencies, and noise level.
