# Airwindows DeRez5 for Schwung / Ableton Move

A Schwung `audio_fx` module port of Airwindows DeRez5, following the module layout/API style of the supplied Echojam project.

## Controls
- **Derez** — frequency/sample movement reduction; lower values are more aggressive. The Schwung port starts at 0.5 so the effect is audible immediately (the original Airwindows default of 1.0 is effectively clean/pass-through).
- **Trim** — adds variation to the derez rate.
- **Square** — moves from point-to-point interpolation toward zero-order hold.
- **Bright** — Bezier smoothing of corners.
- **Bassis** — low-frequency shaping/subtraction.
- **Output** — output level.

The DSP is derived from Airwindows DeRez5, by Chris Johnson, under the MIT license. Original source: https://github.com/airwindows/airwindows and https://www.airwindows.com/derez5/.

## Build for Move (ARM64)
Requires an AArch64 Linux cross compiler (`aarch64-linux-gnu-gcc`):

```sh
./scripts/build.sh
```

The package is written to `dist/derez5-module.tar.gz`. To install over SSH after building:

```sh
./scripts/install.sh
```

The installer expects `ableton@move.local` SSH access to be configured. This is a community Schwung module, not an Ableton product.
