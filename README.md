# ArSonEQ

ArSonEQ is an original, open-source, professional parametric equalizer plug-in project. The goal is a fast, transparent, refined workflow comparable in capability to leading commercial EQs, **not** a clone of another product's source code, branding, or artwork.

**Status: M0 foundation, under active development. Not yet a release-quality replacement for Pro-Q 3.**

## Source and architecture

The initial reference is [MaSonKuPik](https://github.com/masarray/masonkupik), another project owned by the maintainer. Its Web Audio/TypeScript EQ interaction model and Butterworth filter settings inform ArSonEQ, but the browser graph is **not** a native VST engine. Native DSP is independently implemented in portable C++17, and JUCE 9 supplies the plug-in wrapper and UI. No compressor, limiter, exciter, or web runtime is bundled into the EQ.

- [Product requirements](docs/PRD.md)
- [Architecture and real-time design](docs/ARD.md)
- [Migration audit and provenance](docs/MIGRATION.md)
- [Implementation milestones and acceptance gates](docs/ROADMAP.md)

## Build direction

- Initial target: Windows x64 VST3; macOS VST3/AU subsequently.
- CMake 3.24+, C++17, JUCE **9.0.4 pinned** (no floating HEAD). An installed local JUCE checkout can be selected with `JUCE_SOURCE_DIR`.
- Headless C++ DSP tests build without downloading JUCE.
- Configure: `cmake -S . -B build -DARSONEQ_BUILD_PLUGIN=OFF -DARSONEQ_BUILD_TESTS=ON`
- Test: `cmake --build build --config Release && ctest --test-dir build --output-on-failure -C Release`
- Plug-in: `cmake -S . -B build-vst -DARSONEQ_BUILD_PLUGIN=ON -DARSONEQ_BUILD_TESTS=ON`; then `cmake --build build-vst --config Release`.
- If no local JUCE source is supplied, the plug-in configuration fetches pinned JUCE from GitHub. A connection is necessary only at configure time, not when running the plug-in.

## License and dependencies

This repository already uses **AGPL-3.0** (see LICENSE). JUCE 9 modules are offered under AGPLv3 or separate commercial terms. This project uses the AGPL path for the open-source edition. **A commercial JUCE licence may be required** for a future proprietary/closed-source edition. Steinberg VST 3 SDK 3.8+ is MIT-licensed; trademark requirements are separate. Verify all third-party dependencies and source provenance before redistribution.

FabFilter and Pro-Q are trademarks of their respective owner, which is not affiliated with ArSonEQ. There is no FabFilter intellectual property or GUI artwork in this repository.


## Native DSP / analyzer milestone

The M1.5 upgrade uses preallocated dual-path crossfades for discrete filter topology, route and enabled changes. Continuous gain/frequency/Q movements use coefficient smoothing. A zero-latency minimum-phase DSP path remains the only implemented phase mode. A single-producer/single-consumer telemetry queue feeds two 4,096-point FFTs (input and output) on the editor thread, without FFT execution in the audio callback. UI transfer curves reflect L/R/M/S routing via a matrix calculation and are cached until parameters or geometry change.

New headless checks include dynamic transitions, algebraic-vs-measured response, heap-allocation checks in DSP processing, and SPSC thread safety. This is **not** certification of click-free behavior on every host or of low-CPU targets: host testing, CPU profiles, pluginval and independent DAW testing remain necessary before release.


## M2 Windows web GUI (MaSonKuPik-inspired)

On Windows, the editor prefers a **premium dark HTML/CSS/Canvas 2D UI** hosted by JUCE 9 WebView2. Assets are embedded into the VST3; no installer web server, external network, React runtime, or npm package install. WebView2 must be available on the user's machine. If unsupported, the native JUCE editor is retained. The DSP continues to run independently of either editor.

Control map: double-click empty canvas to create a bell filter, double-click an existing node to remove; drag for frequency/gain; wheel over a node for Q or cut slope; click for inspector; route filters to Stereo/L/R/Mid/Side; edit numeric frequency/gain/Q in the right inspector; Delete removes selected band. Native APVTS parameter IDs are not changed. The response curve is computed in the C++ DSP, and a bounded 25Hz FFT stream from the editor's native queue drives the input/output spectrum behind the curve.

Source `WebUI/` is deliberately tiny and zero-dependency. Run `node --check WebUI/app.js && node WebUI/smoke.test.cjs` for an offline bridge wiring smoke check. This is NOT a substitute for actual DAW/WebView2 UI testing, host recall validation or CPU/memory profiling.

### Windows fallback

Set environment variable `ARSONEQ_USE_NATIVE_GUI=1` before launching your DAW to force the original JUCE native editor if WebView2 is unsupported or fails in the host. Restart the host to make it take effect. The embedded JS is not an audio dependency; DSP processing remains available without any GUI.


## M2.1 — Compact premium editor

Based on user-provided screenshots comparing Pro-Q 3, ArSonMix and ArSonEQ, the first WebView2 UI was redesigned around a **full-width EQ graph**. Initial WebView2 editor size is **900 × 535** (host title bar excluded), resizable from 760 × 455. Selected nodes open a small movable-by-anchor floating filter popup rather than occupying a permanent empty 294px sidebar. The top/bottom rails are compact, frequencies and dB labels remain visible in the graph, and the active-band list is available from the BANDS button. The materials follow ArSonMix's dark glass workspace / aluminum control / turquoise status language without copying third-party graphical assets. A no-network, no-React JavaScript/Canvas editor is still bundled into the VST3.

Controls: single-click a node to select and open floating controls; click empty canvas to dismiss; double-click to add/delete; drag Hz/dB; wheel to change Q/slope; click BANDS for other active bands, ? for concise help. The existing native fallback and DAW automation parameters are preserved. Native host visual QA, memory/CPU data and pluginval remain necessary before calling the GUI production-ready.


### Compact editor controls (M2.1)
The primary 900×535 Windows WebView2 GUI uses a single edge-to-edge frequency graph and a contextual floating inspector. Click a node to open filter type, frequency/gain/Q rotary and numeric edits, slope, stereo/Mid/Side route, bypass and delete; close the inspector for a fully unobstructed graph. Rotate a control by dragging vertically or use precise numeric inputs. The ANALYZER switch disables the native audio telemetry and UI FFT work when OFF. For debugging fallback: launch the DAW with `ARSONEQ_USE_NATIVE_GUI=1`.


## M2.2 — TWO Windows deliverables: VST3 + Standalone EXE

The Windows Release job **builds both** `ArSonEQ_VST3` and `ArSonEQ_Standalone` from the same JUCE AudioProcessor, 24-band C++ DSP, WebView2 GUI and 168 stable host parameter IDs. These are different binary formats, not two GUIs or a separate browser website.

- **VST3**: `ArSonEQ.vst3` bundle; copy its complete folder to `C:\Program Files\Common Files\VST3\`; restart/rescan ArSonMix, REAPER, or another VST3 host.
- **Standalone EXE**: `ArSonEQ.exe`; launch directly on Windows x64. It does **not** load other VSTs; it is a standalone EQ audio application. Select the desired input/output audio devices using the standard JUCE standalone audio settings. For processing a microphone or external audio feed, configure a real input source and output monitor; simply starting the EXE does not route system-wide playback through the EQ.
- **Both**: use the same offline-embedded WebView2 interface and require Microsoft Edge WebView2 Runtime. To force the simpler native fallback UI, set `ARSONEQ_USE_NATIVE_GUI=1` in the environment *before* launching the EXE or DAW.

A combined **Windows x64 engineering test ZIP** contains `VST3/`, `Standalone/`, `BUILD.txt` and `README_WINDOWS_TEST.txt`. Both formats compile in **one shared CI job** (pinned JUCE and WebView2 SDK, one CMake configure and native DSP tests). A build PASS does not prove soundcard support or host runtime behavior: physical ASIO/WASAPI audio checks, DAW recall, pluginval, GUI screenshot and CPU profiling remain separate release gates.


## M2.3 GUI fix: unobstructed equalizer graph

Based on real VSTHost side-by-side screenshots, the EQ's old floating editor was found to cover critical frequency response data. The band editor is now a compact, dedicated *bottom control deck*, physically outside the graph DOM. It cannot overlay the response curve at any band frequency/level. Selecting empty plot dismisses the controls and restores full graph height; selecting a filter displays its type/rotaries/numeric fields/slope/route/bypass. The plot features a warm-gold native EQ response with green boost shading, individual colored nodes, separate analyzer rail and automatically expanded dB range when needed. Both packaged formats (VST3 and native Standalone) use exactly the same assets; no C++ DSP/host parameter contract changed.

The Linux fast CI job also generates a deterministic **browser screenshot** at 900×535 with a synthetic host fixture for visual layout inspection; this preview fixture is *not embedded* into end-user builds and must not be confused with the actual C++ audio response. Real VSTHost and ArSonMix comparison/DAW validation remains necessary.


**M2.3.1 visual audit:** A CI-created 900×535 browser screenshot is reviewed for panel occlusion, axis collisions and control sizing. When dragging a not-yet-selected node, the dock now opens on *pointer release* to avoid graph-height changes during audio parameter gestures; double-click deletion retains a small first-hit tolerance. Both compiled formats use this same GUI. The browser preview's example curves are synthesized for **visual QA only**, not audio engine output.


## Competitive gap audit (2026-10-30)

After a user-provided side-by-side screenshot comparison of ArSonEQ Standalone and FabFilter Pro-Q 3 in VSTHost, the latest engineering priorities are documented in [the Pro-Q 3 gap audit and M2.4 milestone plan](docs/PROQ3_GAP_AUDIT_AND_M24_PLAN_20261030.md). The important uncovered issue is not a superficial gradient: ArSonEQ's analyzer currently averages stereo samples before FFT, losing fully anti-phase stereo signals, and maps nearest FFT bins to a coarse log spectrum without Pro-Q-style speed/tilt/resolution options. The planned fix preserves actual stereo spectral energy, correct measurement units and real-time safety. GUI density and VST3/Standalone quality gates follow this correctness milestone. Do not interpret the published M2.3 engineering builds as equal in sound/features/performance to Pro-Q 3.


## M2.4 — native stereo spectrum and professional compact graph (engineering candidate)

The user-reportable Pro-Q 3 comparison motivated replacing the coarse mono spectrum (which cancelled antiphase L/R) with **real pre/post L/R PCM telemetry**, independent FFTs and power-domain stereo aggregation. GUI-side Hann FFT is 1024–8192 selectable, 384 logarithmic points with time-based smoothing; FFT tilt defaults to 4.5 dB/oct and affects the **visualization only**, not audio. Pre/Post/Freeze and display range are selectable; right meter shows sample peaks (not true peaks). A 70px selected-band quick strip keeps at least 390px graph height at 900×535; click MORE for full controls. No new VST3 host parameters, no audio callback allocation/FFT, no online UI libraries.

**Release status: engineering candidate.** Native/GUI automatic tests and a dual-format Windows build are prerequisites; real hardware and VSTHost/ArSonMix testing remain necessary before declaring Pro-Q 3 feature parity.


## M2.5 — floating band inspector + professional editing (engineering candidate)

M2.4's bottom flex dock caused a *real* click/release viewport jump. M2.5 relocates the band inspector **into the graph as an absolutely positioned overlay**, so clicking, closing and opening MORE cannot modify the graph dimensions. The card is placed relative to the selected node while preserving pointer coordinates. Ctrl/Meta-click selects multiple bands, group drag moves frequencies in log space with coherent relative frequency and dB offsets, Shift-drag gives precision, Ctrl+Z/Ctrl+Y and toolbar arrows provide bounded 64-level undo/redo of **local edits**, and right click opens actionable EQ band controls. Group movements use bounded native bridge transactions and properly paired host gestures. Native FFT and DSP audio path unchanged. NOTE: DAW global undo and full Pro-Q 3 features are separate future milestones, as is real VSTHost/ArSonMix hardware QA.
