# ArSonEQ vs Pro-Q 3 — engineering gap audit and milestone contract
**Date:** 2026-10-30 · **Scope:** exact requested Pro-Q 3 benchmark, not Pro-Q 4.

## Evidence and fairness
- User screenshots: ArSonEQ standalone at **902×563 px** (2 active bell bands; thin descending pre/post spectrum, permanently open bottom edit dock); Pro-Q 3 in VSTHost at **941×759 px whole host image**, containing a roughly 900px-wide plugin editor (1 active bell band; full-height gray filled analyzer, right-hand output meter). **These are not matched test signals/parameters**, so do not infer audio fidelity or CPU superiority from the pictures.
- Official FabFilter Pro-Q 3 manual, [PDF](https://www.fabfilter.com/downloads/pdf/help/ffproq3-manual.pdf), specifically printed pp. 6, 9–10, 13–14, 21–26, 28–30; official launch [article](https://www.fabfilter.com/news/1543478400/fabfilter-releases-fabfilter-pro-q-3-equalizer-plug-in-with-dynamic-eq).
- Direct ArSonEQ source audit on branch `feat/initial-vst3-eq-architecture`, at `Source/WebEditor.cpp`, `Source/DSP/AnalyzerQueue.h`, `Source/DSP/ParametricEQ.h`, `WebUI/{app.js,index.html,style.css}`. Reference stage is M2.3, not a legacy native editor screenshot.
- Do not copy FabFilter artwork, code, iconography, trademarks or presets. The target is comparable usability, scientific behavior, sonic performance and professional finish via original code and ArSonMix design language.

## Root-cause matrix (not merely aesthetic taste)
| Priority | Observed ArSonEQ implementation | Reference behavior | Engineering problem and target |
|---|---|---|---|
| **P0 Stereo spectrum correctness** | `PluginProcessor.cpp` sends `0.5*(L+R)` as a single mono pre/post stream. An L=-R stereo signal cancels in that telemetry and can falsely appear silent. | Stereo analyzer must reflect energy present on each physical channel, including Side-heavy sources. | Change fixed SPSC `MeterFrame` to pre/post L+R samples; run channel FFTs then combine **linear spectral power** `0.5*(|L|²+|R|²)` when showing stereo, never mix the samples before FFT. Add anti-phase, left-only, right-only and Mid/Side routing tests. |
| **P0 Analyzer** | `WebEditor.cpp` computes one 4096-point pre/post FFT on the message thread, nearest FFT bin by rounded Hz for each of 224 logarithmic UI points. It reuses temporal smoother state indexed by `k` and may update the same bin more than once per frame. | Pro-Q 3 configurable resolution 1024/2048/4096/8192, speed, 60/90/120dB range, default 4.5dB/oct tilt, freeze. | Replace nearest-bin/duplicate smoothing with power-domain log-frequency aggregation, calibrated/window-aware magnitude and *one* time-domain smoothing pass per display point; tilt is visualization ONLY. CPU on message/background UI thread, never audio callback. |
| **P0 Spectrum look** | Thin angular, step-like trace with under-emphasized pre/post fill and falling treble in screenshot. | Soft translucent gray spectrum mass behind high-contrast curve with readable peaks. | Fill with controlled alpha + fine outline, frequency-dependent visualization smoothing that **preserves narrow peaks**, stable attack/release, independent pre/post toggles and freeze. No fake spectra. |
| **P0 Viewport** | Selected bottom control deck ~148px, fixed header 42px and footer 25px at default 535px editor height; chart height ~320px, only ~60% of total editor. | Graph dominates visual field, controls consume little of it or are contextual. | Introduce **quick edit strip 60–70px** on normal band selection: graph >=~75% at 900×535; advanced controls expand only on demand. Never let edit UI occlude selected curve/node. |
| **P0 UI coherence** | Narrow knobs, excessive always-visible caption/metadata, fonts/decoration unlike ARSonMix controls. | Strong hierarchy, minimal chrome, precise local value labels and hardware-grade controls. | One shared ArSonMix material tokens set (carbon/black-violet graph + metallic chrome + cyan states + band colors), restrained glow, consistent baseline/grid typographic grid, fine value readouts, accessible contrast. |
| **P1 Output metering** | Analyzer queue transmits two mono sums, not independently metered post L/R peaks; no real output level meter. | Right vertical real-time output meters plus analyzer dB labels. | Separate lock-free/sample-peak per-channel metering with held peaks and release and dBFS scale. True-peak must not be claimed unless oversampled/tested. |
| **P1 EQ edit workflow** | Add/remove via double click; single-band pointer, Q wheel, numeric fields, no selection group or band-solo; frequency handle can be clamped by graph range. | Single/multi-band workflows, exact text entry, precision key modifiers, solo, menu, spectrum grab. | Direct edit with Shift/Alt precision, Ctrl multiselect and safe group edits, context menu, hover readout, band solo and undo/redo. Retain stable APVTS IDs; follow host gesture lifecycle. |
| **P2 EQ processing** | Native RBJ-inspired static minimum-phase **6** shapes, 24 bands, **12–48dB cut** slopes, frequency **20Hz–20kHz** and gain **±24dB**, independent Stereo/L/R/M/S, zero latency. | Pro-Q 3 specifies **9 filter shapes** (including bandpass/tilt shelf/flat tilt), 6–96dB/oct and brickwall for cuts, 5Hz–30kHz, ±30dB, and three phase processing modes. | Test amplitude **and phase** with swept sine/impulse at extreme Q/sample rate. Do not present standard RBJ as analog-matched/Natural Phase. Add dynamic engine only after detector/CPU/automation benchmark; phase modes require separate proven architecture. |
| **P2 Pro ecosystem** | No per-instance spectra, EQ match, preset browser, output gain/auto-gain, surround/immersive. | Reference/source compare, collision highlight, EQ Match, full output controls, surround support. | Schedule after essential audio/render maturity. Keep VST3 + Standalone built together. |

## ONE significant milestone: M2.4 "Spectrum + UX convergence"

### A. Native calibrated spectral analyzer (single authoritative implementation)
1. Keep current lock-free SPSC architecture; audio producer writes only pre/post **actual PCM samples**, never runs FFT, allocates or waits. Add explicit telemetry suppression when editor is closed/analyzer disabled.
2. Make FFT length configurable **1024, 2048, 4096, 8192**; preallocate maximum 8192/window/ring FFT buffers in editor. High/4096 initial default, max/8192 optional at low frequencies; budget at 48k/96k and multiple instances. Optional optimizations (overlap/window) must be profiler-driven.
3. Window Hann; calibrate a full-scale coherent sine and amplitude normalization including window coherent gain (amplitude) / ENBW as appropriate for the chosen spectral interpretation. Compute linear power/magnitude and never average values expressed in dB incorrectly.
4. Bin frequencies accurately by sample rate. Aggregate FFT bins into ~384–512 logarithmic x bins with frequency-overlap weighting or bounded interpolation. Suppress duplicate nearest-bin stair steps; no invented peaks.
5. Smooth **per plotted bin, once per displayed frame** with elapsed-time-based attack/release; release speed adjustable (fast/medium/slow). Allow optional log-frequency Gaussian smoothing with explicit bandwidth, never heavy enough to erase audible high-Q spectral peaks.
6. Display tilt `tiltDb(f) = tiltDbPerOct × log2(f/1000Hz)`; user options 0 / 3 / 4.5 / 6 dB/oct, default 4.5. This is applied to **visualization, not the audio output**. Defaults: spectrum 90dB, High=4096, Medium speed, tilt 4.5, Post shaded, Pre outlined.
7. Freeze & peak hold, independent Pre/Post toggles, appropriate logarithmic labels; settings stored with standalone or host project without changing the 168 legacy band parameter IDs. WebView receives bounded immutable spectrum snapshots at <=30FPS, no DSP in JS.
8. Extend fixed `MeterFrame` to preL/preR/postL/postR (no audio-thread allocations); for a mono input duplicate the active channel in the telemetry contract. Aggregate spectral energy per channel in linear power; a Side-only/anti-phase source must not disappear. Then implement independent L/R post sample peak meter on C++ audio callback using fixed-size atomic/envelope state. Explicitly label **sample peak**, not true peak. Smooth/hold/release on editor side. Add meter display on far right with dBFS scale.
9. The analyzer must remain signal-faithful. A pleasing aesthetic is a presentation layer, not a synthesized or retouched spectrum.

### B. Premium instrument UI with more available graph
- At 900×535 selected state: top chrome <=42px, bottom strip <=25px, **quick band strip <=70px**; vertical graph >=390px (>~73%). Selection without modifier shows quick controls: band type, Frequency, Gain, Q and route; additional slope/notes/menu/advanced options behind disclosure. Advanced edit can use the existing 148px dock on demand.
- Selected parameter value capsule near the band node is **small, never occludes the node**. Hide after pointer inactivity; no heavy modal.
- Original gain curve drawn above spectra with antialiased line, controlled gradients for positive/negative gain, per-band colors and optional correct individual band response. Maintain independently labeled EQ gain axis and input/output analyzer dBFS/tilt display.
- Right-side output meter occupies dedicated 16–24px; displays peak label and clipped condition (without false true-peak claim). Spectrum graph and meter cannot overlap text labels or each other.
- Build a single shared CSS/design-token source tied to existing ARSonMix brushed-metal language; no bespoke heavy JS rendering package. Canvas 2D is retained until measured profiling demonstrates a need for a WebGL/WebGPU engine.

### C. Objective acceptance gates before VSTHost aesthetic sign-off
**Signal/FFT tests**
- Stereo anti-phase test: with L=+sine and R=-sine at equal amplitude, the displayed stereo spectrum must remain at the same spectral level as identical L/R dual-mono (within calibration tolerance); input L-only and R-only must be visible; swapping channels must not affect stereo composite. Mid-only/Side-only EQ routing must not silently erase the analyzer. The existing `0.5*(L+R)` telemetry is explicitly rejected.
- Inject -6.0dBFS single sine at 50Hz / 100Hz / 1kHz / 10kHz, 44.1/48/96k sample rates. Displayed calibrated response is within ±1.5dB at 1kHz and ±2.5dB at 50Hz when using max resolution (allow finite-window leakage, documented).
- With a fixed pink-noise input and a tilt setting, analyzer mean slope behaves consistently; tilt OFF versus ON changes **visual dB only** and processing PCM output is bit-identical.
- Attack/release values depend on elapsed time, not UI fps: compare update at 15/25/30Hz; freeze retains last frame; no NaN, Infinity or discontinuities on resolution switch.
- Spectral pre/post measured difference at 1kHz with +6dB bell agrees with DSP transfer response within ±1.5dB away from clip/silence using a steady tone. Do not manipulate plot to fake equalization.
**UI / workflow tests**
- Screenshot golden fixtures at editor 900×535 and 1200×700, both deselected/selected; at 100% and 150% Windows DPI in VSTHost/ArSonMix, no clipped graph/grid/knobs/footer or overlapping meters. Quick-strip selected chart height >=390px at 900×535.
- Same real test signal and corresponding band parameters in ArSonEQ and Pro-Q 3 for fair perceptual/visual comparison; record source audio, sample rate and FFT settings. Multiple band states (flat, bell boost, narrow notch, shelves, two-band).
- Graph node drag cannot reflow its coordinates during the drag; automation begin/edit/end one coherent host gesture; save/restore project matches every band. No all-UI JS refresh while analyzer disabled.
**Performance / release**
- No heap alloc, lock or FFT in the native processBlock; verify at buffer 32, 64, 128 and 48/96k sample rates. FFT and aggregation benchmark on UI thread and on multi-instance hosts, with specific machine+OS documented.
- Artifact must contain **both native Windows VST3 and Standalone** from one test-passing commit and SHA manifest. One consolidated Windows CI run after a fast Linux DSP+JS test, not tiny repeated CI cycles.
- PR stays Draft until actual VSTHost + ArSonMix QA, no audible zipper/click, no clipping, validated preset/state and RAM/CPU results.

## Follow-up milestones after M2.4 acceptance
**M2.5 Workflow parity:** per-band type/range/shape right click, group select, precision modifiers, undo/redo, solo band, spectrum grab (faithful and guided; no hallucinated detected notes). Output gain/global bypass/auto-gain with tests.
**M3 Dynamic EQ:** band-limited trigger per eligible band, auto/manual threshold, envelope follower, program-dependent attack/release/soft-knee and signed dynamic range; realtime-safe parameter interpolation, visual ghost/range. Add new host parameters with backward-compatible versioned state.
**M4 Accurate processing:** DSP curve/phase benchmark at multiple Fs/Q, then research analog-matched behavior, Natural Phase alternatives, standalone linear-phase FIR partitioned processing with properly reported latency and phase testing. Never relabel RBJ zero-latency as Natural Phase.
**M5 Reference tools:** multi-instance spectrum exchange without audio-thread lock, external sidechain, EQ Match based on measured reference spectra, surround/immersive only after core is mature.

## Compatibility / numerical risk register
- Spectrum FFT should never be applied to a time-domain average of stereo channels; anti-phase is normal in Mid/Side music and is not zero sound.
- L/R and M/S plots should be derived from the same actual DSP transfer matrix. A stereo *magnitude* display alone is insufficient to represent L/R crossfeed; display both L/R responses when different, and optionally Mid/Side response on demand.
- Filter extensions must not mutate the meaning of existing `type=0..5` or `b00_*` ... `b23_*` host IDs; add *new* types at the end of enum and migrate state/version safely. Separate frequency/width/gain ranges with genuine DSP validation. A 30kHz selection at 44.1kHz cannot be rendered as a 30kHz processed response past Nyquist: define sample-rate-aware behavior instead of hardcoded clamping.
- Pro-Q 3 uses more than ordinary RBJ biquad matching; equal-looking gain curves do not prove equal phase behavior, especially in shelf resonance, Nyquist warping and high slopes.
- "Standalone" must remain a JUCE audio-device application and "VST3" a host plugin. Both depend on one C++ DSP engine and one WebView2 GUI bundle, and neither uses a fake synthesized spectrum for audio validation.

## Decision priorities
1. **Close analyzer fidelity + screen hierarchy gap first**, using a single integrated M2.4—not another round of cosmetic CSS.
2. **Keep DSP correctness and host automation sacred**; analyzer/GUI must not alter audio.
3. **Do not claim feature parity** until the specific gates above have recorded evidence. Use Pro-Q 3 as UX/spectral reference, not code/assets source.
