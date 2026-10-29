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
