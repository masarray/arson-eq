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
