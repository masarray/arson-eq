# PRD — ArSonEQ Professional Parametric EQ

Version: 0.1 | Updated: 2026-10-29 | Status: implementation baseline
Owner: masarray/arson-eq | Source of truth for parallel threads

## 1. Product outcome

Build a polished, low-CPU parametric EQ for musicians, audio engineers, mastering, live sound, and integration into ArSonMix. Fast workflows, audible transparency, host automation, deterministic DSP, compatibility, and small interface overhead matter more than feature-count marketing. Reference products establish user expectations; the product architecture and visual identity remain original.

## 2. User journeys and success criteria

1. Add a band with a double-click; drag frequency/gain; mouse wheel changes Q/slope; switch shape and stereo placement; bypass one band instantly without clicks.
2. Use 2–8 typical bands at a live buffer size of 32/64/128 samples without stutter.
3. Tackle harshness with per-band dynamic processing (later milestone), safely audible in real time.
4. Observe a smooth pre/post analyzer without UI rendering affecting the audio callback.
5. Save/reopen DAW sessions and presets with identical sound and values; automate parameters without surprises.
6. Embed the DSP core in another product without depending on JUCE, rendering, or host code.

## 3. Feature requirements

| ID | Requirement | Priority | Gate |
|---|---|---|---|
| EQ-01 | Up to 24 individually enabled bands, persistent stable slots | P0 | Unit and host tests |
| EQ-02 | Bell, low/high shelf, low/high cut, notch; Q, gain, frequency | P0 | Response comparison and finite output |
| EQ-03 | 12/24/36/48 dB/oct Butterworth cuts (existing MaSonKuPik values) | P0 | Measured frequency response |
| EQ-04 | Stereo, L, R, Mid, Side processing per band; mono downmix | P0 | Matrix + M/S null tests |
| EQ-05 | Zero-latency minimum-phase mode; no unnecessary lookahead | P0 | Reported latency 0, bypass/null |
| EQ-06 | Native VST3 x64, host automation, state restore, bypass | P0 | REAPER + VST3 validator |
| EQ-07 | Interactive log-frequency graph, compact inspector, resize, sensible contrast | P0 | Visual QA at 100–200% DPI |
| EQ-08 | No allocations/locks/logging/IO in processBlock | P0 | RT audit + profiling |
| EQ-09 | Smooth changes incl. cross-type/slope without crackle | P0 release gate | Automation stress + listening |
| EQ-10 | Pre/post FFT, freeze, adjustable tilt/range, spectrum grab | P1 | Performance/accuracy profiling |
| EQ-11 | Dynamic EQ per band, compression/expansion, manual/auto threshold | P1 | Envelope + gain reduction tests |
| EQ-12 | External sidechain with proper bus negotiation/fallback | P1 | Host interoperability |
| EQ-13 | A/B, undo/redo, band solo, gain scale, keyboard and numeric input | P1 | Interaction acceptance |
| EQ-14 | Natural Phase-like low-phase-distortion mode (original method) | P2 | Phase/impulse/latency measurements |
| EQ-15 | Linear Phase with quality/latency choices | P2 | FIR, delay reporting, pre-ringing analysis |
| EQ-16 | 6–96 dB/oct cut slopes, spectral collision/EQ match, surround | P2 | Separate validated engines |
| EQ-17 | CLAP/AU, multi-instance awareness | P3 | Dedicated platform compatibility |

P0 means foundation and v1 release blockers as applicable; work may be staged under M0/M1. A table entry alone is not an assertion that the feature has shipped.

## 4. UX and visual language

Premium but restrained: graphite/near-black canvas, subtle calibrated frequency/dB grid, fine EQ curve, tiny unobtrusive frequency nodes, legible typography, soft highlight only on selected band, no giant cards, shadows, or gratuitous hover animations. No perpetual animation when stopped or hidden. No embedded browser runtime in audio processing. UI remains functional with FFT disabled.

Primary graph takes most of the window; bottom compact toolbar contains phase mode, spectrum modes, output, bypass and A/B; a single unobtrusive context inspector appears only for the selected band. Double-click add; right-click context; drag adjusts; Shift offers fine changes; scroll adjusts Q; Ctrl/Cmd+Z undo; numeric input for precision. Frequency axis 20 Hz–20 kHz, capped safely to Nyquist. Make at least 100%, 125%, 150%, 200% scale pass.

## 5. Compatibility and state

- Windows 10/11 x64 VST3 first, sample rates 44.1/48/88.2/96/192 kHz, blocks 16–2048 and variable lengths; mono and stereo.
- Main stereo bus: mono/stereo input matching output; sidechain later and optional.
- Serialized schema version, stable non-recycled host parameter IDs, deterministic saved presets.
- Disabled band slots keep IDs; host automation still addresses the same slot after deletions.
- No network/cloud telemetry, installer or registration required for local use.

## 6. Quantifiable performance targets (not yet measured)

On reference desktop Windows x64 48 kHz 64-sample buffer and native release build:
- Zero-latency mode: reported latency 0 samples; no heap allocation in processBlock.
- Typical 8 bell bands stereo: target <0.5% of one modern performance core at 48 kHz (to be benchmarked, not promised).
- 24 bands: target <2.0% of one modern performance core under the same reference conditions; report exact CPU and machine.
- Editor closed: no analyzer/visual work; editor open: FFT reading capped ~30 Hz and no impact on callback deadlines.
- No NaN/Inf, hangs, clicks on automation, silence self-noise, unexpected gain in flat mode.
- Startup without unnecessary heavy initialization or network operations.

## 7. Non-goals

Do not copy FabFilter branding, logos, code, proprietary filter algorithms, exact visual assets, or UI textures. Do not transplant the entire MaSonKuPik mastering chain or introduce compressor/limiter to this EQ. Do not present unimplemented phase modes or advanced dynamics as working controls. Do not require a development/"Lab" mode for users.

## 8. Release acceptance

VST3 validator, host projects, automation/snapshot recall, phase/frequency/impulse tests, soak stress, CPU/memory benchmark and at least two real audio listening scenarios must pass before declaring production. Release artifacts must be traceable to a pinned commit and have an actual reproducible build.
