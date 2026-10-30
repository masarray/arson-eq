# Roadmap and handoff checklist

All contributors and parallel ChatGPT threads must read PRD + ARD + this file **and inspect GitHub HEAD/open PRs** before editing. Avoid overlapping implementations, reversed decisions, expensive CI, or claiming test success without logs.

| Gate | Work | Exit criteria |
|---|---|---|
| M0 — foundation | Native C++17 parametric EQ; 24 fixed slots; core tests; pinned plugin dependency; docs | C++ compile and tests on Linux + Windows |
| M1 — usable VST3 | JUCE VST3, mono/stereo, 24-band host parameters, persistent state, interactive native graph | Windows .vst3 artifact, REAPER smoke, pluginval |
| M1.5 — audio hardening | RT parameter smoothing, bypass/type change transition, Q/frequency edge-case stability, host automation tests | Zero clicks in prescribed stress suite; ASan/UBSan offline; no heap activity on callback |
| M2 — UX | Pre/post real FFT, Q controls, detailed band inspector, undo/redo, A/B, theme polish, resize | 30 FPS max FFT on UI, graph parity with DSP, user workflow tests |
| M3 — Dynamic EQ | Threshold/range, automatic envelope, attack/release, compressed/expanded bands, sidechain | Golden tests, audio listening, CPU budget maintained |
| M4 — phase engine | Measured lower-phase-distortion option, FIR linear phase with correct delay and optional quality | Verified impulse/phase response, delay compensation, musical listening |
| M5 — expansion | Extended slope/surround/macOS/CLAP/EQ match | Platform-specific compatibility suite |

### Non-negotiables

1. Publish no artifact called release-ready until actual tests passed and DAW run evidence is linked.
2. Keep DSP independent from plugin framework; never put allocations, network, locking, heavy logs or render work in the audio callback.
3. Use fixed host IDs; add but never reuse/reorder parameter IDs after public release.
4. Do not copy third-party proprietary GUI artwork, trademarks, presets or algorithms.
5. CI budget: fast headless DSP checks on each PR, heavy platform/plugin builds only on meaningful PR/controlled dispatch; cache dependencies where viable.
6. Make precise claims: **Designed**, **Implemented**, **Compiled**, **Validated in DAW** are different statuses.


### M1.5+Early M2 integrated milestone (2026-10-29)

- **Implemented:** preallocated dual paths for topology changes, 10ms fades, deferred pending topology updates, 12ms continuous parameter smoothing; unchanged 24-slot host parameter contract.
- **Implemented:** actual L/R transfer matrix including M/S crossfeed, cached frequency response UI, 4,096 point dual pre/post FFT and lock-free bounded telemetry queue.
- **Added acceptance tests:** analytic vs measured frequency response, high-frequency automation/topology stress, audio-path allocation tracking, SPSC concurrency and capacity overflow.
- **CI policy:** headless Linux + combined Windows plugin/native tests. Do not rerun unrelated workflows. Note that Windows VST3 compilation is **not** a DAW validation.
- **Next release blockers:** host VST3 validation (pluginval/Steinberg validator), REAPER and a second DAW load/state/automation testing, click/noise golden captures, CPU profiling at 32/64/128 buffers, independent review of sample-rate change lifecycle.
- **Next user features:** coherent band inspector and numeric editing, A/B, undo; dynamic EQ with proper envelope and sidechain only after the real-time foundation is validated.


### M2 — MaSonKuPik UI port / premium compact native-web hybrid

- [x] Implement standalone browser UI bundle (HTML/CSS/Canvas 2D and vanilla JS) with 24-band editing, premium dark appearance, interactive frequency graph and inspector.
- [x] Use native JUCE WebBrowserComponent/WebView2 editor with local-only embedded resources, allow-listed host parameter updates, host gestures and native fallback.
- [x] Drive spectrum and exact filter-response curves from the C++ DSP and existing lock-free ring; no JavaScript DSP or React build tool.
- [x] Add CI javascript syntax and bridge wiring mock-DOM smoke test.
- [ ] Validate host loads web UI rather than native fallback on a real Windows WebView2 runtime; test resizes, multiple instances, keyboard focus, automation, DAW save/restore.
- [ ] Measure first-open latency, idle memory footprint, 25Hz analyzer UI CPU, 32/64/128 audio buffers and performance with many bands.
- [ ] Production GUI enhancements: coherent undo/redo and A/B, spectral analyzer calibration, keyboard navigation, touch ergonomics, peak-hold; then Dynamic EQ engine work.

Do not mark M2 DAW-validated based solely on the Windows compilation or mock-DOM smoke check.


### M2.1 — GUI layout and material corrections
- [x] Remove permanent right inspector panel and oversized empty placeholder; graph takes full width.
- [x] Introduce selection-following floating inspector, bounded above/below nodes, with existing host controls.
- [x] Default Windows WebView2 editor 900×535, resize limits 760×455 to 1360×850; avoid graph clipping.
- [x] Apply ArSonMix metal/glass/cyan semantics with restrained FabFilter-style 0 dB baseline and editable nodes.
- [x] Add layout smoke assertions for disappearing inspector, popover mutual exclusion, and no permanent sidebar.
- [ ] Confirm visual geometry in a real VST3 host at 100% and 150% Windows scaling; capture screenshot review and idle WebView2 memory/CPU.


- [x] Floating native-web inspector now has tactile frequency/gain/Q rotary controls with host gesture lifecycle.
- [x] Analyzer OFF atomically gates audio telemetry and skips editor FFT.
- [x] Local mock-bridge UI smoke passed for rotary, full-canvas inspector, menu and analyzer gating; Windows VST3 final build remains subject to CI and real DAW review.


### M2.2: Native Windows dual-format output

- [x] CMake JUCE target graph declares VST3 **and** Standalone from one shared processor + editor.
- [x] One Windows Release workflow builds both, runs all native DSP tests, verifies both artifacts, packages the EXE and VST3 together with commit manifest and installation README.
- [ ] Confirm CI produces both and the exact ZIP matches the commit.
- [ ] Run Standalone on real Windows input/output devices; test WASAPI/ASIO audio settings, microphone processing, sample-rate changes, persistent states and GUI windows.
- [ ] Run VST3 inside ArSonMix/REAPER and pluginval; test automation, multiple instances and DAW restore.


### M2.3: Full graph, non-obstructing control deck (VSTHost evidence)
- [x] Remove absolute node-following inspector permanently; structural flex layout prevents the band editor from covering a curve or node.
- [x] Replace heavy navy dashboard look with ArSonMix metallic instrument details and Pro-EQ-inspired charcoal plot, warm gold curve, green boost fill.
- [x] Keep default 900×535, independent right dB rail and adaptive ±12 to ±24dB axis; logarithmic X mapping accounts for right rail.
- [x] Maintain native host gesture automation, 24-band parameter identity, FFT visibility gate, both VST3 + Standalone targets.
- [x] Local JS smoke verifies DOM sibling, no-occlusion layout, gestures, band list and analyzer switch.
- [ ] Linux Chromium 900×535 screenshot published and reviewed; verify actual paint at reference size.
- [ ] Windows VST3 + Standalone Release build both formats; verify no regression.
- [ ] Inspect actual VSTHost and ArSonMix screenshots at 100% and 150% DPI and measure WebView2 memory/CPU before production merge.

- [x] Real 900x535 headless Chromium screenshot inspected: graph unoccluded and instrument deck visible, identified +12/+9 dB label collision.
- [x] Correct overlapping extreme dB ticks and right-side 20kHz label clearance; modestly increase knob target diameter and bounded deck height.
- [x] Defer first dock opening until pointer release to prevent canvas-height induced drag coordinate jumps; regression smoke passed.
- [ ] Inspect follow-up visual screenshot and VSTHost Windows behavior after the GUI QA polish build.


## Current strategic priority after real 2026-10-30 Pro-Q 3 comparison

**M2.4 (P0, one consolidated milestone):** real stereo-correct analyzer telemetry (avoid L+R anti-phase cancellation), 1024–8192 FFT, calibrated magnitude, power-domain log mapping, bounded per-point time smoothing, 4.5dB/oct display tilt, pre/post/freeze, output sample-peak meter, 60–70px quick edit dock leaving >=390px chart at 900×535, refined original ArSonMix/Pro-EQ visual design. Native engine/host IDs unchanged. **Evidence gates:** frequency/calibration/anti-phase/headless UI tests, VSTHost and ArSonMix captures at 100/150% DPI, both Windows formats, CPU/RAM baseline.
**M2.5 (P1):** precision UX and workflow: group selection, modifier drag, band solo, context menus, undo/redo, output gain/bypass and trustworthy Spectrum Grab.
**M3 (P2):** dynamic bell/shelf per-band trigger, auto/manual threshold, envelope, ±dynamic range, native automation and no-click processing.
**M4 (P2):** complete DSP filter shape/slope audit; *then* original validated analog-matched response, Natural Phase alternative, Linear Phase mode with reported latency.
**M5 (P3):** reference matching, external/instance spectra, EQ Match, surround and full ecosystem.

Do not declare commercial feature parity until these independent criteria are met. Pro-Q 3's 9 shapes, range and modes must not be represented by renaming the existing 6 RBJ filters. See `docs/PROQ3_GAP_AUDIT_AND_M24_PLAN_20261030.md`.


### M2.4 implementation checkpoint (2026-10-30)
- [x] Four-channel PCM telemetry replaces mono `(L+R)*0.5`; dual-mono fallback for mono.
- [x] Native UI-side FFT orders 10–13 (1024 to 8192), Hann window and normalized stereo spectral energy.
- [x] 384 logarithmic interpolated power bins; per-point elapsed-time attack/release; default 4.5 dB/oct visual-only tilt.
- [x] User-selectable FFT speed/range/tilt, Pre/Post/Freeze, two-channel output sample peak visualization.
- [x] Quick deck compact <=70px with advanced disclosure; 900×535 reference chart >=390px.
- [x] DSP SPSC/anti-phase math tests and JS analyzer/pointer smoke tests staged.
- [ ] Confirm Windows VST3 + Standalone compiled from same SHA; real GUI screenshots and instrument QA.
- [ ] Native full signal calibration 50Hz/1k/10k for 44.1/48/96k and CPU/RAM/multiple instances; remains required.
- [ ] Preserve user analyzer settings across host reopen if needed; currently editor session settings.

- [x] Repair native JUCE fallback's stereo analyzer field integration: four FFT passes, power combination after FFT.
- [x] Precompute Hann window/coherent normalization once when selecting FFT size; remove repetitive per-channel trig in GUI update loop.


### M2.5 integrated implementation candidate — screenshot-driven UX regression fix
- [x] Fix selection/MORE graph coordinate-jump root cause by moving band inspector into `graphWrap` as an absolute floating composite layer.
- [x] Place card according to selected node and available vertical space without moving/resizing canvas.
- [x] Ctrl/Meta multiselect (or Ctrl+A) and proportional-frequency / additive-gain group drag.
- [x] Shift precision drag (~17% pointer multiplier).
- [x] Bounded grouped `gestureMany` + `bulkEdit` commands through the native JUCE host-parameter bridge.
- [x] Local 64-history undo/redo across single/group parameter edits with Ctrl+Z/Y toolbar buttons.
- [x] Original lightweight context menu: edit/bypass/remove/add/select all.
- [x] UI fake-host regression for card geometry, group gestures, history, right-click, analyzer.
- [ ] Confirm CI Linux Chromium visual and Windows dual target VST3+Standalone build from same commit.
- [ ] Real Windows VSTHost/ArSonMix: graph height invariant on click/release, drag at 20Hz/20kHz extremes, MORE at 100%/150% DPI, group gestures, undo/redo; profile WebView2 CPU.
- [ ] Later: band solo audition, multi-band numeric editing, fully host-integrated undo semantics, spectrum grab and output auto-gain.

 
## M2–M5 milestone ledger (current branch, 2026-10-30)
| Milestone | Working scope | State / remaining gate |
|---|---|---|
| M2.4 | Accurate stereo spectrum, 384 log bins, FFT controls | Implemented, Windows/standalone CI pass, needs real signal calibration/host profiling |
| M2.5 | Non-resizing floating card, group edits, 64 undo/redo, right click | Implemented; user confirmed stable floating layout |
| M3 | 9 filter shapes, real JUCE preset picker (8 factory+8 user), native manual-threshold dynamic bell/shelf, 48 new APVTS params | Integrated candidate; needs fast tests, dual Windows binary, VSTHost audio/preset QA |
| M4 | Scientifically verified analog-matched transfer, professional 6–96dB/brickwall slopes, Natural Phase, Linear Phase with latency contract | NOT STARTED as engine features; research/measurement stage only |
| M5 | Real EQ Match, spectrum grab/reference/collision, band solo, surround, polished preset portability, release sign-off | NOT STARTED, sequenced after M4 DSP validation |

### M3 acceptance/test checklist
- [ ] Linux core: old shape IDs, new pass/tilt response, dynamic threshold power response, enabled/disabled passthrough.
- [ ] WebUI: real host-preset event commands, factory/user list populated, filter chips, native dynamic range/threshold commands, M2.5 click/drag no reflow.
- [ ] Windows: paired VST3+Standalone same commit, 3 native tests pass, correctly packaged EXE and .vst3 with manifest.
- [ ] Real hardware: output audible with/without dynamics, reasonable CPU at 24 bands, scene restart and preset save/load, stereo/M/S, 44.1/48/96k at 32/64/128 buffers, 100%/150% DPI.
- [ ] Advanced dynamic auto threshold/phase/brickwall/EQ match remain separate explicitly unimplemented tasks.


### M3.1 after screenshot review — advanced UI + real detector visualization
- [x] Fix dynamic detector stale frequency/Q during automation via topology-safe dual-path transition.
- [x] Per-band actual RMS trigger activity published once per processing block, 24 bounded relaxed atomics.
- [x] Live DSP-derived **effective transfer** L/R curve + center dynamic gain, max 20Hz updates on change, independently of analyzer toggle.
- [x] Visualize dynamic response against static curve, moving gain indicator and live value/threshold progress meter.
- [x] Replace Windows white native Type EQ menu with 9-choice dark app popup; replace native preset chevron with text-only preset button and dark menu.
- [x] Advanced inspector <=484×142 pixels, quick <=484×70; no reflow of EQ canvas.
- [ ] Paired Linux/Windows Release CI and inspect 900×535 advanced popup screenshot.
- [ ] Native VSTHost/ArSonMix audition, manual/auto threshold gap, audio correspondence and CPU profile.
- [ ] **Still incomplete for Pro-Q 3 / iZotope parity:** auto threshold, frequency-dependent ballistics, fully adjustable attack/release, external sidechain, advanced filter and phase modes, true-Q optimized dynamic detector.
