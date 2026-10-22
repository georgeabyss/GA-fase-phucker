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

Build step 2 of 5: every parameter exists (271, listed in `tests/reference/parameters.txt`); the
plug-in still passes audio through the shared GA output stage (Dry/Wet, Bass Mono, Phase Protect,
oversampling, bypass, safety mute), and the window has the header, the preset browser and the bottom
strip. The DSP and the bands and sections follow.

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

Screenshots of the real interface: `cmake -B build-tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DFPH_SCREENSHOTS=ON`,
`cmake --build build-tools --target FasePhuckerScreenshots`, then `build-tools/FasePhuckerScreenshots <folder>`.

Parameter IDs never change once they exist: `tools/check_parameters.sh` compares every parameter with
`tests/reference/parameters.txt` and runs the parameter tests (names, channel links, sessions).
