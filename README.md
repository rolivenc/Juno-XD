# BBD Chorus - Juno-style chorus for the Korg minilogue xd (modfx)

Verified to compile with the logue SDK (arm-none-eabi-gcc 13.2).

## Controls (the xd modfx slot only exposes Time and Depth)
- Time  : mode select in three zones -> I | II | I+II
- Depth : wet level (dry signal always passes, like the Juno)

## Build
    git clone https://github.com/korginc/logue-sdk.git
    cd logue-sdk && git submodule update --init
    cp -r bbd-chorus logue-sdk/platform/minilogue-xd/
    cd logue-sdk/platform/minilogue-xd/bbd-chorus
    # with a local toolchain (brew install --cask gcc-arm-embedded, or apt gcc-arm-none-eabi):
    make GCC_BIN_PATH=$(dirname $(which arm-none-eabi-gcc))
    make install GCC_BIN_PATH=$(dirname $(which arm-none-eabi-gcc))
or use the SDK's Docker flow (docker/run_cmd.sh build minilogue-xd/bbd-chorus).

Output: bbd_chorus.mnlgxdunit

## Load
logue-cli load -u bbd_chorus.mnlgxdunit -s 0   (or Korg Sound Librarian)

## Notes
- The delay line is in SDRAM (`__sdram`); the modfx SRAM region is only 6K.
- Tune the constants at the top of bbd_chorus.cpp (rates, delay times,
  filter corners, noise) against a real Juno or reference plugin.
