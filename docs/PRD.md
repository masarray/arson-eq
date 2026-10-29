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


## P1 premium web UI product requirements — MaSonKuPik-inspired editor

The Windows VST3 ships a complete offline self-contained HTML/CSS/Canvas GUI through JUCE WebView2. The full DSP remains portable C++ and continues working if GUI/WebView2 cannot start. User-visible P1 needs: rich logarithmic graph, input/output spectrum, exact native response, eight rotating band palettes, click/drag handles, wheel Q or cut slope, double-click add/delete, inspector with numerical frequency/gain/Q controls and shape/slope/channel selectors, status and nearest musical note. Performance intent: no React rendering loop, no network access, at most 25Hz analyzer push from C++, capped DPR, event-driven redraw when idle. No claim of absolute low memory is permitted until WebView2 and native editor are benchmarked on actual Windows hardware. The plugin must remain useful in supported DAWs and preserve all 168 parameter identities and project-state persistence.


### M2.1 Compact Pro-EQ interaction acceptance
For the primary 900×535 editor, neither graph nor X labels may clip. No permanent inspector sidebar. The main EQ graph occupies the width; nodes are directly editable and selecting one shows a compact floating popup containing type/frequency/gain/Q/slope/stereo routing, bypass/delete. Floating popup must not leave visible screen bounds; selecting empty space closes it. Header/bottom chrome are compact and derive their optical materials from ArSonMix. Display only information required for EQ operation (no repetitive status labels or empty placeholder columns). The plugin keeps the same actual C++ transfer curves; a fabricated/retuned JavaScript curve is forbidden.


- **P1 rotary interaction:** Frequency, Gain and Q each have a compact visible tactile rotary control (plus numeric editing) in the floating inspector, with native host automation gesture boundaries. Q and frequency use logarithmic response, gain linear. Cut filters disable irrelevant Gain. Analyzer OFF must stop real-time telemetry and FFT work, not just hide the spectrum.


## M2.2 dual-format deliverables (release gate)

Windows builds must always generate BOTH a Windows x64 `ArSonEQ.vst3` folder and a directly runnable `ArSonEQ.exe`. Both must use the identical native DSP engine, 24-band parameter set, GUI, and preset/state contract. Standalone must expose device selection via JUCE standard audio configuration; it must never claim to process all system playback without an actual input/output route. CI must package both binaries and fail if either output is missing. Passing compilation alone is insufficient: test real audio input/output, save/reopen, GUI window lifecycle and available audio drivers on physical Windows before release.


### M2.3 Professional instrument UI — acceptance driven by VSTHost images

1. **No curve obstruction:** When a band is selected, the edit deck is below (not over) the plot in dedicated layout space; deselection restores full plot height. This is invariant for every frequency/gain and editor size.
2. **Proportions:** Retain 900×535 default outer editor content size with 42px brand/header, min-height flexible log graph, 143px control deck only when selected, and 25px status. No giant right inspector or full-screen form controls.
3. **Professional graph:** Visually dominant dark-purple/charcoal spectrum, restrained 3/6dB horizontal grid, dedicated independent analyzer level rail, response line in warm gold with green positive and muted warm negative fill, per-band colored handles, selected-band isolated response. Host C++ transfer curve owns correctness.
4. **Compact controls:** Four clearly grouped slots for filter type, rotary/numeric Frequency, Gain, Q, with Stereo/L/R/M/S, slope, note, bypass/delete. All remain properly automated VST3 params. Disabled gain on cut/notch avoids false operation.
5. **QA:** Headless visual mock screenshot at 900×535 is review evidence only; actual Windows VSTHost/ArSonMix screenshot, DPI, sound and state regression tests are required before declaring competitive polish.


## M2.4 release criteria — measured Pro-Q 3-class spectrum experience

Source: `docs/PROQ3_GAP_AUDIT_AND_M24_PLAN_20261030.md`. First improve spectrum *fidelity* (including L/R cancellation safety), then its visual density and typography. At the standard 900×535 editor size a selected-band quick panel must occupy <=70px and leave >=390px of unobstructed graph; advanced edit must be on demand. Spectrum options include calibrated Pre/Post, FFT 1024/2048/4096/8192, frequency tilt with 4.5dB/oct default (display only), speed, level range, freeze/peak hold and proper output L/R sample meter. Analyzer should reflect the actual input audio, including fully anti-phase L/R. No synthesized spectra or fake peak display are allowed in production. DSP always runs when editor is closed and no spectrum FFT may run in processBlock. The VST3 and Standalone should be paired Release outputs with matched source SHA and reproducible host benchmarks. Future Dynamic EQ/Natural/Linear Phase are separate unimplemented milestones, not a UI claim.
