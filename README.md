# Fase Phucker

**ALL-PASS DISPERSER** – a free multiband all-pass disperser by GA: up to 1000 all-pass stages per
band, with distortion, feedback and an envelope follower for weird, transient-smearing sound design.
It's inspired by a thousand Ableton EQ3's phuckery

VST3 and Audio Unit, for macOS (Apple Silicon and Intel, macOS 11 or newer) and Windows (VST3).
The second plug-in in the GA family after
[Hyper! Hyper! Dimension](https://github.com/georgeabyss/GA-hyper-hyper-dimension), on the shared
[GA framework](https://github.com/georgeabyss/ga-framework).

The design, features and build steps are in [docs/design/spec.md](docs/design/spec.md).

## Status

Build step 5 of 5: the interface. The band strip (cards with their impulse response, split handles),
Disperse | Env Follower and Distortion | Feedback with their channel links, dice and modulation
(drag the grip onto a knob), on the GA framework. `tools/make_screenshots.sh` checks the interface
against `tests/reference/screenshots`.

## Build

Built with [JUCE](https://juce.com) and CMake on the GA framework, a git submodule in `framework/`.
JUCE is pinned (the version in `framework/cmake/juce-version.txt`) and found next to this repo:

```sh
git clone --recurse-submodules https://github.com/georgeabyss/GA-fase-phucker.git FasePhucker
git clone --depth 1 --branch "$(cat FasePhucker/framework/cmake/juce-version.txt)" https://github.com/juce-framework/JUCE.git JUCE
cd FasePhucker
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The plug-ins end up in `build/FasePhucker_artefacts/Release/` (`VST3/Fase Phucker.vst3`,
`AU/Fase Phucker.component`). There is no copy after the build; to install for your user only:

```sh
cp -R "build/FasePhucker_artefacts/Release/VST3/Fase Phucker.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "build/FasePhucker_artefacts/Release/AU/Fase Phucker.component" ~/Library/Audio/Plug-Ins/Components/
```

| | |
| --- | --- |
| Plug-in code / manufacturer | `Fph1` / `Gaau` (GA) |
| Bundle ID | `com.ga.fasephucker` |
| Presets | `~/Music/GA/Fase Phucker/Presets`, `.fphpreset` |

The GA control gallery (every framework control, `framework/docs/controls`): `-DGA_GALLERY=ON`, target `FasePhuckerGallery`.

Screenshots of the real interface: `cmake -B build-tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DFPH_SCREENSHOTS=ON`,
`cmake --build build-tools --target FasePhuckerScreenshots`, then `build-tools/FasePhuckerScreenshots <folder>`.

Parameter IDs never change once they exist: `tools/check_parameters.sh` compares every parameter with
`tests/reference/parameters.txt` and runs the parameter tests (names, channel links, sessions).

The DSP: `tools/check_dsp.sh` measures the engine (flatness, group delay, latency, aliasing, the
feedback loop, the envelope, clicks on every switch, CPU) and compares renders through the plug-in
with `tests/reference/renders`; `tools/check_dsp.sh listen <folder>` writes longer renders to listen to.
