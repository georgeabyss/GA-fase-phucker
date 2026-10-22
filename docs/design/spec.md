# Fase Phucker – Feature & Design Spec

Agreed 21–22 Oct 2026. Companion files in this folder (`docs/design/`):

- `mockup.png` – the agreed main window at 1260 × 726, which is 133 % since 22 Oct (see Window and layout), rendered at 2× (2520 × 1452).
- `mockup.html` – the same mockup, interactive; open it in any browser (no internet needed). Click band cards, drag the Stages number, the split values and the grip/donuts, change dropdowns. Exact sizes, colours and spacing can be read from its CSS and markup.
- `signal-flow.png` – the signal flow diagram.

The framework refactor is specified separately in `docs/refactor-plan.md`.

## Overview

Fase Phucker is a free multiband all-pass disperser by GA: up to 1000 all-pass stages per band, with distortion, feedback and an envelope follower for weird, transient-smearing sound design. It's inspired by a thousand Ableton EQ3's phuckery

- **Built in JUCE**, as the second plugin in the GA family after Hyper! Hyper! Dimension, on the shared `ga-framework`.
- **Same look and feel:** colours, knobs, LEDs, dropdowns, screws, the gothic GA logo and the whole bottom str∏ip are taken over unchanged; only the window shape is new.
- **Name:** Fase Phucker, subtitle ALL-PASS DISPERSER.

## Signal flow and architecture

The input splits into 1, 2 or 3 bands; each band runs its own full chain on two channels (L/R or M/S), and the bands sum back into the shared output stage. See `signal-flow.png`.

- **Band count:** Single, 2 Bands or 3 Bands (LOW | HIGH, or LOW | MID | HIGH); Init is Single. Default splits: 800 Hz for 2 bands; 180 Hz and 2.4 kHz for 3 bands. Each mode keeps its own splits.
- **Crossovers:** set by dragging the split value between two band cards; double-click resets. Minimum spacing between splits is 1.5×.
- **Every band has all settings:** Disperse, Env Follower, Distortion, Feedback, plus on/off and solo.
- **Stereo per band:** a dropdown on each band card picks L / R or M / S. This decides what the two channels are.
- **Channel link per section:** each section header has a LINK / M / S (or LINK / L / R) dropdown. LINK = both channels share that section's values; M or S = edit that channel alone. Switching back to LINK copies the channel being viewed onto the other.
- **Distortion position:** Pre or Post the all-pass chain, per band.
- **Feedback:** loops only around the all-pass chain, never around Distortion (Pre or Post), with its own delay time, tone and polarity.
- **Output stage (shared framework):** global Dry/Wet, Bass Mono, loudness compensation, equal-loudness bypass, Phase Protect, safety mute, oversampling.

## Features by section

Four per-band sections plus the band strip on top and the unchanged GA bottom strip below. Every control in a section follows that section's channel link.

### Bands

| Control | What it does |
| --- | --- |
| Bands (header dropdown) | Single, 2 Bands, 3 Bands |
| Band card | Click to show that band's settings in all sections below |
| Round LED (left) | Band on/off; an off band dims and its trace turns grey |
| S button (right) | Solo; lights orange when on |
| Stage count | Shows the band's stages; "256 / 32 stages" when Mid and Side differ |
| Range + time span | e.g. "180 Hz – 2.4 kHz · 788 ms"; the span is the length of the trace |
| Impulse trace | The band's real impulse response (see Implementation notes); a second, dimmer trace when Disperse is unlinked |
| Stereo dropdown | L / R or M / S |
| Link state | LINKED, or M ≠ S / L ≠ R when any section is unlinked |
| Split value | Between cards; drag left/right to move, double-click to reset; label ◂ SPLIT ▸ |

### Disperse

| Control | Range / options | Notes |
| --- | --- | --- |
| Stages | 1–1000 | Arrows double/halve; drag the number up/down for single stages |
| Mode | Behaved, Unhinged | Behaved = smoothed, stable (TPT/SVF-style); Unhinged = direct-form, unsmoothed, glitchy when modulated |
| Jitter | On/off (square lamp) | Per-stage frequency jitter; separate from Mode |
| Freq | 20 Hz–20 kHz | Centre of the dispersion |
| Pinch | 0–100 % | Q of the all-passes: more = tighter, more tonal |
| Spread | 0–100 % | Spreads stage frequencies |
| Mix | 0–100 % | Dry/wet of this section |

### Env Follower

| Control | Range | Notes |
| --- | --- | --- |
| Gain | −12 to +24 dB | Input sensitivity |
| Amount | −100 to +100 % | Overall depth |
| Attack | 0.1–100 ms | |
| Hold | 0–500 ms | Keeps the envelope up before release |
| Release | 5–2000 ms | |

Modulation is drag-to-assign: drag the grip button in the header onto any knob in Disperse, Distortion or Feedback (+50 % by default). Each destination gets a donut next to the grip: drag up/down for depth (−100 to +100 %), double-click to remove, hover to highlight its knob. Assigned knobs show an arc from their value to value + depth; there is no handle on the knob itself. The donuts are in fixed knob order, Disperse → Distortion → Feedback, left to right, not in the order they were assigned. Five donuts fit; with six or more the header shows four plus a +n button that pages through the rest. The follower has no dice and no on/off LED: it is active whenever it has destinations. Assignments are per band.

### Distortion

| Control | Range / options | Notes |
| --- | --- | --- |
| Type | Noise, Saturate, Clip, Fold, Rectify, Crush | Noise = noise injection: noise ring-modulated by the signal |
| Order | Pre, Post | Before or after the all-pass chain |
| Drive | 0–100 % | Labelled NOISE when Type = Noise |
| Bias | −100 to +100 % | Asymmetry |
| Phat | 0–100 % | Saturation of the low mids and mids only |
| Tone | 0–100 % | Post filter |
| Mix | 0–100 % | Dry/wet of this section |

### Feedback

| Control | Range | Notes |
| --- | --- | --- |
| Invert | On/off (square lamp) | Polarity of the feedback path |
| Amount | 0–95 % | Self-oscillates from about 80 % |
| Time | 0.1–50 ms | Extra delay in the loop |
| Tone | 0–100 % | Filter in the loop |
| Mix | 0–100 % | Dry/wet of this section |

### Shared GA framework (unchanged from Hyper! Hyper! Dimension)

- Bypass (click-free, equal-loudness), Dry/Wet and Bass Mono with locks, oversampling 1–8×, Linear Phase / Low Latency anti-alias filter, window size 75–200 % plus corner drag.
- Correlation meter, Phase Protect (pushes mid into the sides on negative correlation), goniometer.
- Loudness compensation based on perceived loudness, not peaks; safety mute on illegal values or above +24 dB.
- Preset browser with ◂ ▸ and the LED-matrix display, plus a global dice that randomizes all bands. Each of Disperse, Distortion and Feedback has its own dice and on/off LED.

## Design spec

The design reuses Hyper! Hyper! Dimension's look 1:1 and adds one rule set on top: one column grid, one dropdown, one knob, one spacing scale.

### Window and layout

- **Size at 100 %:** 945 × 545 px on screen, the mockup's 75 %. Laid out in Hyper! Hyper! Dimension's layout units (the mockup's pixels × 0.64): 806 × 465, side margins 28, so the bottom strip is exactly its native 750. Both plugins show one layout pixel as 1.172 screen pixels at 100 %, so their controls are the same size. Rack screws in all four corners; purple corner resizer.
- **Mockup pixels:** the sizes in this spec and in `mockup.html` are the mockup's (1260 × 726); multiply by 0.64 for layout pixels.
- **Rows, top to bottom:** header · Bands · Disperse | Env Follower · Distortion | Feedback · bottom strip.
- **12-column grid** for the two section rows: Disperse and Distortion span 7 columns, Env Follower and Feedback span 5. Both rows split at the same column, so section edges and controls line up vertically.
- **One control per column,** centred. No control spans two columns.
- **Same outer edges** for header, Bands, sections and bottom strip.
- **Vertical rhythm:** about 28 px between blocks; section title line, then 18 px, then the controls.

### Header

- Title FASE PHUCKER in the purple gradient with the static "unison echo" smear, subtitle ALL-PASS DISPERSER in spaced grey caps.
- Preset browser between title and logo: ◂, LED-matrix display, ▸, global dice. Title, preset group and logo have fixed widths so they never overlap.
- Gothic GA logo, silver gradient, drawn with the framework's logo path code.

### Section anatomy

- Header line: title on the left in 14 px bold spaced caps; controls right-aligned in this order: channel dropdown, dice, on/off LED. Env Follower: grip button, donuts, channel dropdown.
- Titles never wrap; long names are shortened (ENV FOLLOWER).
- No boxes or inset panels around sections; the dark inset panel is reserved for the bottom strip.
- Under each control: value line (mono, bold), then name (spaced caps). Dropdowns and lamps leave the value line empty instead of showing On/Off.

### Controls

| Control | Spec |
| --- | --- |
| Knob | Hyper! Hyper! Dimension knob (radius 14 at 802 px, scaled), purple pointer, three position dots; all knobs in the sections are the same size |
| Dropdown | One style everywhere in the sections and band cards: 84 × 24 px recess, centred mono text 10.5 px bold, up/down arrows on the right |
| Stages box | Same size and arrows as a dropdown |
| Toggle | Square lamp from Retrig (Jitter, Invert); no On/Off text |
| Section / band LED | Round purple LED |
| Solo | Square "S" button, lights orange |
| Mod source | Square recess button with a 2 × 3 grip icon, no text |
| Mod donut | 22 px ring, purple arc from 12 o'clock = depth (clockwise +, anticlockwise −), zero tick |
| Mod arc on knob | Light lavender arc just outside the knob, from value to value + depth |
| Band card | Dark card; selected card has a purple border and glow |

### Colour and type

| Token | Value | Use |
| --- | --- | --- |
| purple | #b18cfe | Pointers, LEDs, active states |
| purpleHi | #d9c7ff | Hover, highlights |
| text | #f0f1f3 | Values |
| labelGrey | #8f9299 / #a3a6ad | Names, section titles |
| warn / solo | #ff9a3c | Solo, Phase Protect alarm |
| faceplate | #1a1b1e → #0d0e10 | Background gradient |

Values use a monospaced font (SF Mono / Menlo / Consolas); labels use the plugin sans in spaced caps. Names are written in full where they fit (e.g. LINEAR PHASE is never shortened).

### Interaction rules

- Drag knobs up/down; same for Stages (number), donuts and splits (left/right).
- Double-click resets (knobs, splits) or removes (donuts).
- Hover shows tooltips; hovering a donut highlights its knob.
- While dragging the mod source, valid targets get a dashed outline and the target under the cursor highlights.

## Decisions log

Every call made on 21–22 Oct 2026; "Rejected" lists what was tried and dropped.

| Area | Decided | Rejected or replaced | Why |
| --- | --- | --- | --- |
| Name | Fase Phucker | — | Family-friendly(-ish) spoonerism |
| Base | Reuse Hyper! Hyper! Dimension via a shared framework refactor | Copy-paste | One codebase for the GA family |
| All-pass modes | Behaved / Unhinged | Well behaved / Erratic; Polite / Feral | User's pick |
| Jitter | Separate per-stage toggle | Part of Unhinged | Independent control |
| Noise | A distortion type: noise ring-modulated by the signal | Noise/Ring knobs; noise before/after | Behaves like distortion, follows dynamics |
| Distortion types | Noise, Saturate, Clip, Fold, Rectify, Crush | NOISE INJECT etc. (too long); Inject | Fit the 84 px dropdown without abbreviating |
| Bias | Added to Distortion | — | User request |
| Section mix | Mix knob on Disperse, Distortion, Feedback | One global dry/wet only | Every section needs its own dry/wet |
| Extra knobs | Hold (Env Follower), Tone (Distortion), Time and Invert (Feedback) | Empty columns | Fill the grid with useful controls |
| Bands | Single / 2 / 3, all settings per band | One chain with per-band amount | Full control per band |
| Band display | Band cards with impulse trace, left to right LOW → HIGH | Group-delay curve (read as an EQ); IN → OUT routing diagram | Roar-style, clearly not an EQ |
| Trace | Real impulse response, time span shown | Stylised chirp drawing | Accurate, cheap via FFT |
| Bands area | Full width, plain section | Inset box; left routing column | Box was not cohesive with the rest |
| Stereo | L/R or M/S per band card | "Process: L + R / Mid / Side" | L + R did not make sense |
| Channel editing | LINK / M / S dropdown per section header | Global editing bar; Link LED; Copy button | Lopsided, not in the design language; per section is more flexible |
| Band on/solo | LED + orange S on each band card | Band section with Level/Solo/Mute | Unnecessary section |
| Env follower | Own section, drag-to-assign with donuts | Target dropdown; Follow buttons with depth bar | Multiple targets, direct |
| Mod depth UI | Donuts next to the grip; arc on the knob | Handle/donut on each knob | The arc already shows the amount |
| Env Follower header | Grip icon only, no dice, no LED | DRAG text + icon; dice; LED | Redundant; made room for donuts |
| Dropdowns | One 84 × 24 style everywhere | Button rows; mixed sizes | Matches the original's StepBox |
| Toggles | Square lamp, no On/Off text | Toggle with "On" + light | Not like the original |
| Stages box | Same size as a dropdown | Large 74 × 34 box | Too big |
| Section titles | Never wrap; ENV FOLLOWER | ENVELOPE FOLLOWER on two lines | Inline only |
| Bottom strip | Exactly as Hyper! Hyper! Dimension, scaled as one piece | Full-size rebuild with knobs | It is finished and shared |
| Bass Mono | Visible ring like Dry/Wet | Ring hidden at Off | Looked smaller |
| Preset browser | Between title and logo (original balance) | Centred on the window | Original was more balanced |
| Header | Fixed widths, no overlap | — | Preset display overlapped the title |
| Logo | Real GA path, fixed winding | — | Black spots in the first port |
| Spacing | ~28 px between blocks, 12-column grid | Tight, uneven spacing | Unrelated elements were too close |
| Feedback path | Only around the all-pass chain | Around Distortion when Order = Pre | Decided 22 Oct |
| Section dice + LED | Kept on Disperse, Distortion, Feedback | Removed like on Env Follower | Decided 22 Oct |
| Splits | `split` for 2 bands, `split1`/`split2` for 3 bands | Shared split1/split2 | Switching modes never loses a split. Decided 22 Oct |
| Mod assignment | Hidden `_on` parameter per target; donuts in fixed knob order | Depth 0 = unassigned; assignment order in the state | Assigned at 0 % stays; no extra state. Decided 22 Oct |
| Automation names | Follow band count and stereo mode | Static "Band 2 · Ch 2" names | Readable. Decided 22 Oct |
| Init | Single band | 3 Bands | Simplest start. Decided 22 Oct |
| Section grid | 12 columns of 60 layout pixels, 30 between Disperse and Env Follower (and Distortion and Feedback) | Columns without a gap | The section rules stay apart; the native 64 px drop-downs nearly touch in a 60 px column. Decided 22 Oct |
| Single band card | Named FULL | — | Decided 22 Oct |
| Band off | Passes the band unprocessed | Mutes it | Keep the rest of the spectrum. Decided 22 Oct |
| Tone (Distortion, Feedback) | Tilt around 1 kHz, 50 % = neutral | Low-pass sweep | Decided 22 Oct |
| Feedback loop | Loop gain 1.25 × Amount: self-oscillates from about 80 %; tanh limiter keeps it near the signal's level, a DC blocker keeps it audible; Tone never adds gain (the onset stays at 80 %); Mix only while Feedback is on | Always decaying (gain ≤ 0.95) | Feedback should self-oscillate. Decided 22 Oct |
| Pre / Post switch | Restarts the channel: fade out, empty its sections, fade the input in | A short crossfade | The chain would smear the switch out over its group delay. Decided 22 Oct |
| Dice | Mockup ranges; Feedback at most 70 % | 95 % | Below the self-oscillation onset. Decided 22 Oct |
| Init | Neutral: Disperse, Distortion and Feedback off (the knobs keep their example positions); Distortion fully off passes the input exactly, delayed by the latency | The mockup's example settings | The mockup only showed knob positions. Decided 22 Oct |
| Tail | 3 s reported | 2 s | Up to 2 s of dispersion plus feedback. Decided 22 Oct |
| Control sizes | Hyper! Hyper! Dimension's native sizes everywhere (drop-down 64 × 19, LED 6) | The mockup's drop-down (54 × 15 in layout pixels) and LED (9) | Controls keep their native size; the mockup's were approximations. Decided 22 Oct |
| Window size | 100 % = the mockup's 75 % (945 × 545); laid out in Hyper! Hyper! Dimension's units; Hyper! Hyper! Dimension's 100 % grows to its old 117 % to match | 1260 × 726 at 100 % | Much larger than Hyper! Hyper! Dimension; its old 100 % looked too small. Decided 22 Oct |

## Parameter scheme

Decide before the first parameter is added; IDs can never change afterwards.

| Kind | Pattern | Example |
| --- | --- | --- |
| Per band, per channel | `b<band><ch>_<param>`; `ch` is `m` (Mid, or Left) or `s` (Side, or Right) | `b2s_freq` = band 2, Side, Freq |
| Per band | `b<band>_<param>` | `b2_on`, `b2_solo`, `b2_stereo` |
| Section link | `b<band>_<section>_link`; section `disp`, `env`, `dist`, `fb`; Linked / 1st channel / 2nd channel | `b2_dist_link` |
| Modulation depth | `b<band>_mod_<section>_<param>`; section `d` (Disperse), `x` (Distortion), `f` (Feedback) | `b2_mod_d_freq` |
| Modulation assigned | the depth's ID + `_on` (not automatable) | `b2_mod_d_freq_on` |
| Global | plain | `bands`, `split` (2 bands), `split1`, `split2` (3 bands) + the shared GA IDs |

- b1, b2 and b3 are always the 1st, 2nd and 3rd band of the current mode.
- Per channel (27): `stages`, `mode`, `jitter`, `freq`, `pinch`, `spread`, `dispmix`, `dispon` · `envgain`, `envamt`, `attack`, `hold`, `release` · `type`, `order`, `drive`, `bias`, `phat`, `disttone`, `distmix`, `diston` · `invert`, `fbamt`, `fbtime`, `fbtone`, `fbmix`, `fbon`. Every section, its on/off included, follows its channel link.
- Modulation targets (13 per band): `d_freq`, `d_pinch`, `d_spread`, `d_mix` · `x_drive`, `x_bias`, `x_phat`, `x_tone`, `x_mix` · `f_amt`, `f_time`, `f_tone`, `f_mix`.
- Automation names follow the band count and stereo mode, e.g. "Mid Band · Side · Freq", "Mid Band · Env Mod · Freq"; hosts that cache names keep those from when the plug-in loaded.
- Linking: while a section is linked, only the 1st channel counts. Unlinking copies the 1st channel onto the 2nd once; linking again copies the channel that was shown onto the other. This happens in the section header's dropdown, never when a session or preset loads.

271 parameters in total (265 of Fase Phucker's own, 6 shared); `tests/reference/parameters.txt` lists them all, and `tools/check_parameters.sh` fails if one changes or disappears. Presets store only values that differ from the default ("missing = default").

## Open questions

None open. Not approved for now: a signal-order display in the UI, and defaulting the window to 75 %.

## Project setup

| Item | Value |
| --- | --- |
| Repo | https://github.com/georgeabyss/GA-fase-phucker |
| Local folder | `~/Developer/GA/FasePhucker` |
| Plugin code | `Fph1` |
| Manufacturer code / company | `Gaau` / GA |
| Bundle ID | `com.ga.fasephucker` |
| Preset folder | `~/Music/GA/Fase Phucker/Presets` |
| Framework | `ga-framework` as a git submodule at `framework/`, built with `ga_add_plugin()` |

## Build steps

Same workflow as the refactor: one step per commit, stop after each.

1. **Repo skeleton.** `framework/` as a submodule, built with `ga_add_plugin()`, own plugin code, bundle ID and preset folder, only the shared GA parameters. The window is an empty 806 × 465 panel (945 × 545 on screen at 100 %) with the bottom strip. Windows CI builds it (the Mac plug-ins are built locally, as for Hyper! Hyper! Dimension), and it loads in Logic and Ableton.
2. **Parameter layout.** Every ID from the parameter scheme, created up front. IDs can't change after this.
   - Linked sections: the DSP reads Side from Mid's parameters, so only Mid counts while a section is linked; unlinking copies Mid to Side once.
   - b1/b2/b3 are always the 1st/2nd/3rd band of the current mode.
   - Readable automation names, e.g. "Mid Band · Side · Freq".
3. **Framework controls.** Donut, band card, solo button, Stages box and section header, each in a ga-framework commit with screenshots. Hyper! Hyper! Dimension screenshots must stay pixel-identical.
4. **DSP.** Crossover and bands, then each section, then the mod matrix, each with its own test renders.
   - Oversample only Distortion, not the all-pass chain; the processor shell needs a hook for that.
   - Update coefficients every 16–32 samples.
   - Crossfade on Stages and Mode changes.
   - Soft limiter in the feedback loop.
   - Report a tail of about 2 s or more.
   - A longer loudness window than Hyper! Hyper! Dimension.
   - Keep the dice randomisation within sensible ranges.
5. **UI.** The main panel, built from the mockup.

## Implementation notes

### DSP

- 1000 biquads per sample per channel is feasible with SIMD; process only active stages.
- Behaved = TPT/SVF-style all-pass in double precision with smoothed coefficients. Unhinged = direct-form biquads, unsmoothed coefficient jumps, optionally quantized.
- Group delay at low frequencies gets very long with many stages and cannot be latency-compensated.
- Loudness compensation must use perceived loudness (RMS/LUFS), not peaks: dispersion lowers transient peaks and can raise others. Keep the +24 dB safety on true peak.
- Phase Protect will fight intentional M/S and L/R differences; it is optional and on by default as in Hyper! Hyper! Dimension.

### Band trace

H(ω) = (1 − mix) + mix · A(ω)^N, where A is one biquad all-pass at Freq with Q = 0.5 + Pinch · 4. Window it to the band's range (half-octave slopes), inverse-FFT 8192 points, and draw min/max per column. The time span shown = 1.4 × the peak group delay N · 2Q / (π · Freq), clamped to 4 ms–2 s. Recompute only when Stages, Freq, Pinch, Mix or the band edges change. `mockup.html` contains a working JavaScript version (`ir()`).
