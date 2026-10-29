# ARD — ArSonEQ Architecture & Design Rules

Version 0.1 · 2026-10-29 · Status: approved direction, implementation evolving

## Design decisions

**A1 — Native engine, not Web Audio:** Reimplement an independent C++17 DSP core with deterministic biquad coefficients. MaSonKuPik is a functional/UX reference, not a source tree to copy indiscriminately.

**A2 — JUCE 9.0.4 for VST3 and native editor:** Use JUCE only for plug-in host integration, parameters, state and rendering. DSP owns no JUCE types. Pin external dependency version. The repo and the JUCE AGPL distribution path are compatible; commercial distribution requires a separate licence decision.

**A3 — 24 fixed band slots, no dynamic audio allocations:** Each slot has type, frequency, gain, Q, cut slope, channel mode and enabled state. Keep parameter IDs stable even after a slot is removed in UI. Fixed max 4 stages for P0 cut filters.

**A4 — Hard isolation of audio and UI:** Audio callback does no malloc/free, locks, network IO, console IO, GPU interaction, or file access. Parameter snapshots are read as atomics, and coefficients update only on changed values. UI can cache graph responses and throttle analyzer repainting independently.

**A5 — Stereo M/S routing:** For each band transform L/R to M=(L+R)/2, S=(L-R)/2 only if that band's placement needs M/S; apply independent filter state to the selected route; reconstruct L=M+S, R=M-S. Never apply per-band M/S as a global widening effect. Mono input uses mono processing and disables meaningless Left/Right/Side routes as necessary.

**A6 — Separate transfer functions from UI graphs:** Expose analytical magnitude response from the exact DSP coefficient generator. UI graph must use actual sample rate and its current processing mode, not a fabricated 48kHz Web Audio offline context.

**A7 — Explicit progression of phase modes:** P0 direct causal minimum-phase biquad, zero algorithmic delay. P2 separate measured natural-phase alternative and latency-accounted linear-phase FIR. Do not claim a direct analog-copy of FabFilter algorithms.

## Architecture

```text
DAW host
  ├── VST3 wrapper (JUCE): buses, latency, automation, state, host lifetime
  ├── ParameterAdapter: stable IDs, normalized ranges, bounded snapshots
  ├── AudioProcessor (real-time): input → 24 fixed-band EQ → output
  │      ├── biquad coefficient generator (RBJ topology + Butterworth stages)
  │      ├── per-band L/R/M/S state, sanitized numerical limits
  │      └── smoothing / topology transition policy
  └── Editor (message thread): graph, nodes, inspector, cached response
         └── optional RT-safe metering/FFT telemetry queue (future)
```

Portable DSP must be independently tested with a simple C++ test harness. It must not include Web Audio, JUCE, UI, JSON, or licensing code.

## Signal flow / control

1. Host writes normalized parameter values into APVTS atomics. On each block, processor reads a coherent-enough bounded snapshot per slot; each changed slot updates its target coefficients. A later revision may use immutable whole-bank snapshots for simultaneous multi-parameter changes.
2. Each sample flows through enabled slots in stable numerical order, with independent state for L/R or M/S. All stereo transforms are per slot; no summing of nonidentical latency paths.
3. Gain in dB converts to amplitude; cut filters are Butterworth cascades using the MaSonKuPik Q-stage array: 12 [0.70710678], 24 [0.5411961,1.30656296], 36 [0.51763809,0.70710678,1.93185165], 48 [0.50979558,0.60134489,0.89997622,2.56291545].
4. For bell/shelf/notch calculate normalized biquad from RBJ Audio EQ Cookbook. Frequency/Q are bounded; every intermediate is finite; target coefficients computed at configuration changes, **never per sample**.
5. Retune uses interpolation or a fully verified dual-pipeline crossfade to avoid zippering and instability. Shape/slope change needs separate discontinuity tests. A simple block-step rebuild is **not** a release-quality solution.
6. Bypass is unity and preserves host state; transitions require crossfade/ramps, and full bypass must neither alter latency nor create clicks.
7. On sample-rate changes, update all coefficients and reset/transition filter state predictably; on silence avoid denormal stalls.

## Parameter contract

For slot N, IDs are immutable `bNN_enabled`, `bNN_type`, `bNN_freq`, `bNN_gain`, `bNN_q`, `bNN_slope`, `bNN_route`. Type order: Bell, LowShelf, HighShelf, LowCut, HighCut, Notch. Route: Stereo, Left, Right, Mid, Side. Slope: 12,24,36,48. Safe defaults: disabled, bell, 1000 Hz, 0 dB, Q=1, slope=12, Stereo. Numbering and enum order are persistence APIs; do not reorder.

Band ranges: 20 Hz–20 kHz (clamp to 45% SR), -24 to +24 dB, Q 0.1–24; visual options may expose more restricted zoom. Actual cutoff and response are sample-rate limited.

Schema migration: a versioned host state tree will preserve current slot identity when changing GUI, FFT or dynamic features. Any later incompatible addition must be additive or migratable, never rename existing host parameter IDs.

## Lifecycle and threading

- Construction: allocate fixed states once. Prepare: set sample rate, initialize states. Process: no allocation and no throwing.
- UI: repaints only on observed change; optional analyzer target 20–30 FPS. No unbounded `repaint()` loop or heavyweight effects; when editor closes, analysis stops.
- For P1 FFT, use preallocated SPSC sample/tap queue and worker/UI drain; drop visual frames rather than block the audio callback. Ensure lock-free queue semantics and ownership with tests.
- Offline render and host-initiated parameter automation must use the same DSP equations; no dependence on frame rate or wall clock.

## Precision and verification

For reference response test: compare analytical `H(e^{jw})` with measured sine sweep at 44.1/48/96/192 kHz and check normalized Biquad stability; verify 24 stages × two channels with random input finite. Test M/S symmetry, mono, disabled-band unity, impulse tail, repeatability, gain at band center, coefficient edge cases, and rapid-automation stress. Track max absolute error by mode.

Before shipping: run Steinberg validator, JUCE pluginval, REAPER, and a second DAW; run Windows Defender scan on artifacts. Include licenses in binary distribution.

## Design risk register

| Risk | Mitigation |
|---|---|
| Browser DSP copy is not VST-ready | Port feature contract and verified math only |
| Automation zipper/crackle | Per-sample coefficient smoothing and topology-specific crossfade QA |
| GUI hover/FFT slowing real-time audio | Cached CPU draw, no GPU dependency for sound, throttled analyzer |
| JUCE AGPL/commercial licensing | Publish source under AGPL, review commercial edition separately |
| Nonzero-phase behavior misrepresented | Name only implemented modes, measure before enabling |
| Parallel thread drift | PRD/ARD and CHANGELOG are canonical; small PRs with tests, no speculative rewrites |

## Stage gates

M0: repo structure, standalone DSP, unit tests, CMake; M1: VST3 adapter, host params, graph interaction and save state; M2: audited transitions, spectrum analyzer, host validation; M3: dynamic EQ; M4: phase variants/surround/other formats. Consult [ROADMAP.md](ROADMAP.md) for binary acceptance. Avoid costly CI on unneeded toolchains until the host wrapper exists.


## M1.5 / early M2 engineering reconciliation (2026-10-29)

After auditing the initial native implementation, naive coefficient interpolation was replaced for **discrete topology changes**: enabled/bypass, filter shape, route and number of stages now use two fixed preallocated processing paths, a 10ms smoothstep crossfade and bounded last-request-wins pending transitions. This avoids changing the same path's topology/state instantly or restarting an unfinished transition. Topology crossfades cost extra CPU **only while a change is in progress**, and never introduce latency. Frequency/Q/gain updates retain stable-coefficient interpolation over 12ms on the active path. All work is performed with fixed storage and no heap operations on the callback.

**Limitations:** exact pop/click-free behavior still requires listening and more stringent amplitude-difference stress tests with external hosts; high-rate topology automation can defer the most recent request by the remaining crossfade duration. For 24 automated bands all changing simultaneously, two-path execution increases CPU temporarily. This must be benchmarked at small buffers; it is not yet a measured CPU guarantee.

The analyzer uses a fixed 16,384-frame SPSC ring of {pre-mono, post-mono} samples, with release/acquire indexed ownership. The audio thread only pushes when an editor is open; on overflow telemetry frames are dropped rather than delaying audio. The editor consumes on a 30Hz timer, computes Hann-windowed 4,096-point pre/post FFTs on the message thread, and draws both behind the EQ curve. FFT size limits low-frequency resolution (~11.7Hz per bin at 48kHz); future analyzer iterations can use lower-frequency multiresolution windows if CPU budget permits. The audio path does **not** invoke the FFT.

The graph caches a 2x2 complex transfer matrix for the full cascade, including L/R/M/S and crossfeed. UI plots diagonal L<-L and R<-R responses and a subdued crossfeed line, recomputing only when EQ parameters, sample rate or editor geometry changes. This is a more accurate representation than the initial stereo-only approximate graph. It does not claim to equal a measured music-spectrum FFT or FabFilter's proprietary phase algorithms.

The fixed APVTS parameter IDs remain unchanged. The project now tests single-thread finite/RT stress, measured sine gain against the analytic curve, switch continuity, no heap allocations during process, ring buffer overflow and concurrent producer-consumer integrity.

CI efficiency: one small Linux headless job and one Windows VST3 job that also runs the headless tests; avoid a duplicate Windows headless matrix. CI begins only after a coherent multi-file milestone commit, not after each individual edit.


## M2 GUI Architecture — native DSP + bundled JUCE WebView2 shell

**Decision**: User explicitly requested porting MaSonKuPik EQ presentation, with an optimized web GUI. On Windows the editor uses JUCE 9.0.4 WebBrowserComponent with the WebView2 backend. Assets are three embedded local-only files: `WebUI/index.html`, `style.css`, `app.js`. There are **no npm runtime dependencies**, React, Tailwind, live web requests, localhost server, remote fonts, Web Audio DSP, or online CDNs. Windows native JUCE editor remains as a fallback if WebView2 options are unsupported; the DSP and VST3 parameters are unchanged.

**Bridge and ownership**:
- C++ reads the authoritative 168 APVTS host parameters (24 slots x 7) on the editor message thread at 25Hz. Browser can only submit allow-listed `enabled/type/route/freq/gain/q/slope` commands via one `eq-command` listener. Numeric values and indices are bounded. Gestures (`begin` / `edit` / `end`) use `beginChangeGesture`, `setValueNotifyingHost`, `endChangeGesture` to preserve host automation.
- UI response curves are computed from the **same native DSP transfer matrix**, including stereo L/R and M/S; transmitted only on changed bands, sample rate, or selected band (256 log-spaced points). No offline Web Audio simulation or JS filter coefficients.
- Pre/post audio telemetry enters existing fixed-size SPSC queue; the message thread performs FFT and sends 224 log bins via a bounded 25Hz event. The WebView does **not** call native audio processing, use Web Audio, or access the audio callback.
- Canvas 2D retains MaSonKuPik interaction logic (double-click to add/remove, pointer drag, wheel Q/slope, notes and inspector), but uses a new self-contained DOM/Canvas implementation to avoid shipping its entire mastering app and dependency graph. Rendering is event-driven: only repaint after changed state or spectrum and maintain a 2x maximum device pixel ratio.
- Embedded resource provider only serves exact static resource paths; page navigation is limited to local resource root; CSP denies all external origins. Avoid security expansion by never loading third-party pages in a native-integrated WebView.
- WebView2 is a Chromium runtime and may use more baseline memory than JUCE's native editor. A tiny JS bundle is not the same as a zero-cost WebView2 process. Measure resident memory, first-open time, CPU and multi-instance behavior on Windows before calling the UI "ultra-lightweight".

**CI**: Javascript syntax + no-dependency mock-DOM smoke on Linux; Windows build checks pinned WebView2 SDK headers/loader and embeds the assets into the VST3 artifact. Use a cached pinned NuGet SDK and retain native DSP test suites. Real DAW/WebView2 runtime inspection remains a gate.

**Compatibility**: Never change existing host parameter IDs, plug-in ID, manufacturer ID, plugin binary format, state schema or audio process methods just to support GUI redesign. Native editor must remain available as a recovery path. If runtime initialization fails after CMake-supported capability probe, report failure and improve fallback rather than claiming success.
